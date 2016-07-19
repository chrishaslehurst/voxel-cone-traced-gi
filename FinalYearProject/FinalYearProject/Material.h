#ifndef COLOUR_SHADER_H
#define COLOUR_SHADER_H

#include <d3d11_3.h>
#include <DirectXMath.h>
#include <D3DCompiler.h>

#include <fstream>

#include "Texture.h"

#include "LightManager.h"

using namespace std;
using namespace DirectX;



enum MaterialFlags
{
	USE_TEXTURE,
	USE_NORMAL_MAPS,
	USE_SPECULAR_MAPS,
	USE_ALPHA_MASKS,
	USE_PHYSICALLY_BASED_SHADING,
	NULLS,
	MAX
};


class Material
{
	__declspec(align(16)) struct MatrixBuffer
	{
		XMMATRIX world;
		XMMATRIX view;
		XMMATRIX projection;

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_mm_free(p);
		}
	};

	__declspec(align(16)) struct DirectionalLightBuffer
	{
		XMFLOAT4 ambientColour;
		XMFLOAT4 diffuseColour;
		XMFLOAT3 lightDirection;
		float specularPower;
		XMFLOAT4 specularColour;

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_mm_free(p);
		}
	};

	__declspec(align(16)) struct CameraBuffer
	{
		XMFLOAT3 cameraPosition;
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

	__declspec(align(16)) struct PointLightPixelStruct
	{
		XMFLOAT4 vDiffuseColour;
		float	 fRange;
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

	__declspec(align(16)) struct PointLightPixelBuffer
	{
		PointLightPixelStruct pointLights[NUM_LIGHTS];

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_mm_free(p);
		}
	};

	__declspec(align(16)) struct PointLightPositionBuffer
	{
		XMFLOAT4 lightPosition[NUM_LIGHTS];

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_mm_free(p);
		}
	};

public:
	Material();
	~Material();

	bool Initialise(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, HWND hwnd);
	void Shutdown();
	bool Render(ID3D11DeviceContext* pDeviceContext, int iIndexCount, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix, XMFLOAT3 vLightDirection, XMFLOAT4 vDiffuseColour, XMFLOAT4 vAmbientColour, XMFLOAT3 vCameraPos);

	void ReloadShader(ID3D11Device* pDevice, HWND hwnd);

	void SetSpecularProperties(float r, float g, float b, float power);
	void SetDiffuseTexture(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* diffuseTexFilename);
	void SetSpecularMap(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* specMapFilename);
	void SetNormalMap(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* normalMapFilename);
	void SetAlphaMask(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* alphaMaskFilename);
	void SetRoughnessMap(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* roughnessMapFilename);
	void SetMetallicMap(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* metallicMapFilename);

	void SetHasDiffuseTexture(bool bHasTexture) { m_bHasDiffuseTexture = bHasTexture; }
	bool UsesDiffuseTexture() { return m_bHasDiffuseTexture; }

	void SetHasNormal(bool bHasNormal) { m_bHasNormalMap = bHasNormal; }
	bool UsesNormalMaps() { return m_bHasNormalMap; }

	void SetHasSpecular(bool bHasSpecular) { m_bHasSpecularMap = bHasSpecular; }
	bool UsesSpecularMaps() { return m_bHasSpecularMap; }

	void SetHasAlphaMask(bool bHasMask) { m_bHasAlphaMask = bHasMask; }
	bool UsesAlphaMaps() { return m_bHasAlphaMask; }

	void SetHasRoughness(bool bHasRough) { m_bHasRoughnessMap = bHasRough; }
	bool UsesRoughnessMaps() { return m_bHasRoughnessMap; }

	void SetHasMetallic(bool bHasMetallic) { m_bHasMetallicMap = bHasMetallic; }
	bool UsesMetallicMaps() { return m_bHasMetallicMap; }
private:

	Texture* LoadTexture(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* filename);
	void ReleaseTexture();

	bool InitialiseShader(ID3D11Device* pDevice, HWND hwnd, WCHAR* sShaderFilename);
	void ShutdownShader();
	void OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename);

	bool SetShaderParameters(ID3D11DeviceContext* pDeviceContext, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix, XMFLOAT3 vLightDirection, XMFLOAT4 vDiffuseColour, XMFLOAT4 vAmbientColour, XMFLOAT3 vCameraPos);
	void RenderShader(ID3D11DeviceContext* pDeviceContext, int iIndexCount);

	ID3D11VertexShader* m_pVertexShader;
	ID3D11PixelShader*  m_pPixelShader;
	ID3D11InputLayout*  m_pLayout;
	ID3D11Buffer*		m_pMatrixBuffer;
	ID3D11Buffer*		m_pLightBuffer;
	ID3D11Buffer*		m_pCameraBuffer;
	ID3D11Buffer*		m_pPointLightColourBuffer;
	ID3D11Buffer*		m_pPointLightPositionBuffer;

	float				m_fSpecularPower;
	XMFLOAT4			m_vSpecularColour;
	bool				m_bHasDiffuseTexture;
	Texture*			m_pDiffuseTexture;

	bool				m_bHasNormalMap;
	Texture*			m_pNormalMap;
	bool				m_bHasSpecularMap;
	Texture*			m_pSpecularMap;
	bool				m_bHasRoughnessMap;
	Texture*			m_pRoughnessMap;
	bool				m_bHasMetallicMap;
	Texture*			m_pMetallicMap;

	bool				m_bHasAlphaMask;
	Texture*			m_pAlphaMask;

	ID3D11SamplerState*		m_pSampleState;
	ID3D11SamplerState*		m_pShadowMapSampler;
	int						pixelShaderResourceCount;

	D3D_SHADER_MACRO m_defines[MaterialFlags::MAX];
};

#endif // !COLOUR_SHADER_H

