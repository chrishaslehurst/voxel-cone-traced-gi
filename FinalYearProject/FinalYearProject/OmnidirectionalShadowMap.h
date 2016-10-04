#ifndef OMNIDIRECTIONAL_SHADOW_MAP_H
#define OMNIDIRECTIONAL_SHADOW_MAP_H

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <d3d11_3.h>
#include <DirectXMath.h>
#include <D3DCompiler.h>
#include <fstream>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace DirectX;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

__declspec(align(16)) class OmnidirectionalShadowMap
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

	__declspec(align(16)) struct LightBuffer
	{
		XMFLOAT4 vWorldSpaceLightPosition;

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_mm_free(p);
		}
	};

	__declspec(align(16)) struct LightRangeBuffer
	{
		float range;
		XMFLOAT3 padding;

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_mm_free(p);
		}
	};

	__declspec(align(16)) struct LightProjectionBuffer
	{
		XMMATRIX lightViewProjMatrices[6];

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_mm_free(p);
		}
	};

	void* operator new(size_t i)
	{
		return _mm_malloc(i, 16);
	}

		void operator delete(void* p)
	{
		_mm_free(p);
	}

	OmnidirectionalShadowMap(float fScreenNear = 0.1f, float fScreenDepth = 5000.f);
	~OmnidirectionalShadowMap();

	HRESULT Initialise(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, HWND hwnd);
	void SetRenderOutputToShadowMap(ID3D11DeviceContext* pDeviceContext);
	void SetRenderStart(ID3D11DeviceContext* pDeviceContext);
	bool Render(ID3D11DeviceContext* pDeviceContext, int iIndexCount);
	void SetRenderFinished(ID3D11DeviceContext* pDeviceContext);
	bool SetShaderParams(ID3D11DeviceContext* pDeviceContext, const XMFLOAT4& lightPosition, float lightRange, const XMMATRIX& mWorld);
	ID3D11ShaderResourceView* GetShadowMapShaderResource() { return m_pShadowMapCubeShaderView; }

	void Shutdown();
private:

	void OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename);

	D3D11_VIEWPORT			m_ShadowMapViewport;

	ID3D11InputLayout*		m_pLayout;
	ID3D11VertexShader*		m_pVertexShader;
	ID3D11PixelShader*		m_pPixelShader;
	ID3D11GeometryShader*	m_pGeometryShader;

	ID3D11Buffer*			m_pMatrixBuffer;
	ID3D11Buffer*			m_pLightBuffer;
	ID3D11Buffer*			m_pLightRangeBuffer;
	ID3D11Buffer*			m_pLightProjectionBuffer;

	ID3D11Texture2D*			m_pShadowMapCubeTexture;
	ID3D11DepthStencilView*		m_pShadowMapCubeDepthView;
	ID3D11ShaderResourceView*	m_pShadowMapCubeShaderView;
	

	static const int kShadowMapSize = 1024;
	XMMATRIX m_LightViewProjMatrices[6];
	XMMATRIX m_LightProjMatrix;
};

#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////