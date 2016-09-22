#include "VoxelisedScene.h"
#include "Debugging.h"


//Defines for compute shaders..
#define NUM_THREADS 8
#define NUM_GROUPS 2

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoxelisedScene::VoxelisedScene()
{

	m_sNumThreads = std::to_string(NUM_THREADS);
	m_sNumTexelsPerThread = std::to_string((TEXTURE_DIMENSION * TEXTURE_DIMENSION * TEXTURE_DIMENSION) / (NUM_THREADS * NUM_THREADS * NUM_GROUPS));
	m_ComputeShaderDefines[csdNumThreads].Name = "NUM_THREADS";
	m_ComputeShaderDefines[csdNumThreads].Definition = m_sNumThreads.c_str();
	m_ComputeShaderDefines[csdNumTexelsPerThread].Name = "NUM_TEXELS_PER_THREAD";
	m_ComputeShaderDefines[csdNumTexelsPerThread].Definition = m_sNumTexelsPerThread.c_str();
	m_ComputeShaderDefines[csdNulls].Name = nullptr;
	m_ComputeShaderDefines[csdNulls].Definition = nullptr;
}

VoxelisedScene::~VoxelisedScene()
{
	Shutdown();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT VoxelisedScene::Initialise(ID3D11Device3* pDevice, ID3D11DeviceContext* pContext, HWND hwnd, const AABB& voxelGridAABB)
{
	CreateWorldToVoxelGrid(voxelGridAABB);

	HRESULT result = InitialiseShadersAndInputLayout(pDevice, pContext, hwnd);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to initialise shaders and input layout");
		return result;
	}

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

	m_pVoxelisedSceneColours = new Texture3D;
	m_pVoxelisedSceneColours->Init(pDevice, TEXTURE_DIMENSION, TEXTURE_DIMENSION, TEXTURE_DIMENSION, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE);

	m_pVoxelisedSceneNormals = new Texture3D;
	m_pVoxelisedSceneNormals->Init(pDevice, TEXTURE_DIMENSION, TEXTURE_DIMENSION, TEXTURE_DIMENSION, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE);


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

	m_pDebugRenderCube = new Mesh;
	m_pDebugRenderCube->InitialiseCubeFromTxt(pDevice, pContext, hwnd);
	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelisedScene::RenderClearVoxelsPass(ID3D11DeviceContext* pContext)
{
	ID3D11UnorderedAccessView* ppUAViewNULL[1] = { nullptr };

	pContext->CSSetShader(m_pClearVoxelsComputeShader, nullptr, 0);

	ID3D11UnorderedAccessView* uavs[2] = { m_pVoxelisedSceneColours->GetUAV(), m_pVoxelisedSceneNormals->GetUAV() };
	pContext->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);
	pContext->Dispatch(NUM_GROUPS, NUM_GROUPS, 1);
	pContext->CSSetShader(nullptr, nullptr, 0);
	pContext->CSSetUnorderedAccessViews(0, 1, ppUAViewNULL, nullptr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelisedScene::RenderDebugCubes(ID3D11DeviceContext* pContext, const XMMATRIX& mWorld, const XMMATRIX& mView, const XMMATRIX& mProjection, Camera* pCamera)
{
	pContext->IASetInputLayout(m_pLayout);
	pContext->VSSetShader(m_pDebugVertexShader, nullptr, 0);
	pContext->PSSetShader(m_pDebugPixelShader, nullptr, 0);

	ID3D11ShaderResourceView* ppSRVNull[2] = { nullptr, nullptr };
	ID3D11ShaderResourceView* srvs[2] = { m_pVoxelisedSceneColours->GetShaderResourceView(), m_pVoxelisedSceneNormals->GetShaderResourceView() };
	pContext->PSSetShaderResources(0, 2, srvs);

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
					m_pDebugRenderCube->RenderBuffers(0, pContext);
					pContext->DrawIndexed(m_pDebugRenderCube->GetMeshArray()[0]->m_iIndexCount, 0, 0);
				}
			}
		}
 	}

	pContext->PSSetShaderResources(0, 2, ppSRVNull);
	pContext->VSSetShader(nullptr, nullptr, 0);
	pContext->PSSetShader(nullptr, nullptr, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelisedScene::RenderMesh(ID3D11DeviceContext3* pDeviceContext, const XMMATRIX& mWorld, const XMMATRIX& mView, const XMMATRIX& mProjection, const XMFLOAT3& eyePos, Mesh* pMesh)
{
	ID3D11ShaderResourceView* ppSRVNull[1] = { nullptr };
	SetVoxeliseShaderParams(pDeviceContext, mWorld, mView, mProjection, eyePos);
	for (int i = 0; i < pMesh->GetMeshArray().size(); i++)
	{
		if (!pMesh->GetMeshArray()[i]->m_pMaterial->UsesAlphaMaps())
		{
			
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool VoxelisedScene::SetVoxeliseShaderParams(ID3D11DeviceContext3* pDeviceContext, const XMMATRIX& mWorld, const XMMATRIX& mView, const XMMATRIX& mProjection, const XMFLOAT3& eyePos)
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
		pBuffer->mWorldToVoxelGrid = m_mWorldToVoxelGrid;

		pBuffer->mWorldToVoxelGridProj = m_mWorldToVoxelGrid * XMMatrixTranspose(XMMatrixOrthographicLH(2.f, 2.f, 1.f, -1.f));
		pBuffer->mWorldInverseTranspose = XMMatrixTranspose(XMMatrixInverse(&XMMatrixDeterminant(mWorld), mWorld));
		pBuffer->mAxisProjections[0] = m_mViewProjMatrices[0];
		pBuffer->mAxisProjections[1] = m_mViewProjMatrices[1];
		pBuffer->mAxisProjections[2] = m_mViewProjMatrices[2];

		pDeviceContext->Unmap(m_pVoxeliseVertexShaderBuffer, 0);
	}
	pDeviceContext->VSSetConstantBuffers(0, 1, &m_pVoxeliseVertexShaderBuffer);
	pDeviceContext->GSSetConstantBuffers(0, 1, &m_pVoxeliseVertexShaderBuffer);

	pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);

	ID3D11UnorderedAccessView* uavs[2] = { m_pVoxelisedSceneColours->GetUAV(), m_pVoxelisedSceneNormals->GetUAV() };
	pDeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 2, uavs, 0);

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool VoxelisedScene::SetDebugShaderParams(ID3D11DeviceContext* pDeviceContext, const XMMATRIX& mWorld, const XMMATRIX& mView, const XMMATRIX& mProjection, int coord[3])
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////


bool VoxelisedScene::Render(ID3D11DeviceContext* pDeviceContext, int iIndexCount)
{
	//Render the triangle
	pDeviceContext->DrawIndexed(iIndexCount, 0, 0);

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelisedScene::PostRender(ID3D11DeviceContext* pContext)
{
	pContext->VSSetShader(nullptr, nullptr, 0);
	pContext->GSSetShader(nullptr, nullptr, 0);
	pContext->PSSetShader(nullptr, nullptr, 0);

	ID3D11UnorderedAccessView* ppUAViewNULL[1] = { nullptr };
	pContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 1, ppUAViewNULL, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelisedScene::Shutdown()
{
	if (m_pVertexShader)
	{
		m_pVertexShader->Release();
		m_pVertexShader = nullptr;
	}
	if (m_pPixelShader)
	{
		m_pPixelShader->Release();
		m_pPixelShader = nullptr;
	}
	if (m_pGeometryShader)
	{
		m_pGeometryShader->Release();
		m_pGeometryShader = nullptr;
	}
	if (m_pClearVoxelsComputeShader)
	{
		m_pClearVoxelsComputeShader->Release();
		m_pClearVoxelsComputeShader = nullptr;
	}
	if (m_pDebugVertexShader)
	{
		m_pDebugVertexShader->Release();
		m_pDebugVertexShader = nullptr;
	}
	if (m_pDebugPixelShader)
	{
		m_pDebugPixelShader->Release();
		m_pDebugPixelShader = nullptr;
	}
	if (m_pLayout)
	{
		m_pLayout->Release();
		m_pLayout = nullptr;
	}
	if (m_pVoxeliseVertexShaderBuffer)
	{
		m_pVoxeliseVertexShaderBuffer->Release();
		m_pVoxeliseVertexShaderBuffer = nullptr;
	}
	if (m_pMatrixBuffer)
	{
		m_pMatrixBuffer->Release();
		m_pMatrixBuffer = nullptr;
	}
	if (m_pPerCubeDebugBuffer)
	{
		m_pPerCubeDebugBuffer->Release();
		m_pPerCubeDebugBuffer = nullptr;
	}
	if (m_pRasteriserState)
	{
		m_pRasteriserState->Release();
		m_pRasteriserState = nullptr;
	}

	if (m_pSamplerState)
	{
		m_pSamplerState->Release();
		m_pSamplerState = nullptr;
	}

	if (m_pVoxelisedSceneColours)
	{
		delete m_pVoxelisedSceneColours;
		m_pVoxelisedSceneColours = nullptr;
	}
	if (m_pVoxelisedSceneNormals)
	{
		delete m_pVoxelisedSceneNormals;
		m_pVoxelisedSceneNormals = nullptr;
	}
	if (m_pRadianceVolume)
	{
		delete m_pRadianceVolume;
		m_pRadianceVolume = nullptr;
	}
	if (m_pDebugRenderCube)
	{
		delete m_pDebugRenderCube;
		m_pDebugRenderCube = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelisedScene::CreateWorldToVoxelGrid(const AABB& voxelGridAABB)
{
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

	float scale = TEXTURE_DIMENSION / voxelGridSize;

	XMVECTOR translateToOrigin = (-Min - (VoxelGridSize * 0.5f));
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


	m_mWorldToVoxelGrid = XMMatrixTranspose(m_mWorldToVoxelGrid);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT VoxelisedScene::InitialiseShadersAndInputLayout(ID3D11Device3* pDevice, ID3D11DeviceContext* pContext, HWND hwnd)
{
	ID3D10Blob* pErrorMessage(nullptr);
	ID3D10Blob* pVertexShaderBuffer(nullptr);
	ID3D10Blob* pPixelShaderBuffer(nullptr);
	ID3D10Blob* pDebugVertexShaderBuffer(nullptr);
	ID3D10Blob* pDebugPixelShaderBuffer(nullptr);
	ID3D10Blob* pGeometryShaderBuffer(nullptr);
	ID3D10Blob* pComputeShaderBuffer(nullptr);

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

	pDebugPixelShaderBuffer->Release();
	pDebugPixelShaderBuffer = nullptr;

	pDebugVertexShaderBuffer->Release();
	pDebugVertexShaderBuffer = nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelisedScene::OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename)
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