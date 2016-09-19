#include "Texture2D.h"
#include "Debugging.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Texture2D::Texture2D()
	: m_pTexture(nullptr)
	, m_pShaderResourceView(nullptr)
	, m_pRenderTargetView(nullptr)
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Texture2D::~Texture2D()
{
	Shutdown();
}

HRESULT Texture2D::Init(ID3D11Device* pDevice, int iTextureWidth, int iTextureHeight, int mipLevels, int ArraySize, DXGI_FORMAT format, D3D11_USAGE usage, UINT bindFlags, UINT cpuAccessFlags /*= 0*/, UINT MiscFlags /*= 0*/)
{
	HRESULT res;

	D3D11_TEXTURE2D_DESC textureDesc;
	ZeroMemory(&textureDesc, sizeof(textureDesc));

	// Setup the render target texture description.
	textureDesc.Width = iTextureWidth;
	textureDesc.Height = iTextureHeight;
	textureDesc.MipLevels = mipLevels;
	textureDesc.ArraySize = ArraySize;
	textureDesc.Format = format;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Usage = usage;
	textureDesc.BindFlags = bindFlags;
	textureDesc.CPUAccessFlags = cpuAccessFlags;
	textureDesc.MiscFlags = MiscFlags;

	
	res = pDevice->CreateTexture2D(&textureDesc, nullptr, &m_pTexture);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to create texture");
		return false;
	}
	
	if (bindFlags & D3D11_BIND_RENDER_TARGET)
	{
		D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
		renderTargetViewDesc.Format = textureDesc.Format;
		renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		renderTargetViewDesc.Texture2D.MipSlice = 0;

		res = pDevice->CreateRenderTargetView(m_pTexture, &renderTargetViewDesc, &m_pRenderTargetView);
		if (FAILED(res))
		{
			VS_LOG_VERBOSE("Failed to create render target view");
			return false;
		}
		
	}

	if (bindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		//Setup the shader resource views so the shaders can access the texture information
		D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
		shaderResourceViewDesc.Format = textureDesc.Format;
		shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;

		
		res = pDevice->CreateShaderResourceView(m_pTexture, &shaderResourceViewDesc, &m_pShaderResourceView);
		if (FAILED(res))
		{
			VS_LOG_VERBOSE("Failed to create shader resource views");
			return false;
		}
	}

	if (bindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		//Setup the unordered access view so we can render to the textures..
		D3D11_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc;
		unorderedAccessViewDesc.Format = textureDesc.Format;
		unorderedAccessViewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		unorderedAccessViewDesc.Texture2D.MipSlice = 0;

		res = pDevice->CreateUnorderedAccessView(m_pTexture, &unorderedAccessViewDesc, &m_pUAV);
		if (FAILED(res))
		{
			VS_LOG_VERBOSE("Failed to create unordered access view");
			return false;
		}
	}


	return res;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Texture2D::LoadTextureFromFile(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* filename)
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


	hr = GenerateMipMaps(m_pImage->GetImages(), m_pImage->GetImageCount(), m_pImage->GetMetadata(), TEX_FILTER_DEFAULT, 0, mipChain);
	if (FAILED(hr))
	{
		VS_LOG_VERBOSE("Failed to generate mip maps for texture");
		return false;
	}
 	
	hr = CreateShaderResourceView(pDevice, mipChain.GetImages(), mipChain.GetImageCount(), mipChain.GetMetadata(), &m_pShaderResourceView);
	if (FAILED(hr))
	{
		VS_LOG_VERBOSE("Texture failed to load");
		return false;
	}
	
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Texture2D::Shutdown()
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
	if (m_pRenderTargetView)
	{
		m_pRenderTargetView->Release();
		m_pRenderTargetView = nullptr;
	}
	if (m_pShaderResourceView)
	{
		m_pShaderResourceView->Release();
		m_pShaderResourceView = nullptr;
	}
	if (m_pUAV)
	{
		m_pUAV->Release();
		m_pUAV = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ID3D11ShaderResourceView* Texture2D::GetShaderResourceView()
{
	return m_pShaderResourceView;
}

ID3D11RenderTargetView* Texture2D::GetRenderTargetView()
{
	return m_pRenderTargetView;
}

ID3D11UnorderedAccessView* Texture2D::GetUAV()
{
	return m_pUAV;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
