#ifndef TEXTURE3D_H
#define TEXTURE3D_H

#include <d3d11_3.h>
#include "../DirectXTex/DirectXTex.h"

using namespace DirectX;

class Texture3D
{
public:
	Texture3D();
	~Texture3D();

	HRESULT Init(ID3D11Device3* pDevice, ID3D11DeviceContext3* pContext, int iTextureWidth, int iTextureHeight, int iTextureDepth, int mipLevels, DXGI_FORMAT format, DXGI_FORMAT uavFormat, DXGI_FORMAT srvFormat, D3D11_USAGE usage, UINT bindFlags, UINT cpuAccessFlags = 0, UINT MiscFlags = 0);

	void Shutdown();

	ID3D11ShaderResourceView* GetShaderResourceView();
	ID3D11RenderTargetView* GetRenderTargetView();
	ID3D11UnorderedAccessView* GetUAV();
	ID3D11Texture3D*		GetTexture() { return m_pTexture; }
private:

	ID3D11ShaderResourceView* m_pShaderResourceView;
	ID3D11RenderTargetView*   m_pRenderTargetView;
	ID3D11UnorderedAccessView* m_pUAV;
	ID3D11Texture3D*		  m_pTexture;
	
	ID3D11Buffer*				pTilePool;
};

#endif // !TEXTURE3D_H