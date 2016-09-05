#include "VoxelisePass.h"
#include "Debugging.h"


HRESULT VoxelisePass::Initialise(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, HWND hwnd, AABB voxelGridAABB, int iScreenWidth, int iScreenHeight)
{
	m_iScreenHeight = iScreenHeight;
	m_iScreenWidth = iScreenWidth;

	//Initialise Matrices..
	float voxelGridSize = 0;
	XMFLOAT3 vVoxelGridSize;
	XMStoreFloat3(&vVoxelGridSize, (XMLoadFloat3(&voxelGridAABB.Max) - XMLoadFloat3(&voxelGridAABB.Min)));
	//make the voxel grid into a square which will fit the whole scene
	if (vVoxelGridSize.x > vVoxelGridSize.y)
	{
		if (vVoxelGridSize.x > vVoxelGridSize.z)
		{
			voxelGridSize = vVoxelGridSize.x;
		}
		else
		{
			voxelGridSize = vVoxelGridSize.z;
		}
	}
	else
	{
		if (vVoxelGridSize.y > vVoxelGridSize.z)
		{
			voxelGridSize = vVoxelGridSize.y;
		}
		else
		{
			voxelGridSize = vVoxelGridSize.z;
		}
	}
	m_vVoxelGridSize = XMFLOAT3(voxelGridSize, voxelGridSize, voxelGridSize);
	//Create ortho projections for the axes..
	XMMATRIX camProjectionMatrix(2.0f / voxelGridSize, 0.0f,				 0.0f,				   0.0f,
								 0.0f,				   2.0f / voxelGridSize, 0.0f,				   0.0f,
								 0.0f,				   0.0f,				-2.0f / voxelGridSize, 0.0f,
								 0.0f,				   0.0f,				 0.0f,				   1.0f);

	XMMATRIX viewMatrixRightCam(0.0f, 0.0f, 1.0f, 0.0f,
								0.0f, 1.0f, 0.0f, 0.0f,
								1.0f, 0.0f, 0.0f, 0.0f,
								0.0f, 0.0f, -1.0f, 1.0f);

	XMMATRIX viewMatrixTopCam(1.0f, 0.0f, 0.0f, 0.0f,
							  0.0f, 0.0f, 1.0f, 0.0f,
							  0.0f, 1.0f, 0.0f, 0.0f,
							  0.0f, 0.0f, -1.0f, 1.0f);

	XMMATRIX viewMatrixFarCam(-1.0f, 0.0f, 0.0f, 0.0f,
							  0.0f, 1.0f, 0.0f, 0.0f,
							  0.0f, 0.0f, 1.0f, 0.0f,
							  0.0f, 0.0f, -1.0f, 1.0f);

	m_mViewProjMatrices[0] = camProjectionMatrix * viewMatrixRightCam;
	m_mViewProjMatrices[1] = camProjectionMatrix * viewMatrixTopCam;
	m_mViewProjMatrices[2] = camProjectionMatrix * viewMatrixFarCam;

	//TODO: WorldToVoxelGridMatrix...
	XMMATRIX mWorldToVoxelGridTranslate = XMMatrixTranslation(-voxelGridAABB.Min.x, -voxelGridAABB.Min.y, -voxelGridAABB.Min.z);
	XMMATRIX mWorldToVoxelGridScaling = XMMatrixScaling(TEXTURE_DIMENSION / voxelGridSize, TEXTURE_DIMENSION / voxelGridSize, TEXTURE_DIMENSION / voxelGridSize);

	m_mWorldToVoxelGrid = mWorldToVoxelGridScaling * mWorldToVoxelGridTranslate;

	m_pDebugOutput = new Texture2D;
	m_pDebugOutput->Init(pDevice, iScreenWidth, iScreenHeight, 1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT, D3D11_USAGE_DEFAULT, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE);

	ID3D10Blob* pErrorMessage(nullptr);
	ID3D10Blob* pVertexShaderBuffer(nullptr);
	ID3D10Blob* pPixelShaderBuffer(nullptr);
	ID3D10Blob* pGeometryShaderBuffer(nullptr);
	ID3D10Blob* pComputeShaderBuffer(nullptr);
	ID3D10Blob* pComputeShaderBuffer2(nullptr);

	//Compile the clear voxels compute shader code
	HRESULT result = D3DCompileFromFile(L"Voxelise_Clear.hlsl", nullptr, nullptr, "CSClearVoxels", "cs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, &pComputeShaderBuffer, &pErrorMessage);
	if (FAILED(result))
	{
		if (pErrorMessage)
		{
			//If the shader failed to compile it should have written something to error message, so we output that here
			OutputShaderErrorMessage(pErrorMessage, hwnd, L"Voxelise_Clear.hlsl");
		}
		else
		{
			//if it hasn't, then it couldn't find the shader file..
			MessageBox(hwnd, L"Voxelise_Clear.hlsl", L"Missing Shader File", MB_OK);
		}
		return false;
	}

	result = pDevice->CreateComputeShader(pComputeShaderBuffer->GetBufferPointer(), pComputeShaderBuffer->GetBufferSize(), nullptr, &m_pClearVoxelsComputeShader);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create the compute shader.");
		return false;
	}

	//Compile the debug voxel view compute shader code
	result = D3DCompileFromFile(L"VoxelDebugRaymarch.hlsl", nullptr, nullptr, "RaymarchCS", "cs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, &pComputeShaderBuffer2, &pErrorMessage);
	if (FAILED(result))
	{
		if (pErrorMessage)
		{
			//If the shader failed to compile it should have written something to error message, so we output that here
			OutputShaderErrorMessage(pErrorMessage, hwnd, L"VoxelDebugRaymarch.hlsl");
		}
		else
		{
			//if it hasn't, then it couldn't find the shader file..
			MessageBox(hwnd, L"VoxelDebugRaymarch.hlsl", L"Missing Shader File", MB_OK);
		}
		return false;
	}

	result = pDevice->CreateComputeShader(pComputeShaderBuffer2->GetBufferPointer(), pComputeShaderBuffer2->GetBufferSize(), nullptr, &m_pRenderDebugToTextureComputeShader);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create the compute shader.");
		return false;
	}


	//Compile the voxelisation pass vertex shader code
	result = D3DCompileFromFile(L"Voxelise_Populate.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, &pVertexShaderBuffer, &pErrorMessage);
	if (FAILED(result))
	{
		if (pErrorMessage)
		{
			//If the shader failed to compile it should have written something to error message, so we output that here
			OutputShaderErrorMessage(pErrorMessage, hwnd, L"Voxelise_Populate.hlsl");
		}
		else
		{
			//if it hasn't, then it couldn't find the shader file..
			MessageBox(hwnd, L"Voxelise_Populate.hlsl", L"Missing Shader File", MB_OK);
		}
		return false;
	}

	result = pDevice->CreateVertexShader(pVertexShaderBuffer->GetBufferPointer(), pVertexShaderBuffer->GetBufferSize(), nullptr, &m_pVertexShader);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create the vertex shader.");
		return false;
	}

	//Compile the voxelisation pass geometry shader code
	result = D3DCompileFromFile(L"Voxelise_Populate.hlsl", nullptr, nullptr, "GSMain", "gs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, &pGeometryShaderBuffer, &pErrorMessage);
	if (FAILED(result))
	{
		if (pErrorMessage)
		{
			//If the shader failed to compile it should have written something to error message, so we output that here
			OutputShaderErrorMessage(pErrorMessage, hwnd, L"Voxelise_Populate.hlsl");
		}
		else
		{
			//if it hasn't, then it couldn't find the shader file..
			MessageBox(hwnd, L"Voxelise_Populate.hlsl", L"Missing Shader File", MB_OK);
		}
		return false;
	}

	result = pDevice->CreateGeometryShader(pGeometryShaderBuffer->GetBufferPointer(), pGeometryShaderBuffer->GetBufferSize(), nullptr, &m_pGeometryShader);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create the geometry shader.");
		return false;
	}

	//Compile the voxelisation pass pixel shader code
	result = D3DCompileFromFile(L"Voxelise_Populate.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, &pPixelShaderBuffer, &pErrorMessage);
	if (FAILED(result))
	{
		if (pErrorMessage)
		{
			//If the shader failed to compile it should have written something to error message, so we output that here
			OutputShaderErrorMessage(pErrorMessage, hwnd, L"Voxelise_Populate.hlsl");
		}
		else
		{
			//if it hasn't, then it couldn't find the shader file..
			MessageBox(hwnd, L"Voxelise_Populate.hlsl", L"Missing Shader File", MB_OK);
		}
		return false;
	}

	result = pDevice->CreatePixelShader(pPixelShaderBuffer->GetBufferPointer(), pPixelShaderBuffer->GetBufferSize(), nullptr, &m_pPixelShader);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create the pixel shader.");
		return false;
	}

	//Initialise the input layout

	D3D11_INPUT_ELEMENT_DESC polyLayout[6];
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


	polyLayout[3].SemanticName = "COLOR";
	polyLayout[3].SemanticIndex = 0;
	polyLayout[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	polyLayout[3].InputSlot = 0;
	polyLayout[3].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polyLayout[3].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polyLayout[3].InstanceDataStepRate = 0;

	polyLayout[4].SemanticName = "TANGENT";
	polyLayout[4].SemanticIndex = 0;
	polyLayout[4].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polyLayout[4].InputSlot = 0;
	polyLayout[4].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polyLayout[4].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polyLayout[4].InstanceDataStepRate = 0;

	polyLayout[5].SemanticName = "BINORMAL";
	polyLayout[5].SemanticIndex = 0;
	polyLayout[5].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polyLayout[5].InputSlot = 0;
	polyLayout[5].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polyLayout[5].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polyLayout[5].InstanceDataStepRate = 0;

	//Get the number of elements in the layout
	unsigned int iNumElements(sizeof(polyLayout) / sizeof(polyLayout[0]));

	//Create the vertex input layout..
	result = pDevice->CreateInputLayout(polyLayout, iNumElements, pVertexShaderBuffer->GetBufferPointer(), pVertexShaderBuffer->GetBufferSize(), &m_pLayout);

	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create input layout");
		return false;
	}

	//Finished with shader buffers now so they can be released
	pComputeShaderBuffer->Release();
	pComputeShaderBuffer = nullptr;

	pVertexShaderBuffer->Release();
	pVertexShaderBuffer = nullptr;

	pPixelShaderBuffer->Release();
	pPixelShaderBuffer = nullptr;

	pGeometryShaderBuffer->Release();
	pGeometryShaderBuffer = nullptr;

	//Initialise Buffers

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

	D3D11_BUFFER_DESC projMatrixBufferDesc;
	//Setup the description of the dynamic matrix constant buffer that is in the shader..
	projMatrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	projMatrixBufferDesc.ByteWidth = sizeof(ProjectionMatrixBuffer);
	projMatrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	projMatrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	projMatrixBufferDesc.MiscFlags = 0;
	projMatrixBufferDesc.StructureByteStride = 0;

	//Create the buffer so we can access it from within this class
	result = pDevice->CreateBuffer(&projMatrixBufferDesc, NULL, &m_pProjectionMatrixBuffer);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create projection matrix buffer");
		return false;
	}

	D3D11_BUFFER_DESC voxelGridBufferDesc;
	//Setup the description of the dynamic matrix constant buffer that is in the shader..
	voxelGridBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	voxelGridBufferDesc.ByteWidth = sizeof(VoxelGridBuffer);
	voxelGridBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	voxelGridBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	voxelGridBufferDesc.MiscFlags = 0;
	voxelGridBufferDesc.StructureByteStride = 0;

	//Create the buffer so we can access it from within this class
	result = pDevice->CreateBuffer(&voxelGridBufferDesc, NULL, &m_pVoxelGridBuffer);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create matrix buffer");
		return false;
	}

	//Initialise Resources..
	D3D11_TEXTURE3D_DESC textureDesc;
	ZeroMemory(&textureDesc, sizeof(textureDesc));

	// Setup the 3d texture description.
	textureDesc.Width = TEXTURE_DIMENSION;
	textureDesc.Height = TEXTURE_DIMENSION;
	textureDesc.Depth = TEXTURE_DIMENSION;
	textureDesc.MipLevels = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;

	
	result = pDevice->CreateTexture3D(&textureDesc, nullptr, &m_pVoxelisedScene);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create 3d texture");
		return false;
	}

	//Setup the unordered access view so we can render to the textures..
	D3D11_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc;
	unorderedAccessViewDesc.Format = DXGI_FORMAT_R32_UINT;
	unorderedAccessViewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
	unorderedAccessViewDesc.Texture3D.MipSlice = 0;
	unorderedAccessViewDesc.Texture3D.FirstWSlice = 0;
	unorderedAccessViewDesc.Texture3D.WSize = -1;
	
	result = pDevice->CreateUnorderedAccessView(m_pVoxelisedScene, &unorderedAccessViewDesc, &m_pVoxelisedSceneUAV);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create unordered access view");
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	srvDesc.Texture3D.MipLevels = 1;
	srvDesc.Texture3D.MostDetailedMip = 0;
	result = pDevice->CreateShaderResourceView(m_pVoxelisedScene, &srvDesc, &m_pVoxelisedSceneSRV);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create shader resource view");
		return false;
	}

	//Initialise Rasteriser state
	D3D11_RASTERIZER_DESC rasterDesc;
	ZeroMemory(&rasterDesc, sizeof(D3D11_RASTERIZER_DESC));

	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.CullMode = D3D11_CULL_NONE;
	rasterDesc.FrontCounterClockwise = true;
	pDevice->CreateRasterizerState(&rasterDesc, &m_pRasteriserState);

	//Initialise Viewport
	m_pVoxeliseViewport.TopLeftX = 0.f;
	m_pVoxeliseViewport.TopLeftY = 0.f;
	m_pVoxeliseViewport.Width = (float)TEXTURE_DIMENSION;
	m_pVoxeliseViewport.Height = (float)TEXTURE_DIMENSION;
	m_pVoxeliseViewport.MinDepth = 0.f;
	m_pVoxeliseViewport.MaxDepth = 1.f;


	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	sampDesc.MaxAnisotropy = 4;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	pDevice->CreateSamplerState(&sampDesc, &m_pSamplerState);

}

void VoxelisePass::RenderClearVoxelsPass(ID3D11DeviceContext* pContext)
{
	ID3D11UnorderedAccessView* ppUAViewNULL[1] = { nullptr };

	pContext->CSSetShader(m_pClearVoxelsComputeShader, nullptr, 0);
	pContext->CSSetUnorderedAccessViews(0, 1, &m_pVoxelisedSceneUAV, nullptr);
	pContext->Dispatch(1, 1, 1);
	pContext->CSSetShader(nullptr, nullptr, 0);
	pContext->CSSetUnorderedAccessViews(0, 1, ppUAViewNULL, nullptr);
}

void VoxelisePass::RenderDebugViewToTexture(ID3D11DeviceContext* pContext)
{
	ID3D11UnorderedAccessView* ppUAViewNULL[1] = { nullptr };
	ID3D11ShaderResourceView* ppSRVNull[1] = { nullptr };
	pContext->CSSetShader(m_pRenderDebugToTextureComputeShader, nullptr, 0);

	ID3D11UnorderedAccessView* views[1] = { m_pDebugOutput->GetUAV() };
	pContext->CSSetUnorderedAccessViews(0, 1, views, nullptr);

	pContext->CSSetShaderResources(0, 1, &m_pVoxelisedSceneSRV);

	pContext->CSSetSamplers(0, 1, &m_pSamplerState);
	pContext->CSSetConstantBuffers(0, 1, &m_pMatrixBuffer);
	pContext->CSSetConstantBuffers(1, 1, &m_pVoxelGridBuffer);

	//Compute shader dimensions are 16x16
	unsigned int dispatchWidth = (m_iScreenWidth + 16 - 1) / 16;
	unsigned int dispatchHeight = (m_iScreenHeight + 16 - 1) / 16;
	pContext->Dispatch(dispatchWidth, dispatchHeight, 1);

	pContext->CSSetShader(nullptr, nullptr, 0);
	pContext->CSSetUnorderedAccessViews(0, 1, ppUAViewNULL, nullptr);
	pContext->CSSetShaderResources(0, 1, ppSRVNull);
}

bool VoxelisePass::SetShaderParams(ID3D11DeviceContext* pDeviceContext, const XMMATRIX& mWorld, const XMMATRIX& mView, const XMMATRIX& mProjection, const XMFLOAT3& eyePos)
{

	pDeviceContext->IASetInputLayout(m_pLayout);
	pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	pDeviceContext->GSSetShader(m_pGeometryShader, nullptr, 0);
	pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

	pDeviceContext->RSSetState(m_pRasteriserState);

	pDeviceContext->RSSetViewports(1, &m_pVoxeliseViewport);

	XMMATRIX mWorldM = XMMatrixTranspose(mWorld);
	XMMATRIX mViewM = XMMatrixTranspose(mView);
	XMMATRIX mProjectionM = XMMatrixTranspose(mProjection);

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT result = pDeviceContext->Map(m_pMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(result))
	{
		MatrixBuffer* pBuffer = static_cast<MatrixBuffer*>(mappedResource.pData);
		pBuffer->world = mWorldM;
		pBuffer->view = mViewM;
		pBuffer->projection = mProjectionM;
		pBuffer->eyePos = eyePos;
		pBuffer->padding = 0.f;

		pDeviceContext->Unmap(m_pMatrixBuffer, 0);
	}
	pDeviceContext->VSSetConstantBuffers(0, 1, &m_pMatrixBuffer);
	
	result = pDeviceContext->Map(m_pProjectionMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(result))
	{
		ProjectionMatrixBuffer* pBuffer = static_cast<ProjectionMatrixBuffer*>(mappedResource.pData);
		for (int i = 0; i < 3; i++)
		{
			pBuffer->viewProjMatrices[i] = m_mViewProjMatrices[i];
		}
		pBuffer->voxelGridSize = m_vVoxelGridSize;
		pBuffer->VoxelTextureSize = TEXTURE_DIMENSION;
		pDeviceContext->Unmap(m_pProjectionMatrixBuffer, 0);
	}
	pDeviceContext->GSSetConstantBuffers(0, 1, &m_pProjectionMatrixBuffer);
	
	result = pDeviceContext->Map(m_pVoxelGridBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(result))
	{
		VoxelGridBuffer* pBuffer = static_cast<VoxelGridBuffer*>(mappedResource.pData);
		pBuffer->mWorldToVoxelGrid = m_mWorldToVoxelGrid;
		pBuffer->mWorldToVoxelGridInverse = m_mWorldToVoxelGrid; //TODO: ACTUALLY PASS THE INVERSE
		pBuffer->voxelGridSize = m_vVoxelGridSize;
		pBuffer->VoxelTextureSize = TEXTURE_DIMENSION;

		pDeviceContext->Unmap(m_pVoxelGridBuffer, 0);
	}
	pDeviceContext->PSSetConstantBuffers(0, 1, &m_pVoxelGridBuffer);

	pDeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 1, &m_pVoxelisedSceneUAV, 0);

	return true;
}

bool VoxelisePass::Render(ID3D11DeviceContext* pDeviceContext, int iIndexCount)
{
	//Render the triangle
	pDeviceContext->DrawIndexed(iIndexCount, 0, 0);

	return true;
}

void VoxelisePass::PostRender(ID3D11DeviceContext* pContext)
{
	pContext->VSSetShader(nullptr, nullptr, 0);
	pContext->GSSetShader(nullptr, nullptr, 0);
	pContext->PSSetShader(nullptr, nullptr, 0);

	ID3D11UnorderedAccessView* ppUAViewNULL[1] = { nullptr };
	pContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 1, ppUAViewNULL, 0);
}

void VoxelisePass::Shutdown()
{

}

void VoxelisePass::OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename)
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

