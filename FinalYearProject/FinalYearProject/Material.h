#ifndef COLOUR_SHADER_H
#define COLOUR_SHADER_H

#include <d3d11_3.h>
#include <DirectXMath.h>
#include <D3DCompiler.h>

#include <fstream>

#include "Texture.h"

using namespace std;
using namespace DirectX;

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

	bool Initialise(ID3D11Device* pDevice, HWND hwnd, WCHAR* textureFileName);
	void Shutdown();
	bool Render(ID3D11DeviceContext* pDeviceContext, int iIndexCount, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix);

private:

	bool LoadTexture(ID3D11Device* pDevice, WCHAR* filename);
	void ReleaseTexture();

	bool InitialiseShader(ID3D11Device* pDevice, HWND hwnd, WCHAR* sShaderFilename);
	void ShutdownShader();
	void OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename);

	bool SetShaderParameters(ID3D11DeviceContext* pDeviceContext, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix);
	void RenderShader(ID3D11DeviceContext* pDeviceContext, int iIndexCount);


	ID3D11VertexShader* m_pVertexShader;
	ID3D11PixelShader*  m_pPixelShader;
	ID3D11InputLayout*  m_pLayout;
	ID3D11Buffer*		m_pMatrixBuffer;

	Texture* m_pTexture;
	ID3D11SamplerState* m_pSampleState;

};

#endif // !COLOUR_SHADER_H

