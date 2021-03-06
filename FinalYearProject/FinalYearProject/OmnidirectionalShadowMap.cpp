#include "OmnidirectionalShadowMap.h"
#include "Debugging.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

OmnidirectionalShadowMap::OmnidirectionalShadowMap(float fScreenNear, float fScreenDepth)
	: m_pShadowMapRenderPass(nullptr)
	, m_pShadowMapCubeShaderView(nullptr)
	, m_pShadowMapCubeTexture(nullptr)
	, m_pShadowMapCubeDepthView(nullptr)
{
	m_LightProjMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV2, (FLOAT)kShadowMapSize / (FLOAT)kShadowMapSize, fScreenNear, fScreenDepth);
}

OmnidirectionalShadowMap::~OmnidirectionalShadowMap()
{
	Shutdown();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT OmnidirectionalShadowMap::Initialise(ID3D11Device3* pDevice, ID3D11DeviceContext3* pContext, HWND hwnd)
{
	//Create viewport
	m_ShadowMapViewport.TopLeftX = 0.0f;
	m_ShadowMapViewport.TopLeftY = 0.0f;
	m_ShadowMapViewport.Width = (FLOAT)kShadowMapSize;
	m_ShadowMapViewport.Height = (FLOAT)kShadowMapSize;
	m_ShadowMapViewport.MaxDepth = 1.0f;

	//Create Depth Stencil Buffer
	D3D11_TEXTURE2D_DESC descDepthStencil;
	descDepthStencil.Width = kShadowMapSize;
	descDepthStencil.Height = kShadowMapSize;
	descDepthStencil.MipLevels = 1;
	descDepthStencil.ArraySize = 6;
	descDepthStencil.Format = DXGI_FORMAT_R32_TYPELESS;
	descDepthStencil.SampleDesc.Count = 1;
	descDepthStencil.SampleDesc.Quality = 0;
	descDepthStencil.Usage = D3D11_USAGE_DEFAULT;
	descDepthStencil.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	descDepthStencil.CPUAccessFlags = 0;
	descDepthStencil.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

	HRESULT result = pDevice->CreateTexture2D(&descDepthStencil, NULL, &m_pShadowMapCubeTexture);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create shadow map depth stencil");
		return result;
	}

	//Create Depth Stencil View
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	descDSV.Format = DXGI_FORMAT_D32_FLOAT;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
	descDSV.Flags = 0;
	descDSV.Texture2DArray.MipSlice = 0;
	descDSV.Texture2DArray.FirstArraySlice = 0;
	descDSV.Texture2DArray.ArraySize = 6;

	result = pDevice->CreateDepthStencilView(m_pShadowMapCubeTexture, &descDSV, &m_pShadowMapCubeDepthView);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create shadow map depth stencil view");
		return result;
	}

	//Create Shader Resource View
	D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
	descSRV.Format = DXGI_FORMAT_R32_FLOAT;
	descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	descSRV.TextureCube.MostDetailedMip = 0;
	descSRV.TextureCube.MipLevels = 1;

	result = pDevice->CreateShaderResourceView(m_pShadowMapCubeTexture, &descSRV, &m_pShadowMapCubeShaderView);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create shadow map cube shader resource view");
		return result;
	}

	D3D11_INPUT_ELEMENT_DESC polyLayout[5];
	//Setup data layout for the shader, needs to match the VertexType struct in the Mesh class and in the shader code.
	polyLayout[0].SemanticName = "POSITION";
	polyLayout[0].SemanticIndex = 0;
	polyLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polyLayout[0].InputSlot = 0;
	polyLayout[0].AlignedByteOffset = 0;
	polyLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polyLayout[0].InstanceDataStepRate = 0;

	polyLayout[1].SemanticName = "NORMAL";
	polyLayout[1].SemanticIndex = 0;
	polyLayout[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polyLayout[1].InputSlot = 0;
	polyLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polyLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polyLayout[1].InstanceDataStepRate = 0;


	polyLayout[2].SemanticName = "TEXCOORD";
	polyLayout[2].SemanticIndex = 0;
	polyLayout[2].Format = DXGI_FORMAT_R32G32_FLOAT;
	polyLayout[2].InputSlot = 0;
	polyLayout[2].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polyLayout[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polyLayout[2].InstanceDataStepRate = 0;

	polyLayout[3].SemanticName = "TANGENT";
	polyLayout[3].SemanticIndex = 0;
	polyLayout[3].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polyLayout[3].InputSlot = 0;
	polyLayout[3].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polyLayout[3].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polyLayout[3].InstanceDataStepRate = 0;

	polyLayout[4].SemanticName = "BINORMAL";
	polyLayout[4].SemanticIndex = 0;
	polyLayout[4].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polyLayout[4].InputSlot = 0;
	polyLayout[4].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polyLayout[4].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polyLayout[4].InstanceDataStepRate = 0;

	//Get the number of elements in the layout
	unsigned int iNumElements(sizeof(polyLayout) / sizeof(polyLayout[0]));

	m_pShadowMapRenderPass = new RenderPass;
	m_pShadowMapRenderPass->Initialise(pDevice, hwnd, polyLayout, iNumElements, L"../Assets/Shaders/OmniShadowMap.hlsl", "VSMain", "GSMain", "PSMain");
	
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create input layout");
		return false;
	}

	D3D11_BUFFER_DESC matrixBufferDesc;
	//Setup the description of the dynamic matrix constant buffer that is in the shader..
	matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	matrixBufferDesc.ByteWidth = sizeof(MatrixBuffer);
	matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	matrixBufferDesc.MiscFlags = 0;
	matrixBufferDesc.StructureByteStride = 0;

	//Create the buffer so we can access it from within this class
	result = pDevice->CreateBuffer(&matrixBufferDesc, NULL, &m_pMatrixBuffer);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create matrix buffer");
		return false;
	}

	D3D11_BUFFER_DESC lightBufferDesc;
	//Setup the description of the dynamic matrix constant buffer that is in the shader..
	lightBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	lightBufferDesc.ByteWidth = sizeof(LightBuffer);
	lightBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	lightBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	lightBufferDesc.MiscFlags = 0;
	lightBufferDesc.StructureByteStride = 0;

	//Create the buffer so we can access it from within this class
	result = pDevice->CreateBuffer(&lightBufferDesc, NULL, &m_pLightBuffer);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create light buffer");
		return false;
	}

	D3D11_BUFFER_DESC lightRangeBufferDesc;
	//Setup the description of the dynamic matrix constant buffer that is in the shader..
	lightRangeBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	lightRangeBufferDesc.ByteWidth = sizeof(LightRangeBuffer);
	lightRangeBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	lightRangeBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	lightRangeBufferDesc.MiscFlags = 0;
	lightRangeBufferDesc.StructureByteStride = 0;

	//Create the buffer so we can access it from within this class
	result = pDevice->CreateBuffer(&lightRangeBufferDesc, NULL, &m_pLightRangeBuffer);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create light range buffer");
		return false;
	}


	D3D11_BUFFER_DESC lightProjectionsBufferDesc;
	lightProjectionsBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	lightProjectionsBufferDesc.ByteWidth = sizeof(LightProjectionBuffer);
	lightProjectionsBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	lightProjectionsBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	lightProjectionsBufferDesc.MiscFlags = 0;
	lightProjectionsBufferDesc.StructureByteStride = 0;

	result = pDevice->CreateBuffer(&lightProjectionsBufferDesc, NULL, &m_pLightProjectionBuffer);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create light projection buffer");
		return false;
	}

	return result;
}

void OmnidirectionalShadowMap::SetRenderOutputToShadowMap(ID3D11DeviceContext* pDeviceContext)
{
	
	pDeviceContext->OMSetRenderTargets(0, NULL, m_pShadowMapCubeDepthView);
	pDeviceContext->RSSetViewports(1, &m_ShadowMapViewport);
}

void OmnidirectionalShadowMap::SetRenderStart(ID3D11DeviceContext3* pDeviceContext)
{
	m_pShadowMapRenderPass->SetActiveRenderPass(pDeviceContext);
}

bool OmnidirectionalShadowMap::Render(ID3D11DeviceContext* pDeviceContext, int iIndexCount)
{
	//Render the triangles
	pDeviceContext->DrawIndexed(iIndexCount, 0, 0);

	return true;
}

void OmnidirectionalShadowMap::SetRenderFinished(ID3D11DeviceContext* pDeviceContext)
{
	pDeviceContext->GSSetShader(nullptr, nullptr, 0);
	pDeviceContext->VSSetShader(nullptr, nullptr, 0);
	pDeviceContext->PSSetShader(nullptr, nullptr, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OmnidirectionalShadowMap::ClearShadowMap(ID3D11DeviceContext* pDeviceContext)
{
	pDeviceContext->ClearDepthStencilView(m_pShadowMapCubeDepthView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OmnidirectionalShadowMap::Shutdown()
{

	if (m_pShadowMapRenderPass)
	{
		m_pShadowMapRenderPass->Shutdown();
		delete m_pShadowMapRenderPass;
		m_pShadowMapRenderPass = nullptr;
	}

	m_pMatrixBuffer->Release();
	m_pMatrixBuffer = nullptr;
	m_pLightBuffer->Release();
	m_pLightBuffer = nullptr;
	m_pLightRangeBuffer->Release();
	m_pLightRangeBuffer = nullptr;
	m_pLightProjectionBuffer->Release();
	m_pLightProjectionBuffer = nullptr;

	m_pShadowMapCubeTexture->Release();
	m_pShadowMapCubeTexture = nullptr;
	m_pShadowMapCubeDepthView->Release();
	m_pShadowMapCubeDepthView = nullptr;
	m_pShadowMapCubeShaderView->Release();
	m_pShadowMapCubeShaderView = nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool OmnidirectionalShadowMap::SetShaderParams(ID3D11DeviceContext* pDeviceContext, const XMFLOAT4& lightPosition, float lightRange, const XMMATRIX& mWorld)
{
	XMVECTOR lightPos = XMLoadFloat4(&lightPosition);
	//Projections for rendering the 6 sides of the cube map
	// +x, -x, +y, -y, +z, -z
	m_LightViewProjMatrices[0] = XMMatrixLookAtLH(lightPos, lightPos + XMVectorSet(10.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)) * m_LightProjMatrix;
	m_LightViewProjMatrices[1] = XMMatrixLookAtLH(lightPos, lightPos + XMVectorSet(-10.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)) * m_LightProjMatrix;
	m_LightViewProjMatrices[2] = XMMatrixLookAtLH(lightPos, lightPos + XMVectorSet(0.0f, 10.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f)) * m_LightProjMatrix;
	m_LightViewProjMatrices[3] = XMMatrixLookAtLH(lightPos, lightPos + XMVectorSet(0.0f, -10.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)) * m_LightProjMatrix;
	m_LightViewProjMatrices[4] = XMMatrixLookAtLH(lightPos, lightPos + XMVectorSet(0.0f, 0.0f, 10.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)) * m_LightProjMatrix;
	m_LightViewProjMatrices[5] = XMMatrixLookAtLH(lightPos, lightPos + XMVectorSet(0.0f, 0.0f, -10.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)) * m_LightProjMatrix;

	

	pDeviceContext->VSSetConstantBuffers(0, 1, &m_pMatrixBuffer);
	pDeviceContext->VSSetConstantBuffers(1, 1, &m_pLightBuffer);

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT result = pDeviceContext->Map(m_pMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(result))
	{
		MatrixBuffer* pBuffer = static_cast<MatrixBuffer*>(mappedResource.pData);
		pBuffer->world = mWorld;

		pDeviceContext->Unmap(m_pMatrixBuffer, 0);
	}

	result = pDeviceContext->Map(m_pLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(result))
	{
		LightBuffer* pBuffer = static_cast<LightBuffer*>(mappedResource.pData);
		pBuffer->vWorldSpaceLightPosition = lightPosition;

		pDeviceContext->Unmap(m_pLightBuffer, 0);
	}

	
	pDeviceContext->GSSetConstantBuffers(0, 1, &m_pLightProjectionBuffer);

	result = pDeviceContext->Map(m_pLightProjectionBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(result))
	{
		LightProjectionBuffer* pBuffer = static_cast<LightProjectionBuffer*>(mappedResource.pData);
		for (int index = 0; index < 6; ++index)
		{
			pBuffer->lightViewProjMatrices[index] = m_LightViewProjMatrices[index];
		}

		pDeviceContext->Unmap(m_pLightProjectionBuffer, 0);
	}

	pDeviceContext->PSSetConstantBuffers(0, 1, &m_pLightRangeBuffer);

	result = pDeviceContext->Map(m_pLightRangeBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(result))
	{
		LightRangeBuffer* pBuffer = static_cast<LightRangeBuffer*>(mappedResource.pData);
		pBuffer->range = lightRange;

		pDeviceContext->Unmap(m_pLightRangeBuffer, 0);
	}

	
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OmnidirectionalShadowMap::OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename)
{
	//Get ptr to error message text buffer
	char* compileErrors = (char*)(errorMessage->GetBufferPointer());

	//get the length of the message..
	size_t u_iBufferSize = errorMessage->GetBufferSize();

	ofstream fout;
	fout.open("shader-error.txt");
	//Write out the error to the file..

	for (size_t i = 0; i < u_iBufferSize; i++)
	{
		fout << compileErrors[i];
	}
	//Close file
	fout.close();

	errorMessage->Release();
	errorMessage = nullptr;

	// Pop a message up on the screen to notify the user to check the text file for compile errors.
	MessageBox(hwnd, L"Error compiling shader. Check shader-error.txt for message.", shaderFilename, MB_OK);

}