#include "VoxelisePass.h"
#include "Debugging.h"

#define NUM_THREADS 16

VoxelisePass::VoxelisePass()
{

	m_sNumThreads = std::to_string(NUM_THREADS);
	m_sNumTexelsPerThread = std::to_string((TEXTURE_DIMENSION * TEXTURE_DIMENSION * TEXTURE_DIMENSION) / (NUM_THREADS * NUM_THREADS));
	m_ComputeShaderDefines[csdNumThreads].Name = "NUM_THREADS";
	m_ComputeShaderDefines[csdNumThreads].Definition = m_sNumThreads.c_str();
	m_ComputeShaderDefines[csdNumTexelsPerThread].Name = "NUM_TEXELS_PER_THREAD";
	m_ComputeShaderDefines[csdNumTexelsPerThread].Definition = m_sNumTexelsPerThread.c_str();
	m_ComputeShaderDefines[csdNulls].Name = nullptr;
	m_ComputeShaderDefines[csdNulls].Definition = nullptr;
}

HRESULT VoxelisePass::Initialise(ID3D11Device3* pDevice, ID3D11DeviceContext* pContext, HWND hwnd, AABB voxelGridAABB, int iScreenWidth, int iScreenHeight)
{
	m_iScreenHeight = iScreenHeight;
	m_iScreenWidth = iScreenWidth;

	//Initialise Matrices..
	float voxelGridSize = 0;
	XMFLOAT3 vVoxelGridSize;
	XMVECTOR Min, Max, VoxelGridSize;
	Max = XMLoadFloat3(&voxelGridAABB.Max);
	Min = XMLoadFloat3(&voxelGridAABB.Min);

	VoxelGridSize = Max - Min;
	XMStoreFloat3(&vVoxelGridSize, VoxelGridSize);
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
	XMMATRIX camProjectionMatrix(2.0f / 64, 0.0f,				 0.0f,							0.f,
								 0.0f,				   2.0f / 64, 0.0f,							0.f,
								 0.0f,				   0.0f,				-2.0f / 64,			0.f,
								 0.0f,				   0.0f,				 0.0f,			    1.0f);

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
	float scale = TEXTURE_DIMENSION / voxelGridSize;

	XMVECTOR translateToOrigin = (- Min - (VoxelGridSize * 0.5f));
	XMMATRIX scaling = XMMatrixScaling(2.f / voxelGridSize, 2.f / voxelGridSize, 2.f / voxelGridSize);
	translateToOrigin = XMVector3Transform(translateToOrigin, scaling);

	m_mWorldToVoxelGrid.r[0].m128_f32[0] = 2.f / voxelGridSize;
	m_mWorldToVoxelGrid.r[0].m128_f32[1] = 0.0f;
	m_mWorldToVoxelGrid.r[0].m128_f32[2] = 0.0f;
	m_mWorldToVoxelGrid.r[0].m128_f32[3] = 0.0f;
	
	m_mWorldToVoxelGrid.r[1].m128_f32[0] = 0.0f;
	m_mWorldToVoxelGrid.r[1].m128_f32[1] = 2.f / voxelGridSize;
	m_mWorldToVoxelGrid.r[1].m128_f32[2] = 0.0f;
	m_mWorldToVoxelGrid.r[1].m128_f32[3] = 0.0f;
	
	m_mWorldToVoxelGrid.r[2].m128_f32[0] = 0.0f;
	m_mWorldToVoxelGrid.r[2].m128_f32[1] = 0.0f;
	m_mWorldToVoxelGrid.r[2].m128_f32[2] = 2.f / voxelGridSize;
	m_mWorldToVoxelGrid.r[2].m128_f32[3] = 0.0f;
	
	m_mWorldToVoxelGrid.r[3] = translateToOrigin;
	m_mWorldToVoxelGrid.r[3].m128_f32[3] = 1.0f;
	
//	m_mWorldToVoxelGrid = mWorldToVoxelGridScaling * m_mWorldToVoxelGrid;

	m_mWorldToVoxelGrid = XMMatrixTranspose(m_mWorldToVoxelGrid);

	XMFLOAT4 testVert = XMFLOAT4(150.f, 10.f, 10.f, 1.f);
	XMVECTOR vtest = XMVector4Transform(XMLoadFloat4(&testVert), m_mWorldToVoxelGrid);
	XMVECTOR vmin = XMVector3Transform(Min, m_mWorldToVoxelGrid);
	XMVECTOR vmax = XMVector3Transform(Max, m_mWorldToVoxelGrid);

	
	XMVECTOR vMaxScaled = ((vmax * 0.5f) + XMLoadFloat3(&XMFLOAT3(0.5f, 0.5f, 0.5f))) * 64;
	XMVECTOR vMinScaled = ((vmin * 0.5f) + XMLoadFloat3(&XMFLOAT3(0.5f, 0.5f, 0.5f))) * 64;

	m_pDebugOutput = new Texture2D;
	m_pDebugOutput->Init(pDevice, iScreenWidth, iScreenHeight, 1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT, D3D11_USAGE_DEFAULT, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE);

	ID3D10Blob* pErrorMessage(nullptr);
	ID3D10Blob* pVertexShaderBuffer(nullptr);
	ID3D10Blob* pPixelShaderBuffer(nullptr);
	ID3D10Blob* pDebugVertexShaderBuffer(nullptr);
	ID3D10Blob* pDebugPixelShaderBuffer(nullptr);
	ID3D10Blob* pGeometryShaderBuffer(nullptr);
	ID3D10Blob* pComputeShaderBuffer(nullptr);
	ID3D10Blob* pComputeShaderBuffer2(nullptr);

	//Compile the clear voxels compute shader code
	HRESULT result = D3DCompileFromFile(L"Voxelise_Clear.hlsl", m_ComputeShaderDefines, nullptr, "CSClearVoxels", "cs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, &pComputeShaderBuffer, &pErrorMessage);
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

	//Compile the debug render voxel vertex shader code
	result = D3DCompileFromFile(L"VoxelRenderShader.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, &pDebugVertexShaderBuffer, &pErrorMessage);
	if (FAILED(result))
	{
		if (pErrorMessage)
		{
			//If the shader failed to compile it should have written something to error message, so we output that here
			OutputShaderErrorMessage(pErrorMessage, hwnd, L"VoxelRenderShader.hlsl");
		}
		else
		{
			//if it hasn't, then it couldn't find the shader file..
			MessageBox(hwnd, L"VoxelRenderShader.hlsl", L"Missing Shader File", MB_OK);
		}
		return false;
	}

	result = pDevice->CreateVertexShader(pDebugVertexShaderBuffer->GetBufferPointer(), pDebugVertexShaderBuffer->GetBufferSize(), nullptr, &m_pDebugVertexShader);
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

	//Compile the debug voxel render pixel shader code
	result = D3DCompileFromFile(L"VoxelRenderShader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, &pDebugPixelShaderBuffer, &pErrorMessage);
	if (FAILED(result))
	{
		if (pErrorMessage)
		{
			//If the shader failed to compile it should have written something to error message, so we output that here
			OutputShaderErrorMessage(pErrorMessage, hwnd, L"VoxelRenderShader.hlsl");
		}
		else
		{
			//if it hasn't, then it couldn't find the shader file..
			MessageBox(hwnd, L"VoxelRenderShader.hlsl", L"Missing Shader File", MB_OK);
		}
		return false;
	}

	result = pDevice->CreatePixelShader(pDebugPixelShaderBuffer->GetBufferPointer(), pDebugPixelShaderBuffer->GetBufferSize(), nullptr, &m_pDebugPixelShader);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create the pixel shader.");
		return false;
	}

	//Initialise the input layout

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
	D3D11_BUFFER_DESC voxelVertBufferDesc;
	//Setup the description of the dynamic matrix constant buffer that is in the shader..
	voxelVertBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	voxelVertBufferDesc.ByteWidth = sizeof(VoxeliseVertexShaderBuffer);
	voxelVertBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	voxelVertBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	voxelVertBufferDesc.MiscFlags = 0;
	voxelVertBufferDesc.StructureByteStride = 0;

	//Create the buffer so we can access it from within this class
	result = pDevice->CreateBuffer(&voxelVertBufferDesc, NULL, &m_pVoxeliseVertexShaderBuffer);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create Voxelise Vertex Shader buffer");
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

	D3D11_BUFFER_DESC perCubeDebugBufferDesc;
	//Setup the description of the dynamic matrix constant buffer that is in the shader..
	perCubeDebugBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	perCubeDebugBufferDesc.ByteWidth = sizeof(PerCubeDebugBuffer);
	perCubeDebugBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	perCubeDebugBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	perCubeDebugBufferDesc.MiscFlags = 0;
	perCubeDebugBufferDesc.StructureByteStride = 0;

	//Create the buffer so we can access it from within this class
	result = pDevice->CreateBuffer(&perCubeDebugBufferDesc, NULL, &m_pPerCubeDebugBuffer);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create cube debug buffer");
		return false;
	}

	m_pVoxelisedScene = new Texture3D;
	m_pVoxelisedScene->Init(pDevice, TEXTURE_DIMENSION, TEXTURE_DIMENSION, TEXTURE_DIMENSION, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE);

	//Initialise Rasteriser state
	D3D11_RASTERIZER_DESC2 rasterDesc;
	ZeroMemory(&rasterDesc, sizeof(D3D11_RASTERIZER_DESC2));

	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.CullMode = D3D11_CULL_NONE;
	rasterDesc.FrontCounterClockwise = true;
	rasterDesc.ConservativeRaster = D3D11_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	pDevice->CreateRasterizerState2(&rasterDesc, &m_pRasteriserState);

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

	m_arrDebugRenderCube = new Mesh;
	m_arrDebugRenderCube->InitialiseCubeFromTxt(pDevice, pContext, hwnd);
	
}

void VoxelisePass::RenderClearVoxelsPass(ID3D11DeviceContext* pContext)
{
	ID3D11UnorderedAccessView* ppUAViewNULL[1] = { nullptr };

	pContext->CSSetShader(m_pClearVoxelsComputeShader, nullptr, 0);

	ID3D11UnorderedAccessView* uav = m_pVoxelisedScene->GetUAV();
	pContext->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
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

	ID3D11ShaderResourceView* srv = m_pVoxelisedScene->GetShaderResourceView();
	pContext->CSSetShaderResources(0, 1, &srv);

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

void VoxelisePass::RenderDebugCubes(ID3D11DeviceContext* pContext, const XMMATRIX& mWorld, const XMMATRIX& mView, const XMMATRIX& mProjection, Camera* pCamera)
{
	pContext->IASetInputLayout(m_pLayout);
	pContext->VSSetShader(m_pDebugVertexShader, nullptr, 0);
	pContext->PSSetShader(m_pDebugPixelShader, nullptr, 0);

	ID3D11ShaderResourceView* ppSRVNull[1] = { nullptr };
	ID3D11ShaderResourceView* srv = m_pVoxelisedScene->GetShaderResourceView();
	pContext->PSSetShaderResources(0, 1, &srv);

	XMMATRIX mViewM = XMMatrixTranspose(mView);
	XMMATRIX mProjectionM = XMMatrixTranspose(mProjection);

	for (int x = 0; x < TEXTURE_DIMENSION; x++)
	{
		for (int y = 0; y < TEXTURE_DIMENSION; y++)
		{
			for (int z = 0; z < TEXTURE_DIMENSION; z++)
 			{
				AABB cube;
				cube.Min = XMFLOAT3((x * 2) - 1, (y * 2) - 1, (z * 2) - 1);
				cube.Max = XMFLOAT3((x * 2) + 1, (y * 2) + 1, (z * 2) + 1);
				if (pCamera->CheckBoundingBoxInsideViewFrustum(cube))
				{
					XMMATRIX mWorldMat = XMMatrixTranslation(x *2.f, y *2.f, z *2.f);
					int coords[3] = { x, y, z };
					SetDebugShaderParams(pContext, mWorldMat, mViewM, mProjectionM, coords); //Needs to change world matrix for every cube
					m_arrDebugRenderCube->RenderBuffers(0, pContext);
					pContext->DrawIndexed(m_arrDebugRenderCube->GetMeshArray()[0]->m_iIndexCount, 0, 0);
				}
			}
		}
 	}

	pContext->PSSetShaderResources(0, 1, ppSRVNull);
	pContext->VSSetShader(nullptr, nullptr, 0);
	pContext->PSSetShader(nullptr, nullptr, 0);
}

void VoxelisePass::RenderMesh(ID3D11DeviceContext3* pDeviceContext, const XMMATRIX& mWorld, const XMMATRIX& mView, const XMMATRIX& mProjection, const XMFLOAT3& eyePos, Mesh* pMesh)
{
	ID3D11ShaderResourceView* ppSRVNull[1] = { nullptr };
	
	for (int i = 0; i < pMesh->GetMeshArray().size(); i++)
	{
		if (!pMesh->GetMeshArray()[i]->m_pMaterial->UsesAlphaMaps())
		{
			SetVoxeliseShaderParams(pDeviceContext, mWorld, mView, mProjection, eyePos);
			ID3D11ShaderResourceView* SRVDiffuseTex = pMesh->GetMeshArray()[i]->m_pMaterial->GetDiffuseTexture()->GetShaderResourceView();
			pDeviceContext->PSSetShaderResources(0, 1, &SRVDiffuseTex);
			pMesh->RenderBuffers(i, pDeviceContext);
			int indexCount = pMesh->GetMeshArray()[i]->m_iIndexCount;
			pDeviceContext->DrawIndexed(indexCount, 0, 0);
			pDeviceContext->PSSetShaderResources(0, 1, ppSRVNull);
		}
	}
	PostRender(pDeviceContext);
}

bool VoxelisePass::SetVoxeliseShaderParams(ID3D11DeviceContext3* pDeviceContext, const XMMATRIX& mWorld, const XMMATRIX& mView, const XMMATRIX& mProjection, const XMFLOAT3& eyePos)
{

	pDeviceContext->IASetInputLayout(m_pLayout);
	pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	pDeviceContext->GSSetShader(m_pGeometryShader, nullptr, 0);
	pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

	pDeviceContext->RSSetState(m_pRasteriserState);
	pDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

	pDeviceContext->RSSetViewports(1, &m_pVoxeliseViewport);

	D3D11_MAPPED_SUBRESOURCE mappedResource;

	HRESULT result = pDeviceContext->Map(m_pVoxeliseVertexShaderBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(result))
	{
		VoxeliseVertexShaderBuffer* pBuffer = static_cast<VoxeliseVertexShaderBuffer*>(mappedResource.pData);
		
		pBuffer->mWorld = mWorld;
		pBuffer->mWorldView = m_mWorldToVoxelGrid;

		pBuffer->mWorldViewProj = m_mWorldToVoxelGrid * XMMatrixTranspose(XMMatrixOrthographicLH(2.f, 2.f, 1.f, -1.f));
		pBuffer->mWorldInverseTranspose = XMMatrixTranspose(XMMatrixInverse(&XMMatrixDeterminant(mWorld), mWorld));
		pBuffer->mAxisProjections[0] = m_mViewProjMatrices[0];
		pBuffer->mAxisProjections[1] = m_mViewProjMatrices[1];
		pBuffer->mAxisProjections[2] = m_mViewProjMatrices[2];

		pDeviceContext->Unmap(m_pVoxeliseVertexShaderBuffer, 0);
	}
	pDeviceContext->VSSetConstantBuffers(0, 1, &m_pVoxeliseVertexShaderBuffer);
	pDeviceContext->GSSetConstantBuffers(0, 1, &m_pVoxeliseVertexShaderBuffer);

	pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);

	ID3D11UnorderedAccessView* uav = m_pVoxelisedScene->GetUAV();
	pDeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 1, &uav, 0);

	return true;
}

bool VoxelisePass::SetDebugShaderParams(ID3D11DeviceContext* pDeviceContext, const XMMATRIX& mWorld, const XMMATRIX& mView, const XMMATRIX& mProjection, int coord[3])
{
	XMMATRIX mWorldM = XMMatrixTranspose(mWorld);	

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT result = pDeviceContext->Map(m_pMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(result))
	{
		MatrixBuffer* pBuffer = static_cast<MatrixBuffer*>(mappedResource.pData);
		pBuffer->world = mWorldM;
		pBuffer->view = mView;
		pBuffer->projection = mProjection;

		pDeviceContext->Unmap(m_pMatrixBuffer, 0);
	}
	pDeviceContext->VSSetConstantBuffers(0, 1, &m_pMatrixBuffer);

	result = pDeviceContext->Map(m_pPerCubeDebugBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(result))
	{
		PerCubeDebugBuffer* pBuffer = static_cast<PerCubeDebugBuffer*>(mappedResource.pData);
		pBuffer->volumeCoord[0] = coord[0];
		pBuffer->volumeCoord[1] = coord[1];
		pBuffer->volumeCoord[2] = coord[2];
		pBuffer->padding = 0.f;

		pDeviceContext->Unmap(m_pPerCubeDebugBuffer, 0);
	}

	pDeviceContext->PSSetConstantBuffers(0, 1, &m_pPerCubeDebugBuffer);

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
	delete m_arrDebugRenderCube;
	m_arrDebugRenderCube = nullptr;
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

