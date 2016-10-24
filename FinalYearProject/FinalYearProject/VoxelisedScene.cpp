#include "VoxelisedScene.h"
#include "Debugging.h"


//Defines for compute shaders..
#define NUM_THREADS 16
#define NUM_GROUPS 16

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoxelisedScene::VoxelisedScene()
	:m_iDebugMipLevel(0)
#if TILED_RESOURCES
	, m_iCurrentOccupationTexture(0)
#endif
{
	m_sNumThreads = std::to_string(NUM_THREADS);

	m_sNumTexelsPerThread = std::to_string((TEXTURE_DIMENSION * TEXTURE_DIMENSION * TEXTURE_DIMENSION) / (NUM_THREADS * NUM_THREADS * NUM_GROUPS * NUM_GROUPS));
	m_ComputeShaderDefines[csdNumThreads].Name = "NUM_THREADS";
	m_ComputeShaderDefines[csdNumThreads].Definition = m_sNumThreads.c_str();
	m_ComputeShaderDefines[csdNumTexelsPerThread].Name = "NUM_TEXELS_PER_THREAD";
	m_ComputeShaderDefines[csdNumTexelsPerThread].Definition = m_sNumTexelsPerThread.c_str();

	m_sNumGroups = std::to_string(NUM_GROUPS);
	m_ComputeShaderDefines[csdNumGroups].Name = "NUM_GROUPS";
	m_ComputeShaderDefines[csdNumGroups].Definition = m_sNumGroups.c_str();
	m_ComputeShaderDefines[csdNulls].Name = nullptr;
	m_ComputeShaderDefines[csdNulls].Definition = nullptr;
#if TILED_RESOURCES
	for (int z = 0; z < TEXTURE_DIMENSION / 16; z++)
	{
		for (int y = 0; y < TEXTURE_DIMENSION / 32; y++)
		{
			for (int x = 0; x < TEXTURE_DIMENSION / 32; x++)
			{
				m_bPreviousFrameOccupation[z][y][x] = false;
			}
		}
	}

	for (int i = 1; i < MIP_LEVELS; i++)
	{
		int mult = std::pow(2, i);
		int numTilesInMip = ((TEXTURE_DIMENSION / 16) / (mult)) * ((TEXTURE_DIMENSION / 32) / mult) * ((TEXTURE_DIMENSION / 32) / mult);
		m_bPreviousFrameOccupationMipLevels[i-1] = new bool[numTilesInMip];
		for (int j = 0; j < numTilesInMip; j++)
		{
			m_bPreviousFrameOccupationMipLevels[i - 1][j] = false;
		}
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoxelisedScene::~VoxelisedScene()
{
	Shutdown();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT VoxelisedScene::Initialise(ID3D11Device3* pDevice, ID3D11DeviceContext3* pContext, HWND hwnd, const AABB& voxelGridAABB)
{
	CreateWorldToVoxelGrid(voxelGridAABB);
	
	InitialiseDebugBuffers(pDevice);

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

	D3D11_BUFFER_DESC debugBufferDesc;
	//Setup the description of the dynamic matrix constant buffer that is in the shader..
	debugBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	debugBufferDesc.ByteWidth = sizeof(DebugRenderBuffer);
	debugBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	debugBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	debugBufferDesc.MiscFlags = 0;
	debugBufferDesc.StructureByteStride = 0;

	//Create the buffer so we can access it from within this class
	result = pDevice->CreateBuffer(&debugBufferDesc, NULL, &m_pMatrixBuffer);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create matrix buffer");
		return false;
	}
	UINT MiscFlags = 0;
#if TILED_RESOURCES
	MiscFlags = D3D11_RESOURCE_MISC_TILED;
	//Need 3 of these for triple buffering to avoid gpu syncs flushing the pipeline and stalling everything
	for (int i = 0; i < 3; i++)
	{
		m_pTileOccupation[i] = new Texture3D;
		m_pTileOccupation[i]->Init(pDevice, pContext, TEXTURE_DIMENSION / 32.f, TEXTURE_DIMENSION / 32.f, TEXTURE_DIMENSION / 16.f, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE, 0, 0);
	}
	D3D11_TEXTURE3D_DESC textureDesc;
	m_pTileOccupation[0]->GetTexture()->GetDesc(&textureDesc);
	textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	textureDesc.Usage = D3D11_USAGE_STAGING;
	textureDesc.BindFlags = 0;
	result = pDevice->CreateTexture3D(&textureDesc, nullptr, &m_pTileOccupationStaging);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create tile occupation staging texture")
		return false;
	}

#endif

#if SPARSE_VOXEL_OCTREES
	InitialiseOctreeData();
#endif
	m_pVoxelisedSceneColours = new Texture3D;
	m_pVoxelisedSceneColours->Init(pDevice, pContext, TEXTURE_DIMENSION, TEXTURE_DIMENSION, TEXTURE_DIMENSION, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE, 0, MiscFlags);

	m_pVoxelisedSceneNormals = new Texture3D;
	m_pVoxelisedSceneNormals->Init(pDevice, pContext, TEXTURE_DIMENSION, TEXTURE_DIMENSION, TEXTURE_DIMENSION, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE, 0, MiscFlags);



	float mipScale = 1;
	for (int i = 0; i < MIP_LEVELS; i++)
	{
		m_pRadianceVolumeMips[i] = new Texture3D;
		m_pRadianceVolumeMips[i]->Init(pDevice, pContext, TEXTURE_DIMENSION*mipScale, TEXTURE_DIMENSION*mipScale, TEXTURE_DIMENSION*mipScale, MIP_LEVELS, DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE, 0, MiscFlags);
		mipScale *= 0.5f;
		MiscFlags = 0; //TODO: NEED TO FIX SO WE CAN DO ALL THE MIPS WITHIN ONE TEXTURE, FOR NOW THIS MAKES THE MIP LEVELS NONE TILED FOR THE COPYING..
	}

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
	sampDesc.MaxAnisotropy = 16;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	result = pDevice->CreateSamplerState(&sampDesc, &m_pSamplerState);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create sampler");
		return result;
	}

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelisedScene::RenderClearVoxelsPass(ID3D11DeviceContext* pContext)
{
	ID3D11UnorderedAccessView* ppUAViewNULL[2] = { nullptr, nullptr };

	pContext->CSSetShader(m_pClearVoxelsComputeShader, nullptr, 0);

	ID3D11UnorderedAccessView* uavs[2] = { m_pVoxelisedSceneColours->GetUAV(), m_pVoxelisedSceneNormals->GetUAV() };
	pContext->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);
	pContext->Dispatch(NUM_GROUPS, NUM_GROUPS, 1);
	pContext->CSSetShader(nullptr, nullptr, 0);
	pContext->CSSetUnorderedAccessViews(0, 2, ppUAViewNULL, nullptr);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelisedScene::RenderInjectRadiancePass(ID3D11DeviceContext* pContext)
{
	ID3D11UnorderedAccessView* ppUAViewNULL[1] = { nullptr };
	ID3D11ShaderResourceView* ppSRVNull[2] = { nullptr, nullptr };

	pContext->CSSetShader(m_pInjectRadianceComputeShader, nullptr, 0);
	ID3D11UnorderedAccessView* uav = m_pRadianceVolumeMips[0]->GetUAV();
	ID3D11ShaderResourceView* srvs[2] = { m_pVoxelisedSceneColours->GetShaderResourceView(), m_pVoxelisedSceneNormals->GetShaderResourceView() };
	pContext->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
	
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
	pContext->CSSetShaderResources(0, 2, srvs);
	pContext->CSGetShaderResources(2, NUM_LIGHTS, pShadowCubeArray);
	
	ID3D11Buffer* pLightBuffer = LightManager::Get()->GetLightBuffer();
	pContext->CSSetConstantBuffers(0, 1, &pLightBuffer);
	pContext->CSSetConstantBuffers(1, 1, &m_pVoxeliseVertexShaderBuffer);
	pContext->Dispatch(NUM_GROUPS, NUM_GROUPS, 1);


	//Reset
	pContext->CSSetShader(nullptr, nullptr, 0);
	pContext->CSSetUnorderedAccessViews(0, 1, ppUAViewNULL, nullptr);
	pContext->CSSetShaderResources(0, 2, ppSRVNull);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelisedScene::GenerateMips(ID3D11DeviceContext* pContext)
{
	ID3D11UnorderedAccessView* ppUAViewNULL = nullptr;
	ID3D11ShaderResourceView* ppSRVNull = nullptr;

	pContext->CSSetShader(m_pGenerateMipsShader, nullptr, 0);
	for (int i = 1; i < MIP_LEVELS; i++)
	{
		ID3D11ShaderResourceView* pSrcVolume = m_pRadianceVolumeMips[i-1]->GetShaderResourceView();
		ID3D11UnorderedAccessView* pDestVolume = m_pRadianceVolumeMips[i]->GetUAV();

		pContext->CSSetUnorderedAccessViews(0, 1, &pDestVolume, nullptr);
		pContext->CSSetShaderResources(0, 1, &pSrcVolume);

		pContext->Dispatch(NUM_GROUPS, NUM_GROUPS, 1);

		pContext->CopySubresourceRegion(m_pRadianceVolumeMips[0]->GetTexture(), i, 0, 0, 0, m_pRadianceVolumeMips[i]->GetTexture(), 0, nullptr);
	}

	//Reset
	pContext->CSSetShader(nullptr, nullptr, 0);
	pContext->CSSetUnorderedAccessViews(0, 1, &ppUAViewNULL, nullptr);
	pContext->CSSetShaderResources(0, 1, &ppSRVNull);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelisedScene::RenderDebugCubes(ID3D11DeviceContext3* pContext, const XMMATRIX& mWorld, const XMMATRIX& mView, const XMMATRIX& mProjection, Camera* pCamera)
{
	m_pDebugRenderPass->SetActiveRenderPass(pContext);

	ID3D11ShaderResourceView* ppSRVNull[1] = { nullptr };
	ID3D11ShaderResourceView* srv;
	
	srv = m_pRadianceVolumeMips[0]->GetShaderResourceView();
	

	pContext->GSSetShaderResources(0, 1, &srv);

	XMMATRIX mWorldM = XMMatrixTranspose(mWorld);
	XMMATRIX mViewM = XMMatrixTranspose(mView);
	XMMATRIX mProjectionM = XMMatrixTranspose(mProjection);

	SetDebugShaderParams(pContext, mWorldM, mViewM, mProjectionM); //Needs to change world matrix for every cube			
				
	unsigned int stride;
	unsigned int offset;

	//Set vertex buffer stride and offset.
	stride = sizeof(DebugCubesVertexType);
	offset = 0;

	// Set the vertex buffer to active in the input assembler so it can be rendered.
	pContext->IASetVertexBuffers(0, 1, &m_pDebugCubesVertexBuffer, &stride, &offset);

	// Set the index buffer to active in the input assembler so it can be rendered.
	pContext->IASetIndexBuffer(m_pDebugCubesIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// Set the type of primitive that should be rendered from this vertex buffer, in this case triangles.
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
				
	pContext->DrawIndexed(TEXTURE_DIMENSION*TEXTURE_DIMENSION*TEXTURE_DIMENSION, 0, 0);
	

	pContext->PSSetShaderResources(0, 1, ppSRVNull);
	pContext->VSSetShader(nullptr, nullptr, 0);
	pContext->PSSetShader(nullptr, nullptr, 0);
	pContext->GSSetShader(nullptr, nullptr, 0);
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
	m_pVoxeliseScenePass->SetActiveRenderPass(pDeviceContext);

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

#if TILED_RESOURCES
	//Necessary for triple buffering to avoid gpu syncs
	m_iCurrentOccupationTexture = (m_iCurrentOccupationTexture + 1) % 3;
	ID3D11UnorderedAccessView* uavs[3] = { m_pVoxelisedSceneColours->GetUAV(), m_pVoxelisedSceneNormals->GetUAV(), m_pTileOccupation[m_iCurrentOccupationTexture]->GetUAV() };
	pDeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 3, uavs, 0);
#else
	ID3D11UnorderedAccessView* uavs[2] = { m_pVoxelisedSceneColours->GetUAV(), m_pVoxelisedSceneNormals->GetUAV() };
	pDeviceContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 2, uavs, 0);
#endif

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool VoxelisedScene::SetDebugShaderParams(ID3D11DeviceContext* pDeviceContext, const XMMATRIX& mWorld, const XMMATRIX& mView, const XMMATRIX& mProjection)
{
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT result = pDeviceContext->Map(m_pMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (SUCCEEDED(result))
	{
		DebugRenderBuffer* pBuffer = static_cast<DebugRenderBuffer*>(mappedResource.pData);
		pBuffer->world = mWorld;
		pBuffer->view = mView;
		pBuffer->projection = mProjection;
		pBuffer->DebugMipLevel = m_iDebugMipLevel;
		pBuffer->padding[0] = 0;
		pBuffer->padding[1] = 0;
		pBuffer->padding[2] = 0;

		pDeviceContext->Unmap(m_pMatrixBuffer, 0);
	}
	pDeviceContext->GSSetConstantBuffers(0, 1, &m_pMatrixBuffer);

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

	ID3D11UnorderedAccessView* ppUAViewNULL[3] = { nullptr, nullptr, nullptr };
	pContext->OMSetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, 0, 3, ppUAViewNULL, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelisedScene::Shutdown()
{
	if (m_pVoxeliseScenePass)
	{
		m_pVoxeliseScenePass->Shutdown();
		delete m_pVoxeliseScenePass;
		m_pVoxeliseScenePass = nullptr;
	}
	if (m_pDebugRenderPass)
	{
		m_pDebugRenderPass->Shutdown();
		delete m_pDebugRenderPass;
		m_pDebugRenderPass = nullptr;
	}
	if (m_pClearVoxelsComputeShader)
	{
		m_pClearVoxelsComputeShader->Release();
		m_pClearVoxelsComputeShader = nullptr;
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
	for (int i = 0; i < MIP_LEVELS; i++)
	{
		delete m_pRadianceVolumeMips[i];
		m_pRadianceVolumeMips[i] = nullptr;
	}
	
#if TILED_RESOURCES
	for (int i = 0; i < MIP_LEVELS - 1; i++)
	{
		delete[] m_bPreviousFrameOccupationMipLevels[i];
		m_bPreviousFrameOccupationMipLevels[i] = nullptr;
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelisedScene::GetRadianceVolumes(ID3D11ShaderResourceView* volumes[MIP_LEVELS])
{
	for (int i = 0; i < MIP_LEVELS; i++)
	{
		volumes[i] = m_pRadianceVolumeMips[i]->GetShaderResourceView();
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelisedScene::Update(ID3D11DeviceContext3* pDeviceContext)
{
#if TILED_RESOURCES
	UpdateTiles(pDeviceContext);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if TILED_RESOURCES
void VoxelisedScene::UpdateTiles(ID3D11DeviceContext3* pContext)
{
	int iFrameMinus2TileOccupation = (m_iCurrentOccupationTexture + 1) % 3;
	pContext->CopySubresourceRegion(m_pTileOccupationStaging, 0, 0, 0, 0, m_pTileOccupation[iFrameMinus2TileOccupation]->GetTexture(), 0, nullptr);
	
	D3D11_MAPPED_SUBRESOURCE pTexture;
	HRESULT res = pContext->Map(m_pTileOccupationStaging, 0, D3D11_MAP_READ, 0, &pTexture);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Couldn't map resource..")
		return;
	}
	UINT* pTexData = static_cast<UINT*>(pTexture.pData);

	UINT num = pTexData[0];
	int index = 0;
	for (int z = 0; z < TEXTURE_DIMENSION / 16; z++)
	{
		for (int y = 0; y < TEXTURE_DIMENSION / 32; y++)
		{
			index = z * (pTexture.DepthPitch/4) + y * (pTexture.RowPitch/4);
			for (int x = 0; x < TEXTURE_DIMENSION / 32; x++)
			{
				UINT* TexData = &pTexData[index];
				if (pTexData[index] > 0)
				{
					if (!m_bPreviousFrameOccupation[z][y][x])
					{
						m_pRadianceVolumeMips[0]->MapTile(pContext, x, y, z, 0);
						m_pVoxelisedSceneColours->MapTile(pContext, x, y, z, 0);
						m_pVoxelisedSceneNormals->MapTile(pContext, x, y, z, 0);
						for (int i = 1; i < MIP_LEVELS; i++)
						{
							int mult = std::pow(2, i);
							int mipZ = z / mult;
							int mipY = y / mult;
							int mipX = x / mult;
							int mipIdx = (mipZ * ((TEXTURE_DIMENSION / 32) / mult) * ((TEXTURE_DIMENSION / 32) / mult)) + mipY * ((TEXTURE_DIMENSION / 32) / mult) + mipX;
							if (!m_bPreviousFrameOccupationMipLevels[i-1][mipIdx])
							{
								m_pRadianceVolumeMips[0]->MapTile(pContext, mipX, mipY, mipZ, i);
								m_bPreviousFrameOccupationMipLevels[i-1][mipIdx] = true;
							}
						}
					}
					m_bPreviousFrameOccupation[z][y][x] = true;
				}
				else
				{
					//TODO: and unmap the tile..
					m_bPreviousFrameOccupation[z][y][x] = false;
				}
				index++;
			}
		}
	}
	pContext->Unmap(m_pTileOccupationStaging, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
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
	
	ID3D10Blob* pComputeShaderBuffer(nullptr);
	ID3D10Blob* pRadianceComputeShaderBuffer(nullptr);
	ID3D10Blob* pGenerateMipsShaderBuffer(nullptr);

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


	//Compile the clear voxels compute shader code
	result = D3DCompileFromFile(L"InjectRadiance.hlsl", m_ComputeShaderDefines, nullptr, "CSInjectRadiance", "cs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, &pRadianceComputeShaderBuffer, &pErrorMessage);
	if (FAILED(result))
	{
		if (pErrorMessage)
		{
			//If the shader failed to compile it should have written something to error message, so we output that here
			OutputShaderErrorMessage(pErrorMessage, hwnd, L"InjectRadiance.hlsl");
		}
		else
		{
			//if it hasn't, then it couldn't find the shader file..
			MessageBox(hwnd, L"InjectRadiance.hlsl", L"Missing Shader File", MB_OK);
		}
		return false;
	}
	result = pDevice->CreateComputeShader(pRadianceComputeShaderBuffer->GetBufferPointer(), pRadianceComputeShaderBuffer->GetBufferSize(), nullptr, &m_pInjectRadianceComputeShader);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create the compute shader.");
		return false;
	}


	//Compile the clear voxels compute shader code
	result = D3DCompileFromFile(L"GenerateMips.hlsl", m_ComputeShaderDefines, nullptr, "CSGenerateMips", "cs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, &pGenerateMipsShaderBuffer, &pErrorMessage);
	if (FAILED(result))
	{
		if (pErrorMessage)
		{
			//If the shader failed to compile it should have written something to error message, so we output that here
			OutputShaderErrorMessage(pErrorMessage, hwnd, L"GenerateMips.hlsl");
		}
		else
		{
			//if it hasn't, then it couldn't find the shader file..
			MessageBox(hwnd, L"GenerateMips.hlsl", L"Missing Shader File", MB_OK);
		}
		return false;
	}
	result = pDevice->CreateComputeShader(pGenerateMipsShaderBuffer->GetBufferPointer(), pGenerateMipsShaderBuffer->GetBufferSize(), nullptr, &m_pGenerateMipsShader);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create the compute shader.");
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

	m_pVoxeliseScenePass = new RenderPass;
	m_pVoxeliseScenePass->Initialise(pDevice, hwnd, polyLayout, iNumElements, L"Voxelise_Populate.hlsl", "VSMain", "GSMain", "PSMain");
	
	m_pDebugRenderPass = new RenderPass;
	m_pDebugRenderPass->Initialise(pDevice, hwnd, polyLayout, 1, L"VoxelRenderShader.hlsl", "VSMain", "GSMain", "PSMain");

	//Finished with shader buffers now so they can be released
	pErrorMessage->Release();
	pErrorMessage = nullptr;

	pComputeShaderBuffer->Release();
	pComputeShaderBuffer = nullptr;

	pRadianceComputeShaderBuffer->Release();
	pRadianceComputeShaderBuffer = nullptr;

	pGenerateMipsShaderBuffer->Release();
	pGenerateMipsShaderBuffer = nullptr;
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

bool VoxelisedScene::InitialiseDebugBuffers(ID3D11Device* pDevice)
{
	DebugCubesVertexType* verts;
	unsigned long* indices;

	D3D11_BUFFER_DESC vertexBufferDesc, indexBufferDesc;
	D3D11_SUBRESOURCE_DATA vertexData, indexData;
	HRESULT result;

	verts = new DebugCubesVertexType[TEXTURE_DIMENSION*TEXTURE_DIMENSION*TEXTURE_DIMENSION];
	if (!verts)
	{
		VS_LOG_VERBOSE("Unable to allocate debug cubes vertices buffer");
		return false;
	}

	indices = new unsigned long[TEXTURE_DIMENSION*TEXTURE_DIMENSION*TEXTURE_DIMENSION];
	if (!indices)
	{
		VS_LOG_VERBOSE("Unable to allocate debug cubes indices buffer");
		return false;
	}

	int idx = 0;
	for (int x = 0; x < TEXTURE_DIMENSION; x++)
	{
		for (int y = 0; y < TEXTURE_DIMENSION; y++)
		{
			for (int z = 0; z < TEXTURE_DIMENSION; z++)
			{
				verts[idx].position = XMFLOAT3(x, y, z);
				indices[idx] = idx;
				idx++;
			}
		}
	}

	//Setup the description of the static vertex buffer.
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(DebugCubesVertexType) * idx;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	//Give the subresource struct a pointer to the vert data
	vertexData.pSysMem = verts;
	vertexData.SysMemPitch = 0;
	vertexData.SysMemSlicePitch = 0;

	//Create the Vertex buffer..
	result = pDevice->CreateBuffer(&vertexBufferDesc, &vertexData, &m_pDebugCubesVertexBuffer);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create vertex buffer");
		return false;
	}

	//Setup the description of the static index buffer
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(unsigned long) * idx;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;

	//Give the subresource a pointer to the index data.
	indexData.pSysMem = indices;
	indexData.SysMemPitch = 0;
	indexData.SysMemSlicePitch = 0;

	//Create the index buffer
	result = pDevice->CreateBuffer(&indexBufferDesc, &indexData, &m_pDebugCubesIndexBuffer);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create index buffer");
		return false;
	}

	//Release the arrays now that the vertex and index buffers have been created and loaded..
	delete[] verts;
	verts = nullptr;

	delete[] indices;
	indices = nullptr;

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if SPARSE_VOXEL_OCTREES
bool VoxelisedScene::InitialiseOctreeData(ID3D11Device3* pDevice, ID3D11DeviceContext3* pContext)
{
	m_pSparseVoxelOctreeBricks = new Texture3D;
	m_pSparseVoxelOctreeBricks->Init(pDevice, pContext, TEXTURE_DIMENSION, TEXTURE_DIMENSION, TEXTURE_DIMENSION, 1, DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE, 0, 0);

	D3D11_BUFFER_DESC descGPUBuffer;
	ZeroMemory(&descGPUBuffer, sizeof(descGPUBuffer));
	descGPUBuffer.BindFlags = D3D11_BIND_UNORDERED_ACCESS |	D3D11_BIND_SHADER_RESOURCE;
	descGPUBuffer.ByteWidth = TEXTURE_DIMENSION * sizeof(OctreeNode);
	descGPUBuffer.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	descGPUBuffer.StructureByteStride = 0; 

	if (FAILED(pDevice->CreateBuffer(&descGPUBuffer, nullptr, &m_pSparseVoxelOctree)))
	{
		VS_LOG_VERBOSE("Failed to create structured buffer");
		return false;
	}

	D3D11_UNORDERED_ACCESS_VIEW_DESC descView;
	ZeroMemory(&descView, sizeof(descView));
	descView.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	descView.Buffer.FirstElement = 0;
	// Format must be must be DXGI_FORMAT_UNKNOWN, when creating 
	// a View of a Structured Buffer
	descView.Format = DXGI_FORMAT_UNKNOWN;
	descView.Buffer.NumElements = TEXTURE_DIMENSION;
	descView.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
	if (FAILED(pDevice->CreateUnorderedAccessView(m_pSparseVoxelOctree, &descView, &m_pSVOUAV)))
	{
		VS_LOG_VERBOSE("Failed to create structured buffer uav");
		return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
	ZeroMemory(&descView, sizeof(descView));
	descSRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	descSRV.BufferEx.FirstElement = 0;
	descSRV.Format = DXGI_FORMAT_UNKNOWN;
	descSRV.BufferEx.NumElements =TEXTURE_DIMENSION;

	if (FAILED(pDevice->CreateShaderResourceView(m_pSparseVoxelOctree, &descSRV, &m_pSVOSRV)))
	{
		VS_LOG_VERBOSE("Failed to create structured buffer srv");
		return false;
	}
}
#endif
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////