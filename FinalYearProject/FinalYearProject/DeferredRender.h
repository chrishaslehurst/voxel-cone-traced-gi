#ifndef DEFERRED_RENDER_H
#define DEFERRED_RENDER_H
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <d3d11_3.h>
#include <DirectXMath.h>
#include <D3DCompiler.h>
#include "Texture2D.h"
#include "LightManager.h"
#include "VoxelisedScene.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace DirectX;

enum BufferType
{
	btWorldPos,
	btDiffuse, //metallic is stored in the w component of the diffuse
	btNormals, //roughness is stored in the w component of the normals texture
	btMax
};

enum GIRenderFlag
{
	giNone,
	giFull,
	giDiff,
	giSpec,
	giAO,
	giMax
};

class DeferredRender
{
public:
	__declspec(align(16)) struct MatrixBuffer
	{
		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_mm_free(p);
		}

		XMMATRIX mWorld;
		XMMATRIX mView;
		XMMATRIX mProjection;
	};

	

	__declspec(align(16)) struct CameraBuffer
	{
		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_mm_free(p);
		}

		XMMATRIX mWorldToVoxelGrid;
		XMFLOAT3 vCameraPosition;
		float fVoxelScale;
	};

	struct RenderFlags
	{
		GIRenderFlag eRenderGlobalIllumination;
		int padding[3];
	};


public:

	DeferredRender();
	~DeferredRender();

	void SetRenderTargets(ID3D11DeviceContext3* pContext);
	void ClearRenderTargets(ID3D11DeviceContext3* pContext, float r, float g, float b, float a);

	ID3D11ShaderResourceView* GetShaderResourceView(BufferType index);
	Texture2D* GetTexture(BufferType index) { return m_arrBufferTextures[index]; }

	HRESULT Initialise(ID3D11Device3* pDevice, ID3D11DeviceContext* pContext, HWND hwnd, int iTextureWidth, int iTextureHeight, float fScreenDepth, float fScreenNear);
	void Shutdown();

	bool RenderLightingPass(ID3D11DeviceContext3* pContext, int iIndexCount, XMMATRIX mWorld, XMMATRIX mView, XMMATRIX mProjection, const XMFLOAT3& vCamPos, VoxelisedScene* pVoxelisedScene, GIRenderFlag eGIFlags);

private:

	bool InitialiseShader(ID3D11Device3* pDevice, HWND hwnd, WCHAR* sShaderFilename);
	void ShutdownShader();
	void OutputShaderErrorMessage(ID3D10Blob* pBlob, HWND hwnd, WCHAR* sShaderFilename);

	bool SetShaderParameters(ID3D11DeviceContext3* pContext, XMMATRIX mWorld, XMMATRIX mView, XMMATRIX mProjection, const XMFLOAT3& vCameraPos, VoxelisedScene* pVoxelisedScene, GIRenderFlag eGIFlags);
	void RenderShader(ID3D11DeviceContext3* pContext, int iIndexCount);

	int m_iTextureWidth;
	int m_iTextureHeight;

	Texture2D* m_arrBufferTextures[BufferType::btMax];

	ID3D11Texture2D* m_pDepthStencilBuffer;
	ID3D11DepthStencilView* m_pDepthStencilView;
	D3D11_VIEWPORT m_viewport;

	RenderPass* m_pLightingPass;

	ID3D11SamplerState* m_pSampleState;
	ID3D11SamplerState* m_pShadowMapSampleState;
	ID3D11SamplerState* m_pVoxelSampler;
	ID3D11Buffer* m_pMatrixBuffer;
	ID3D11Buffer* m_pGIRenderFlagBuffer;
	ID3D11Buffer* m_pCameraBuffer;

};

#endif