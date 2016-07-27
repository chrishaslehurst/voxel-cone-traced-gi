#ifndef DEFERRED_RENDER_H
#define DEFERRED_RENDER_H
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <d3d11_3.h>
#include <DirectXMath.h>
#include <D3DCompiler.h>


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define NUM_LIGHTS 4

using namespace DirectX;

enum BufferType
{
	btWorldPos,
	btDiffuse,
	btNormals, //roughness is stored in the w component of the normals texture
	btMetallic,
	btMax
};

class DeferredRender
{
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

	__declspec(align(16)) struct PointLightPixelStruct
	{
		XMFLOAT4 vDiffuseColour;
		float	 fRange;
		XMFLOAT3 vPosition;

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_mm_free(p);
		}
	};

	__declspec(align(16)) struct LightBuffer
	{
		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_mm_free(p);
		}

		XMFLOAT4 AmbientColour;
		XMFLOAT4 DirectionalLightColour;
		XMFLOAT3 DirectionalLightDirection;
		PointLightPixelStruct pointLights[NUM_LIGHTS];
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

		XMFLOAT3 vCameraPosition;
		float padding;
	};


public:

	DeferredRender();
	~DeferredRender();

	void SetRenderTargets(ID3D11DeviceContext* pContext);
	void ClearRenderTargets(ID3D11DeviceContext* pContext, float r, float g, float b, float a);

	ID3D11ShaderResourceView* GetShaderResourceView(BufferType index);

	HRESULT Initialise(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, HWND hwnd, int iTextureWidth, int iTextureHeight, float fScreenDepth, float fScreenNear);
	void Shutdown();

	bool RenderLightingPass(ID3D11DeviceContext* pContext, int iIndexCount, XMMATRIX mWorld, XMMATRIX mView, XMMATRIX mProjection, const XMFLOAT3& vCamPos);

private:

	bool InitialiseShader(ID3D11Device* pDevice, HWND hwnd, WCHAR* sShaderFilename);
	void ShutdownShader();
	void OutputShaderErrorMessage(ID3D10Blob* pBlob, HWND hwnd, WCHAR* sShaderFilename);

	bool SetShaderParameters(ID3D11DeviceContext* pContext, XMMATRIX mWorld, XMMATRIX mView, XMMATRIX mProjection, const XMFLOAT3& vCameraPos);
	void RenderShader(ID3D11DeviceContext* pContext, int iIndexCount);

	int m_iTextureWidth;
	int m_iTextureHeight;

	ID3D11Texture2D* m_arrRenderTargetTextures[BufferType::btMax];
	ID3D11RenderTargetView* m_arrRenderTargetViews[BufferType::btMax];
	ID3D11ShaderResourceView* m_arrShaderResourceViews[BufferType::btMax];
	ID3D11Texture2D* m_pDepthStencilBuffer;
	ID3D11DepthStencilView* m_pDepthStencilView;
	D3D11_VIEWPORT m_viewport;

	ID3D11VertexShader* m_pVertexShader;
	ID3D11PixelShader* m_pPixelShader;
	ID3D11InputLayout* m_pLayout;
	ID3D11SamplerState* m_pSampleState;
	ID3D11SamplerState* m_pShadowMapSampleState;
	ID3D11Buffer* m_pMatrixBuffer;
	ID3D11Buffer* m_pLightingBuffer;
	ID3D11Buffer* m_pCameraBuffer;

};

#endif