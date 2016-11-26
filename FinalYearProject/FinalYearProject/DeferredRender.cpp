#include "DeferredRender.h"
#include "Debugging.h"
#include "OmnidirectionalShadowMap.h"


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DeferredRender::DeferredRender()
	: m_pLightingPass(nullptr)
	, m_pSampleState (nullptr)
	, m_pMatrixBuffer(nullptr)
	, m_pCameraBuffer(nullptr)
{
	for (int i = 0; i < BufferType::btMax; i++)
	{
		m_arrBufferTextures[i] = nullptr;
	}

	m_pDepthStencilBuffer = nullptr;
	m_pDepthStencilView = nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DeferredRender::~DeferredRender()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeferredRender::SetRenderTargets(ID3D11DeviceContext3* pContext)
{
	ID3D11RenderTargetView* arrRenderTargets[BufferType::btMax];
	for (int i = 0; i < BufferType::btMax; i++)
	{
		arrRenderTargets[i] = m_arrBufferTextures[i]->GetRenderTargetView();
	}
	

	pContext->OMSetRenderTargets(BufferType::btMax, arrRenderTargets, m_pDepthStencilView);

	pContext->RSSetViewports(1, &m_viewport);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeferredRender::ClearRenderTargets(ID3D11DeviceContext3* pContext, float r, float g, float b, float a)
{
	float colour[4];
	colour[0] = r;
	colour[1] = g;
	colour[2] = b;
	colour[3] = a;

	for (int i = 0; i < BufferType::btMax; i++)
	{
		pContext->ClearRenderTargetView(m_arrBufferTextures[i]->GetRenderTargetView(), colour);
	}

	pContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.f, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ID3D11ShaderResourceView* DeferredRender::GetShaderResourceView(BufferType index)
{
	return m_arrBufferTextures[index]->GetShaderResourceView();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT DeferredRender::Initialise(ID3D11Device3* pDevice, ID3D11DeviceContext* pContext, HWND hwnd, int iTextureWidth, int iTextureHeight, float fScreenDepth, float fScreenNear)
{
	HRESULT res;

	m_iTextureHeight = iTextureHeight;
	m_iTextureWidth = iTextureWidth;

	//Create array of textures for deferred render shader to output to
	for (int i = 0; i < BufferType::btMax; i++)
	{
		m_arrBufferTextures[i] = new Texture2D;
		res = m_arrBufferTextures[i]->Init(pDevice, m_iTextureWidth, m_iTextureHeight, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
		if (FAILED(res))
		{
			VS_LOG_VERBOSE("Failed to create render target textures");
			return false;
		}
	}

	D3D11_TEXTURE2D_DESC depthBufferDesc;
	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
	ZeroMemory(&depthBufferDesc, sizeof(depthBufferDesc));

	// Set up the description of the depth buffer.
	depthBufferDesc.Width = m_iTextureWidth;
	depthBufferDesc.Height = m_iTextureHeight;
	depthBufferDesc.MipLevels = 1;
	depthBufferDesc.ArraySize = 1;
	depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthBufferDesc.SampleDesc.Count = 1;
	depthBufferDesc.SampleDesc.Quality = 0;
	depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthBufferDesc.CPUAccessFlags = 0;
	depthBufferDesc.MiscFlags = 0;

	// Create the texture for the depth buffer using the filled out description.
	res = pDevice->CreateTexture2D(&depthBufferDesc, NULL, &m_pDepthStencilBuffer);
	if (FAILED(res))
	{
		return false;
	}

	// Initailze the depth stencil view description.
	ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));

	// Set up the depth stencil view description.
	depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Texture2D.MipSlice = 0;

	// Create the depth stencil view.
	res = pDevice->CreateDepthStencilView(m_pDepthStencilBuffer, &depthStencilViewDesc, &m_pDepthStencilView);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to create depth stencil view");
		return false;
	}

	// Setup the viewport for rendering.
	m_viewport.Width = (float)m_iTextureWidth;
	m_viewport.Height = (float)m_iTextureHeight;
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;
	m_viewport.TopLeftX = 0.0f;
	m_viewport.TopLeftY = 0.0f;

	res = InitialiseShader(pDevice, hwnd, L"LightingPass.hlsl");
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to initialise lighting shader");
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeferredRender::Shutdown()
{
	if (m_pDepthStencilView)
	{
		m_pDepthStencilView->Release();
		m_pDepthStencilView = nullptr;
	}

	if (m_pDepthStencilBuffer)
	{
		m_pDepthStencilBuffer->Release();
		m_pDepthStencilBuffer = nullptr;
	}

	for (int i = 0; i < BufferType::btMax; i++)
	{

		m_arrBufferTextures[i]->Shutdown();
	}

	ShutdownShader();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DeferredRender::RenderLightingPass(ID3D11DeviceContext3* pContext, int iIndexCount, XMMATRIX mWorld, XMMATRIX mView, XMMATRIX mProjection, const XMFLOAT3& vCamPos, VoxelisedScene* pVoxelisedScene, GIRenderFlag eGIFlags)
{
	if (!SetShaderParameters(pContext, mWorld, mView, mProjection, vCamPos, pVoxelisedScene, eGIFlags))
	{
		return false;
	}

	RenderShader(pContext, iIndexCount);

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DeferredRender::InitialiseShader(ID3D11Device3* pDevice, HWND hwnd, WCHAR* sShaderFilename)
{
	HRESULT res;

	//Create vertex input layout description..
	D3D11_INPUT_ELEMENT_DESC polygonLayout[2];

	polygonLayout[0].SemanticName = "POSITION";
	polygonLayout[0].SemanticIndex = 0;
	polygonLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polygonLayout[0].InputSlot = 0;
	polygonLayout[0].AlignedByteOffset = 0;
	polygonLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[0].InstanceDataStepRate = 0;

	polygonLayout[1].SemanticName = "TEXCOORD";
	polygonLayout[1].SemanticIndex = 0;
	polygonLayout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	polygonLayout[1].InputSlot = 0;
	polygonLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polygonLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[1].InstanceDataStepRate = 0;

	unsigned int numElements = sizeof(polygonLayout) / sizeof(polygonLayout[0]);

	m_pLightingPass = new RenderPass;
	m_pLightingPass->Initialise(pDevice, hwnd, polygonLayout, numElements, sShaderFilename, "VSMain", nullptr, "PSMain");

	// Create a texture sampler state description.
	D3D11_SAMPLER_DESC samplerDesc;
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samplerDesc.BorderColor[0] = 0;
	samplerDesc.BorderColor[1] = 0;
	samplerDesc.BorderColor[2] = 0;
	samplerDesc.BorderColor[3] = 0;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// Create the texture sampler state.
	res = pDevice->CreateSamplerState(&samplerDesc, &m_pSampleState);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to create texture sampler state");
		return false;
	}

	//Create ShadowMap Sampler State
	D3D11_SAMPLER_DESC shadowSamplerDesc;
	shadowSamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	shadowSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	shadowSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	shadowSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	shadowSamplerDesc.MipLODBias = 0;
	shadowSamplerDesc.MaxAnisotropy = 16;
	shadowSamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
	shadowSamplerDesc.BorderColor[0] = 0.0f;
	shadowSamplerDesc.BorderColor[1] = 0.0f;
	shadowSamplerDesc.BorderColor[2] = 0.0f;
	shadowSamplerDesc.BorderColor[3] = 0.0f;
	shadowSamplerDesc.MinLOD = 0.0f;
	shadowSamplerDesc.MaxLOD = 0.0f;

	res = pDevice->CreateSamplerState(&shadowSamplerDesc, &m_pShadowMapSampleState);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to create shadow map sampler");
		return res;
	}

	// Create a texture sampler state description for the voxel sampler
	D3D11_SAMPLER_DESC voxelSamplerDesc;
	voxelSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	voxelSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	voxelSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	voxelSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	voxelSamplerDesc.MipLODBias = 0.0f;
	voxelSamplerDesc.MaxAnisotropy = 16;
	voxelSamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	voxelSamplerDesc.BorderColor[0] = 0;
	voxelSamplerDesc.BorderColor[1] = 0;
	voxelSamplerDesc.BorderColor[2] = 0;
	voxelSamplerDesc.BorderColor[3] = 0;
	voxelSamplerDesc.MinLOD = 0;
	voxelSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// Create the texture sampler state for the voxel sampler
	res = pDevice->CreateSamplerState(&voxelSamplerDesc, &m_pVoxelSampler);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to create texture sampler state");
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

	//Create the buffer so we can access it
	res = pDevice->CreateBuffer(&matrixBufferDesc, nullptr, &m_pMatrixBuffer);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to create matrix buffer");
		return false;
	}

	D3D11_BUFFER_DESC giRenderFlagBuffer;
	//Setup the description of the dynamic matrix constant buffer that is in the shader..
	giRenderFlagBuffer.Usage = D3D11_USAGE_DYNAMIC;
	giRenderFlagBuffer.ByteWidth = sizeof(RenderFlags);
	giRenderFlagBuffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	giRenderFlagBuffer.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	giRenderFlagBuffer.MiscFlags = 0;
	giRenderFlagBuffer.StructureByteStride = 0;

	//Create the buffer so we can access it
	res = pDevice->CreateBuffer(&giRenderFlagBuffer, nullptr, &m_pGIRenderFlagBuffer);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to create render flags buffer");
		return false;
	}

	D3D11_BUFFER_DESC cameraBufferDesc;
	cameraBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	cameraBufferDesc.ByteWidth = sizeof(CameraBuffer);
	cameraBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cameraBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cameraBufferDesc.MiscFlags = 0;
	cameraBufferDesc.StructureByteStride = 0;

	//Create the light buffer so we can access it
	res = pDevice->CreateBuffer(&cameraBufferDesc, nullptr, &m_pCameraBuffer);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to create camera buffer");
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeferredRender::ShutdownShader()
{
	if (m_pLightingPass)
	{
		m_pLightingPass->Shutdown();
		delete m_pLightingPass;
		m_pLightingPass = nullptr;
	}

	if (m_pCameraBuffer)
	{
		m_pCameraBuffer->Release();
		m_pCameraBuffer = nullptr;
	}

	if (m_pMatrixBuffer)
	{
		m_pMatrixBuffer->Release();
		m_pMatrixBuffer = nullptr;
	}

	if (m_pSampleState)
	{
		m_pSampleState->Release();
		m_pSampleState = nullptr;
	}

	if (m_pShadowMapSampleState)
	{
		m_pShadowMapSampleState->Release();
		m_pShadowMapSampleState = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeferredRender::OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename)
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DeferredRender::SetShaderParameters(ID3D11DeviceContext3* pContext, XMMATRIX mWorld, XMMATRIX mView, XMMATRIX mProjection, const XMFLOAT3& vCamPos, VoxelisedScene* pVoxelisedScene, GIRenderFlag eGIFlags)
{
	HRESULT res;
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	
	mWorld = XMMatrixTranspose(mWorld);
	mView = XMMatrixTranspose(mView);
	mProjection = XMMatrixTranspose(mProjection);

	res = pContext->Map(m_pMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to lock the matrix buffer");
		return false;
	}
	MatrixBuffer* pMatrixData;
	//Get pointer to the buffer data
	pMatrixData = (MatrixBuffer*)mappedResource.pData;

	pMatrixData->mWorld = mWorld;
	pMatrixData->mView = mView;
	pMatrixData->mProjection = mProjection;

	//Unlock the buffer
	pContext->Unmap(m_pMatrixBuffer, 0);

	res = pContext->Map(m_pGIRenderFlagBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to lock the matrix buffer");
		return false;
	}
	RenderFlags* pRenderFlagsData;
	//Get pointer to the buffer data
	pRenderFlagsData = (RenderFlags*)mappedResource.pData;

	pRenderFlagsData->eRenderGlobalIllumination = eGIFlags;
	pRenderFlagsData->padding[0] = 0;
	pRenderFlagsData->padding[1] = 0;
	pRenderFlagsData->padding[2] = 0;


	//Unlock the buffer
	pContext->Unmap(m_pGIRenderFlagBuffer, 0);

	res = pContext->Map(m_pCameraBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to lock the camera buffer");
		return false;
	}
	CameraBuffer* pCameraData;
	pCameraData = (CameraBuffer*)mappedResource.pData;
	if (pVoxelisedScene)
	{
		pCameraData->fVoxelScale = pVoxelisedScene->GetVoxelScale();
		pCameraData->vCameraPosition = vCamPos;
		pCameraData->mWorldToVoxelGrid = pVoxelisedScene->GetWorldToVoxelMatrix();
	}
	else
	{
		pCameraData->fVoxelScale = 1.f;
		pCameraData->vCameraPosition = vCamPos;
		pCameraData->mWorldToVoxelGrid = XMMatrixIdentity();
	}
	pContext->Unmap(m_pCameraBuffer, 0);


	unsigned int u_iBufferNumber = 0;

	ID3D11Buffer* pLightBuffer = LightManager::Get()->GetLightBuffer();
	pContext->VSSetConstantBuffers(u_iBufferNumber, 1, &m_pMatrixBuffer);
	pContext->PSSetConstantBuffers(u_iBufferNumber, 1, &pLightBuffer);

	u_iBufferNumber++;
	pContext->PSSetConstantBuffers(u_iBufferNumber, 1, &m_pCameraBuffer);
	
	u_iBufferNumber++;
	pContext->PSSetConstantBuffers(u_iBufferNumber, 1, &m_pGIRenderFlagBuffer);

	for (int i = 0; i < BufferType::btMax; i++)
	{
		ID3D11ShaderResourceView* pResource = m_arrBufferTextures[i]->GetShaderResourceView();
		pContext->PSSetShaderResources(i, 1, &pResource);
	}
	ID3D11ShaderResourceView* pShadowCubeArray[NUM_LIGHTS];
	ID3D11ShaderResourceView* nullSRV = nullptr;
	for (int i = 0; i < NUM_LIGHTS; i++)
	{
		OmnidirectionalShadowMap* pShadowMap = LightManager::Get()->GetPointLight(i)->GetShadowMap();
		if (pShadowMap)
		{
			pShadowCubeArray[i] = pShadowMap->GetShadowMapShaderResource();
		}
		else
		{
			pShadowCubeArray[i] = nullSRV;
		}
	}
	pContext->PSSetShaderResources(BufferType::btMax, NUM_LIGHTS, pShadowCubeArray);
	if (pVoxelisedScene)
	{
		ID3D11ShaderResourceView* pVoxelRadianceVolumes = pVoxelisedScene->GetRadianceVolume();
		pContext->PSSetShaderResources(BufferType::btMax + NUM_LIGHTS, 1, &pVoxelRadianceVolumes);
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DeferredRender::RenderShader(ID3D11DeviceContext3* pContext, int iIndexCount)
{
	m_pLightingPass->SetActiveRenderPass(pContext);

	// Set the sampler state in the pixel shader.
	pContext->PSSetSamplers(0, 1, &m_pSampleState);
	pContext->PSSetSamplers(1, 1, &m_pShadowMapSampleState);
	pContext->PSSetSamplers(2, 1, &m_pVoxelSampler);

	// Render the geometry.
	pContext->DrawIndexed(iIndexCount, 0, 0);

	ID3D11ShaderResourceView* nullSRV[9] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	pContext->PSSetShaderResources(0, 9, nullSRV);

	ID3D11SamplerState* sample[2] = { nullptr, nullptr };
	pContext->PSSetSamplers(0, 2, sample);

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////


