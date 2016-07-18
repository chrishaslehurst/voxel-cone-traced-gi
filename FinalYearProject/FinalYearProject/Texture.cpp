#include "Texture.h"
#include "Debugging.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Texture::Texture()
	:m_pTexture(nullptr)
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Texture::~Texture()
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Texture::LoadTexture(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* filename)
{
	m_pImage = new ScratchImage;
	ScratchImage mipChain;
	TexMetadata texMeta;
	HRESULT hr = LoadFromTGAFile(filename, &texMeta, *m_pImage);
	if (FAILED(hr))
	{
		VS_LOG_VERBOSE("Failed to load tex from file");
		return false;
	}
	const Image* pImage = m_pImage->GetImage(0, 0, 0);
	

	hr = GenerateMipMaps(m_pImage->GetImages(), m_pImage->GetImageCount(), m_pImage->GetMetadata(), TEX_FILTER_DEFAULT, 0, mipChain);
	if (FAILED(hr))
	{
		VS_LOG_VERBOSE("Failed to generate mip maps for texture");
		return false;
	}
	
	hr = CreateShaderResourceView(pDevice, mipChain.GetImages(), mipChain.GetImageCount(), mipChain.GetMetadata(), &m_pTexture);
	if (FAILED(hr))
	{
		VS_LOG_VERBOSE("Texture failed to load");
		return false;
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Texture::Shutdown()
{
	if (m_pTexture)
	{
		m_pTexture->Release();
		m_pTexture = nullptr;
	}
	if (m_pImage)
	{
		m_pImage->Release();
		m_pImage = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ID3D11ShaderResourceView* Texture::GetTexture()
{
	return m_pTexture;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
