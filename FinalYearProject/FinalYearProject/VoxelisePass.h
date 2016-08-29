#ifndef VOXELISE_PASS_H
#define VOXELISE_PASS_H

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <d3d11_3.h>
#include <DirectXMath.h>
#include <D3DCompiler.h>
#include <fstream>
#include "AABB.h"

#define TEXTURE_DIMENSION 64

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace DirectX;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class VoxelisePass
{
public:

	__declspec(align(16)) struct MatrixBuffer
	{
		XMMATRIX world;

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

		void operator delete(void* p)
		{
			_mm_free(p);
		}
	};

	__declspec(align(16)) struct ProjectionMatrixBuffer
	{
		XMMATRIX viewProjMatrices[3];
		XMFLOAT3 voxelGridSize; //The dimension in worldspace of the voxel volume...
		unsigned int VoxelTextureSize; //the texture resolution of the voxel grid.

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

		void operator delete(void* p)
		{
			_mm_free(p);
		}
	};

	__declspec(align(16)) struct VoxelGridBuffer
	{
		XMMATRIX mWorldToVoxelGrid;
		XMMATRIX mWorldToVoxelGridInverse;

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

		void operator delete(void* p)
		{
			_mm_free(p);
		}
	};

	HRESULT Initialise(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, HWND hwnd, AABB voxelGridAABB);
	void RenderClearVoxelsPass(ID3D11DeviceContext* pContext);
	bool SetShaderParams(ID3D11DeviceContext* pDeviceContext, const XMMATRIX& mWorld);
	bool Render(ID3D11DeviceContext* pDeviceContext, int iIndexCount);
	void PostRender(ID3D11DeviceContext* pContext);
	void Shutdown();

private:
	void OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename);

	ID3D11VertexShader*		m_pVertexShader;
	ID3D11PixelShader*		m_pPixelShader;
	ID3D11GeometryShader*	m_pGeometryShader;
	ID3D11ComputeShader*	m_pComputeShader;

	ID3D11InputLayout*		m_pLayout;
	ID3D11Buffer*			m_pMatrixBuffer;
	ID3D11Buffer*			m_pProjectionMatrixBuffer;
	ID3D11Buffer*			m_pVoxelGridBuffer;

	ID3D11RasterizerState* m_pRasteriserState;
	D3D11_VIEWPORT m_pVoxeliseViewport;

	ID3D11Texture3D* m_pVoxelisedScene;
	ID3D11UnorderedAccessView* m_pVoxelisedSceneUAV;

	XMMATRIX m_mViewProjMatrices[3];
	XMFLOAT3 m_vVoxelGridSize;

	XMMATRIX m_mWorldToVoxelGrid;
	XMMATRIX m_mWorldToVoxelGridInverse;
};

#endif
