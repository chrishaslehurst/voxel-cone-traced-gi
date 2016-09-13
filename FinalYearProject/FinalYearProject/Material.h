#ifndef COLOUR_SHADER_H
#define COLOUR_SHADER_H

#include <d3d11_3.h>
#include <DirectXMath.h>
#include <D3DCompiler.h>

#include <fstream>

#include "Texture2D.h"

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


public:
	Material();
	~Material();

	bool Initialise(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, HWND hwnd);
	void Shutdown();
	bool Render(ID3D11DeviceContext* pDeviceContext, int iIndexCount, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix);

	void ReloadShader(ID3D11Device* pDevice, HWND hwnd);

	void SetSpecularProperties(float r, float g, float b, float power);
	void SetDiffuseTexture(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* diffuseTexFilename);
	void SetSpecularMap(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* specMapFilename);
	void SetNormalMap(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* normalMapFilename);
	void SetAlphaMask(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* alphaMaskFilename);
	void SetRoughnessMap(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* roughnessMapFilename);
	void SetMetallicMap(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* metallicMapFilename);

	Texture2D* GetDiffuseTexture() { return m_pDiffuseTexture; }
	void SetHasDiffuseTexture(bool bHasTexture) { m_bHasDiffuseTexture = bHasTexture; }
	bool UsesDiffuseTexture() { return m_bHasDiffuseTexture; }

	Texture2D* GetNormalMap() { return m_pNormalMap; }
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

	bool SetPerFrameShaderParameters(ID3D11DeviceContext* pDeviceContext, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix);
	void SetShadersAndSamplers(ID3D11DeviceContext* pDeviceContext);
private:

	Texture2D* LoadTexture(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* filename);
	void ReleaseTextures();

	bool InitialiseShader(ID3D11Device* pDevice, HWND hwnd, WCHAR* sShaderFilename);
	void ShutdownShader();
	void OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename);

	bool SetShaderParameters(ID3D11DeviceContext* pDeviceContext, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix);
	void RenderShader(ID3D11DeviceContext* pDeviceContext, int iIndexCount);

	ID3D11VertexShader* m_pVertexShader;
	ID3D11PixelShader*  m_pPixelShader;
	ID3D11InputLayout*  m_pLayout;
	ID3D11Buffer*		m_pMatrixBuffer;

	float				m_fSpecularPower;
	XMFLOAT4			m_vSpecularColour;
	bool				m_bHasDiffuseTexture;
	Texture2D*			m_pDiffuseTexture;

	bool				m_bHasNormalMap;
	Texture2D*			m_pNormalMap;
	bool				m_bHasSpecularMap;
	Texture2D*			m_pSpecularMap;
	bool				m_bHasRoughnessMap;
	Texture2D*			m_pRoughnessMap;
	bool				m_bHasMetallicMap;
	Texture2D*			m_pMetallicMap;

	bool				m_bHasAlphaMask;
	Texture2D*			m_pAlphaMask;

	ID3D11SamplerState*		m_pSampleState;
	ID3D11SamplerState*		m_pShadowMapSampler;
	int						pixelShaderResourceCount;

	D3D_SHADER_MACRO m_defines[MaterialFlags::MAX];
};

#endif // !COLOUR_SHADER_H

