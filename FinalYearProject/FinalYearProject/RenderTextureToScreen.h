#ifndef RENDER_TEXTURE_TO_SCREEN_H
#define RENDER_TEXTURE_TO_SCREEN_H

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <d3d11_3.h>
#include <DirectXMath.h>
#include <D3DCompiler.h>
#include "Texture2D.h"
#include "DeferredRender.h"
#include <fstream>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class RenderTextureToScreen
{


public:
	HRESULT Initialise(ID3D11Device3* pDevice, ID3D11DeviceContext* pContext, HWND hwnd, int iTextureWidth, int iTextureHeight, float fScreenDepth, float fScreenNear);
	void Shutdown();

	bool RenderTexture(ID3D11DeviceContext3* pContext, int iIndexCount, XMMATRIX mWorld, XMMATRIX mView, XMMATRIX mProjection, const XMFLOAT3& vCamPos, Texture2D* pTexture);

private:
	bool InitialiseShader(ID3D11Device3* pDevice, HWND hwnd, WCHAR* sShaderFilename);
	bool SetShaderParameters(ID3D11DeviceContext* pContext, XMMATRIX mWorld, XMMATRIX mView, XMMATRIX mProjection, const XMFLOAT3& vCameraPos, Texture2D* pTexture);
	void RenderShader(ID3D11DeviceContext3* pContext, int iIndexCount);

	void OutputShaderErrorMessage(ID3D10Blob* pBlob, HWND hwnd, WCHAR* sShaderFilename);

	D3D11_VIEWPORT m_viewport;

	ID3D11Buffer* m_pMatrixBuffer;

	RenderPass* m_pRenderTexturePass;

	ID3D11SamplerState* m_pSampleState;
};

#endif