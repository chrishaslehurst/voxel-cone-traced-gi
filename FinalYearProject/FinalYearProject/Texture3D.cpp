#include "Texture3D.h"
#include "Debugging.h"

//Tiles are 32x32x16 grids of 32 byte pixels given the format RGBA8
static const int kTileSizeInBytes = 32 * 32 * 32 * 16;

Texture3D::Texture3D()
	: m_pTexture(nullptr)
	, m_pUAV(nullptr)
	, m_pShaderResourceView(nullptr)
	, m_pRenderTargetView(nullptr)
	, m_iNumTilesMapped(0)
	, m_iBufferSizeInTiles(1)
{

}

Texture3D::~Texture3D()
{
	Shutdown();
}

HRESULT Texture3D::Init(ID3D11Device3* pDevice, ID3D11DeviceContext3* pContext, int iTextureWidth, int iTextureHeight, int iTextureDepth, int mipLevels, DXGI_FORMAT format, DXGI_FORMAT uavFormat, DXGI_FORMAT srvFormat, D3D11_USAGE usage, UINT bindFlags, UINT cpuAccessFlags /*= 0*/, UINT MiscFlags /*= 0*/)
{
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

	if (MiscFlags == D3D11_RESOURCE_MISC_TILED)
	{
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

