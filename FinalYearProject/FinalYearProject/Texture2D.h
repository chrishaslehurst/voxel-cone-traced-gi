#ifndef TEXTURE2D_H
#define TEXTURE2D_H

#include <d3d11_3.h>
#include "../DirectXTex/DirectXTex.h"

using namespace DirectX;

class Texture2D
{
public:
	Texture2D();
	~Texture2D();

	HRESULT Init(ID3D11Device* pDevice, int iTextureWidth, int iTextureHeight, int mipLevels, int ArraySize, DXGI_FORMAT format, D3D11_USAGE usage, UINT bindFlags, UINT cpuAccessFlags = 0, UINT MiscFlags = 0);

	bool LoadTextureFromFile(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* filename);
	void Shutdown();

	ID3D11ShaderResourceView* GetShaderResourceView();
	ID3D11RenderTargetView* GetRenderTargetView();
	ID3D11UnorderedAccessView* GetUAV();
	ID3D11Texture2D* GetTexture() { return m_pTexture; }

private:

	ID3D11ShaderResourceView* m_pShaderResourceView;
	ID3D11RenderTargetView*   m_pRenderTargetView;
	ID3D11UnorderedAccessView* m_pUAV;
	ID3D11Texture2D*		  m_pTexture;
	ScratchImage*			  m_pImage;
};

#endif // !TEXTURE2D_H
