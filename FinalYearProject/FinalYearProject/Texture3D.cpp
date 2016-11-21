#include "Texture3D.h"
#include "Debugging.h"

//Tiles are 32x32x16 grids of 4 byte pixels given the format RGBA8
static const int kTileSizeInBytes = 4 * 32 * 32 * 16;

Texture3D::Texture3D()
	: m_pTexture(nullptr)
	, m_pUAV(nullptr)
	, m_pShaderResourceView(nullptr)
	, m_pRenderTargetView(nullptr)
	, m_iNumTilesMapped(0)
	, m_iBufferSizeInTiles(1)
	, m_bTiled(false)
{

}

Texture3D::~Texture3D()
{
	Shutdown();
}

HRESULT Texture3D::Init(ID3D11Device3* pDevice, ID3D11DeviceContext3* pContext, int iTextureWidth, int iTextureHeight, int iTextureDepth, int mipLevels, DXGI_FORMAT format, DXGI_FORMAT uavFormat, DXGI_FORMAT srvFormat, D3D11_USAGE usage, UINT bindFlags, UINT cpuAccessFlags /*= 0*/, UINT MiscFlags /*= 0*/)
{
	m_iResolution[0] = iTextureWidth;
	m_iResolution[1] = iTextureHeight;
	m_iResolution[2] = iTextureDepth;
	m_iMipLevels = mipLevels;
	//Initialise Resources..
	D3D11_TEXTURE3D_DESC textureDesc;
	ZeroMemory(&textureDesc, sizeof(textureDesc));

	// Setup the 3d texture description.
	textureDesc.Width = iTextureWidth;
	textureDesc.Height = iTextureHeight;
	textureDesc.Depth = iTextureDepth;
	textureDesc.MipLevels = mipLevels;
	textureDesc.Format = format;
	textureDesc.Usage = usage;
	textureDesc.BindFlags = bindFlags;
	textureDesc.CPUAccessFlags = cpuAccessFlags;
	textureDesc.MiscFlags = MiscFlags;

	HRESULT result = pDevice->CreateTexture3D(&textureDesc, nullptr, &m_pTexture);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create 3d texture");
		return false;
	}

	if (MiscFlags & D3D11_RESOURCE_MISC_TILED)
	{
		m_bTiled = true;
		D3D11_BUFFER_DESC tilePoolDesc;
		ZeroMemory(&tilePoolDesc, sizeof(tilePoolDesc));
		tilePoolDesc.ByteWidth = m_iBufferSizeInTiles * kTileSizeInBytes;
		tilePoolDesc.Usage = D3D11_USAGE_DEFAULT;
		tilePoolDesc.MiscFlags = D3D11_RESOURCE_MISC_TILE_POOL;

		HRESULT result = pDevice->CreateBuffer(&tilePoolDesc, nullptr, &pTilePool);
		if (FAILED(result))
		{
			VS_LOG_VERBOSE("Failed to create tile pool");
			return false;
		}
	}

	if (textureDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{

		//Setup the unordered access view so we can render to the textures..
		D3D11_UNORDERED_ACCESS_VIEW_DESC unorderedAccessViewDesc;
		unorderedAccessViewDesc.Format = uavFormat;
		unorderedAccessViewDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
		unorderedAccessViewDesc.Texture3D.MipSlice = 0;
		unorderedAccessViewDesc.Texture3D.FirstWSlice = 0;
		unorderedAccessViewDesc.Texture3D.WSize = iTextureDepth;

		result = pDevice->CreateUnorderedAccessView(m_pTexture, &unorderedAccessViewDesc, &m_pUAV);
		if (FAILED(result))
		{
			VS_LOG_VERBOSE("Failed to create unordered access view");
			return false;
		}
	}
	if (textureDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = srvFormat;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		srvDesc.Texture3D.MipLevels = mipLevels;
		srvDesc.Texture3D.MostDetailedMip = 0;
		result = pDevice->CreateShaderResourceView(m_pTexture, &srvDesc, &m_pShaderResourceView);
		if (FAILED(result))
		{
			VS_LOG_VERBOSE("Failed to create shader resource view");
			return false;
		}
	}
	
	
}

void Texture3D::Shutdown()
{
	
	if (m_pTexture)
	{
		m_pTexture->Release();
		m_pTexture = nullptr;
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
	if (pTilePool)
	{
		pTilePool->Release();
		pTilePool = nullptr;
	}
}

ID3D11ShaderResourceView* Texture3D::GetShaderResourceView()
{
	return m_pShaderResourceView;
}

ID3D11RenderTargetView* Texture3D::GetRenderTargetView()
{
	return m_pRenderTargetView;
}

ID3D11UnorderedAccessView* Texture3D::GetUAV()
{
	return m_pUAV;
}

HRESULT Texture3D::MapTile(ID3D11DeviceContext3* pContext, int x, int y, int z, int mipLevel)
{
	if ((m_iNumTilesMapped + 1) > m_iBufferSizeInTiles)
	{
		if (FAILED(pContext->ResizeTilePool(pTilePool, (m_iBufferSizeInTiles + 1) * kTileSizeInBytes)))
		{
			VS_LOG_VERBOSE("Failed to create tile pool");
			return false;
		}
		m_iBufferSizeInTiles++;
	}
	D3D11_TILED_RESOURCE_COORDINATE coord;
	coord.X = x;
	coord.Y = y;
	coord.Z = z;
	coord.Subresource = mipLevel;

	D3D11_TILE_REGION_SIZE TRS;
	TRS.bUseBox = false;
	TRS.NumTiles = 1;

	UINT RangeFlags = 0;
	UINT startOffset = m_iNumTilesMapped;

	if (FAILED(pContext->UpdateTileMappings(m_pTexture, 1, &coord, NULL, pTilePool, 1, &RangeFlags, &startOffset, nullptr, D3D11_TILE_MAPPING_NO_OVERWRITE)))
	{
		VS_LOG_VERBOSE("Failed to map tiles");
		return false;
	}

	m_iNumTilesMapped++;
	return true;
}

HRESULT Texture3D::UnmapTile(ID3D11DeviceContext3* pContext, int x, int y, int z, int mipLevel)
{
	return true;
}

HRESULT Texture3D::UnmapAllTiles(ID3D11DeviceContext3* pContext)
{
	m_iNumTilesMapped = 0;
	D3D11_TILED_RESOURCE_COORDINATE coord;
	coord.X = 0;
	coord.Y = 0;
	coord.Z = 0;
	coord.Subresource = 0;

	D3D11_TILE_REGION_SIZE TRS;
	TRS.bUseBox = true;
	TRS.NumTiles = (m_iResolution[0]/32) * (m_iResolution[1]/32) * (m_iResolution[2]/16);
	TRS.Width = (m_iResolution[0] / 32);
	TRS.Depth = (m_iResolution[2] / 16);
	TRS.Height = (m_iResolution[1] / 32);

	UINT RangeFlags = D3D11_TILE_RANGE_NULL;
	UINT startOffset = 0;
	HRESULT hr = pContext->UpdateTileMappings(m_pTexture, 1, &coord, &TRS, pTilePool, 1, &RangeFlags, &startOffset, nullptr, 0);
	if (FAILED(hr))
	{
		VS_LOG_VERBOSE("Failed to map tiles");
		return false;
	}
}

int Texture3D::GetMemoryUsageInBytes()
{
	if (m_bTiled)
	{
		return m_iBufferSizeInTiles * kTileSizeInBytes;
	}
	else
	{
		int numPixels = 0;
		float mipDiv = 1;
		for (int i = 0; i < m_iMipLevels; i++)
		{
			//Multiply by 4 because its always 4 bytes for the ones we're interested in. Would need a better solution if this changed.
			numPixels += ((m_iResolution[0] * mipDiv) * (m_iResolution[1] * mipDiv) * (m_iResolution[2] * mipDiv));
			mipDiv *= 0.5f;
		}
		return numPixels * 4;
	}
}

