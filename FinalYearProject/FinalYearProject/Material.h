#ifndef COLOUR_SHADER_H
#define COLOUR_SHADER_H

#include <d3d11_3.h>
#include <DirectXMath.h>
#include <D3DCompiler.h>

#include <fstream>

#include "Texture.h"

using namespace std;
using namespace DirectX;

enum MaterialFlags
{
	USE_NORMAL_MAPS,
	USE_SPECULAR_MAPS,
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

	__declspec(align(16)) struct LightBuffer
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


public:
	Material();
	~Material();

	bool Initialise(ID3D11Device* pDevice, HWND hwnd, WCHAR* textureFileName);
	void Shutdown();
	bool Render(ID3D11DeviceContext* pDeviceContext, int iIndexCount, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix, XMFLOAT3 vLightDirection, XMFLOAT4 vDiffuseColour, XMFLOAT4 vAmbientColour, XMFLOAT3 vCameraPos);

	void SetSpecularProperties(float r, float g, float b, float power);
	void SetSpecularMap(ID3D11Device* pDevice, WCHAR* specMapFilename);
	void SetNormalMap(ID3D11Device* pDevice, WCHAR* normalMapFilename);

	void SetHasNormal(bool bHasNormal) { m_bHasNormalMap = bHasNormal; }
	bool UsesNormalMaps() { return m_bHasNormalMap; }

	void SetHasSpecular(bool bHasSpecular) { m_bHasSpecularMap = bHasSpecular; }
	bool UsesSpecularMaps() { return m_bHasSpecularMap; }
private:

	Texture* LoadTexture(ID3D11Device* pDevice, WCHAR* filename);
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


	float				m_fSpecularPower;
	XMFLOAT4			m_vSpecularColour;
	Texture*			m_pDiffuseTexture;

	bool				m_bHasNormalMap;
	Texture*			m_pNormalMap;
	bool				m_bHasSpecularMap;
	Texture*			m_pSpecularMap;
	ID3D11SamplerState* m_pSampleState;


	D3D_SHADER_MACRO m_defines[MaterialFlags::MAX];
};

#endif // !COLOUR_SHADER_H

