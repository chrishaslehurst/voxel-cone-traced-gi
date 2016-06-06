#ifndef TEXTURE_H
#define TEXTURE_H

#include <d3d11_3.h>
#include "../DirectXTex/DirectXTex.h"

using namespace DirectX;

class Texture
{
public:
	Texture();
	~Texture();

	bool LoadTexture(ID3D11Device* pDevice, WCHAR* filename);
	void Shutdown();

	ID3D11ShaderResourceView* GetTexture();

private:

	ID3D11ShaderResourceView* m_pTexture;
};

#endif // !TEXTURE_H
