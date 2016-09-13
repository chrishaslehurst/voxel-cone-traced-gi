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

#define TEXTURE_DIMENSION 64

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace DirectX;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class VoxelisePass
{
public:

	__declspec(align(16)) struct VoxeliseVertexShaderBuffer
	{
		XMMATRIX mWorldInverseTranspose;
		XMMATRIX mWorld;
		XMMATRIX mWorldView;
		XMMATRIX mWorldViewProj;
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

	__declspec(align(16)) struct MatrixBuffer
	{
		XMMATRIX world;
		XMMATRIX view;
		XMMATRIX projection;
		XMFLOAT3 eyePos;
		float padding;

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

		void operator delete(void* p)
		{
			_mm_free(p);
		}
	};

	__declspec(align(16)) struct PerCubeDebugBuffer
	{
		int volumeCoord[3];
		float padding;

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_mm_free(p);
		}
	};

	HRESULT Initialise(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, HWND hwnd, AABB voxelGridAABB, int iScreenWidth, int iScreenHeight);
	void RenderClearVoxelsPass(ID3D11DeviceContext* pContext);
	void RenderDebugViewToTexture(ID3D11DeviceContext* pContext);
	void RenderDebugCubes(ID3D11DeviceContext* pContext, const XMMATRIX& mWorld, const XMMATRIX& mView, const XMMATRIX& mProjection);
	
	void RenderMesh(ID3D11DeviceContext* pDeviceContext, const XMMATRIX& mWorld, const XMMATRIX& mView, const XMMATRIX& mProjection, const XMFLOAT3& eyePos, Mesh* pVoxelise);
	bool SetVoxeliseShaderParams(ID3D11DeviceContext* pDeviceContext, const XMMATRIX& mWorld, const XMMATRIX& mView, const XMMATRIX& mProjection, const XMFLOAT3& eyePos);
	bool SetDebugShaderParams(ID3D11DeviceContext* pDeviceContext, const XMMATRIX& mWorld, const XMMATRIX& mView, const XMMATRIX& mProjection, int coord[3]);

	bool Render(ID3D11DeviceContext* pDeviceContext, int iIndexCount);
	void PostRender(ID3D11DeviceContext* pContext);
	void Shutdown();

	Texture2D* GetDebugTexture() { return m_pDebugOutput; }

private:
	void OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename);

	ID3D11VertexShader*		m_pVertexShader;
	ID3D11PixelShader*		m_pPixelShader;
	ID3D11GeometryShader*	m_pGeometryShader;
	ID3D11ComputeShader*	m_pClearVoxelsComputeShader;
	ID3D11ComputeShader*	m_pRenderDebugToTextureComputeShader;

	ID3D11VertexShader*		m_pDebugVertexShader;
	ID3D11PixelShader*		m_pDebugPixelShader;

	ID3D11InputLayout*		m_pLayout;
	ID3D11Buffer*			m_pVoxeliseVertexShaderBuffer;
	ID3D11Buffer*			m_pMatrixBuffer;
	ID3D11Buffer*			m_pPerCubeDebugBuffer;
	ID3D11Buffer*			m_pProjectionMatrixBuffer;
	ID3D11Buffer*			m_pVoxelGridBuffer;

	ID3D11RasterizerState* m_pRasteriserState;
	D3D11_VIEWPORT m_pVoxeliseViewport;

	ID3D11Texture3D* m_pVoxelisedScene;
	ID3D11UnorderedAccessView* m_pVoxelisedSceneUAV;
	ID3D11ShaderResourceView* m_pVoxelisedSceneSRV;

	Texture2D* m_pDebugOutput;
	ID3D11SamplerState* m_pSamplerState;

	XMMATRIX m_mViewProjMatrices[3];
	XMFLOAT3 m_vVoxelGridSize;

	XMMATRIX m_mWorldToVoxelGrid;
	XMMATRIX m_mWorldToVoxelGridInverse;

	Mesh* m_arrDebugRenderCube;

	int m_iScreenHeight;
	int m_iScreenWidth;
};

#endif
