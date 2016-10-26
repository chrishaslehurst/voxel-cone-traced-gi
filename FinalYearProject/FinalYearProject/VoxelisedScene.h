#ifndef VOXELISE_PASS_H
#define VOXELISE_PASS_H

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <d3d11_3.h>
#include <DirectXMath.h>
#include <D3DCompiler.h>
#include <fstream>
#include "AABB.h"
#include "Mesh.h"
#include "Texture2D.h"
#include "Texture3D.h"
#include "RenderPass.h"

#define TEXTURE_DIMENSION 256
#define MIP_LEVELS 4


#define TILED_RESOURCES 1
#define SPARSE_VOXEL_OCTREES 0

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace DirectX;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum ComputeShaderDefines
{
	csdNumTexelsPerThread,
	csdNumThreads,
	csdNumGroups,
	csdNulls
};

class VoxelisedScene
{
public:

	__declspec(align(16)) struct VoxeliseVertexShaderBuffer
	{
		
		XMMATRIX mWorld;
		XMMATRIX mWorldToVoxelGrid;
		XMMATRIX mWorldToVoxelGridProj;
		XMMATRIX mWorldInverseTranspose;
		XMMATRIX mAxisProjections[3];
		
		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_mm_free(p);
		}
	};

	__declspec(align(16)) struct DebugRenderBuffer
	{
		XMMATRIX world;
		XMMATRIX view;
		XMMATRIX projection;
		int DebugMipLevel;
		int padding[3];

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

		void operator delete(void* p)
		{
			_mm_free(p);
		}
	};

	__declspec(align(16)) struct DebugCubesVertexType
	{
		XMFLOAT3 position;

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_mm_free(p);
		}
	};

	VoxelisedScene();
	~VoxelisedScene();

	HRESULT Initialise(ID3D11Device3* pDevice, ID3D11DeviceContext3* pContext, HWND hwnd, const AABB& voxelGridAABB);
	void RenderClearVoxelsPass(ID3D11DeviceContext* pContext);
	void RenderInjectRadiancePass(ID3D11DeviceContext* pContext);
	void GenerateMips(ID3D11DeviceContext* pContext);
	void RenderDebugCubes(ID3D11DeviceContext3* pContext, const XMMATRIX& mWorld, const XMMATRIX& mView, const XMMATRIX& mProjection, Camera* pCamera);
	
	void RenderMesh(ID3D11DeviceContext3* pDeviceContext, const XMMATRIX& mWorld, const XMMATRIX& mView, const XMMATRIX& mProjection, const XMFLOAT3& eyePos, Mesh* pVoxelise);
	bool SetVoxeliseShaderParams(ID3D11DeviceContext3* pDeviceContext, const XMMATRIX& mWorld, const XMMATRIX& mView, const XMMATRIX& mProjection, const XMFLOAT3& eyePos);
	bool SetDebugShaderParams(ID3D11DeviceContext* pDeviceContext, const XMMATRIX& mWorld, const XMMATRIX& mView, const XMMATRIX& mProjection);

	bool Render(ID3D11DeviceContext* pDeviceContext, int iIndexCount);
	void PostRender(ID3D11DeviceContext* pContext);
	void Shutdown();

	void IncreaseDebugMipLevel() { if (m_iDebugMipLevel < MIP_LEVELS-1) { m_iDebugMipLevel++; } }
	void DecreaseDebugMipLevel() { if (m_iDebugMipLevel > 0) { m_iDebugMipLevel--; } }

	void GetRadianceVolumes(ID3D11ShaderResourceView* volumes[MIP_LEVELS]);
	const XMMATRIX& GetWorldToVoxelMatrix() { return m_mWorldToVoxelGrid; }

	float GetVoxelScale() { return m_vVoxelGridSize.x / TEXTURE_DIMENSION; } //size of one voxel

	void Update(ID3D11DeviceContext3* pDeviceContext);

	void UnmapAllTiles(ID3D11DeviceContext3* pDeviceContext);
private:
	void UpdateTiles(ID3D11DeviceContext3* pDeviceContext);
	

	void CreateWorldToVoxelGrid(const AABB& voxelGridAABB);
	HRESULT InitialiseShadersAndInputLayout(ID3D11Device3* pDevice, ID3D11DeviceContext* pContext, HWND hwnd);
	void OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename);
	bool InitialiseDebugBuffers(ID3D11Device* pDevice);

	
	RenderPass*				m_pVoxeliseScenePass;
	RenderPass*				m_pDebugRenderPass;

	ID3D11ComputeShader*	m_pClearVoxelsComputeShader;
	ID3D11ComputeShader*	m_pInjectRadianceComputeShader;
	ID3D11ComputeShader*	m_pGenerateMipsShader;

	
	ID3D11Buffer*			m_pVoxeliseVertexShaderBuffer;
	ID3D11Buffer*			m_pMatrixBuffer;

	ID3D11RasterizerState2* m_pRasteriserState;
	D3D11_VIEWPORT m_pVoxeliseViewport;
	ID3D11SamplerState* m_pSamplerState;

	Texture3D* m_pVoxelisedSceneColours;
	Texture3D* m_pVoxelisedSceneNormals;
	//TODO: Generate mips without needing the array - do it directly to mip level in the shader
	Texture3D* m_pRadianceVolumeMips[MIP_LEVELS];

	XMMATRIX m_mViewProjMatrices[3];
	XMFLOAT3 m_vVoxelGridSize;
	XMFLOAT3 m_vVoxelGridMin;
	XMFLOAT3 m_vVoxelGridMax;

	XMMATRIX m_mWorldToVoxelGrid;

	D3D_SHADER_MACRO m_ComputeShaderDefines[4];

	std::string m_sNumThreads;
	std::string m_sNumTexelsPerThread;
	std::string m_sNumGroups;

	ID3D11Buffer*			m_pDebugCubesVertexBuffer;
	ID3D11Buffer*			m_pDebugCubesIndexBuffer;

	int m_iDebugMipLevel;

#if TILED_RESOURCES
	int m_iCurrentOccupationTexture;
	Texture3D* m_pTileOccupation[3];
	ID3D11Texture3D* m_pTileOccupationStaging;

	bool m_bPreviousFrameOccupation[TEXTURE_DIMENSION / 16][TEXTURE_DIMENSION / 32][TEXTURE_DIMENSION / 32];
	bool *m_bPreviousFrameOccupationMipLevels[MIP_LEVELS - 1];
#endif

#if SPARSE_VOXEL_OCTREES
	
	struct OctreeNode
	{
		int childDataIndex[3]; //points to the 2x2x2 brick in the Texture.
		int childNodeIndex; //points to the 2x2x2 nodes held later in the buffer
	};

	ID3D11Buffer* m_pSparseVoxelOctree; //Structured buffer to hold the octree nodes..
	ID3D11UnorderedAccessView* m_pSVOUAV;
	ID3D11ShaderResourceView* m_pSVOSRV;
	Texture3D* m_pSparseVoxelOctreeBricks; //2x2x2 bricks to hold SVO data, indexed into by the octree

	bool InitialiseOctreeData(ID3D11Device3* pDevice, ID3D11DeviceContext3* pContext);
#endif 
};

#endif
