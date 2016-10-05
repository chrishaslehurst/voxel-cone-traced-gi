#include "Texture3D.h"
#include "Debugging.h"

Texture3D::Texture3D()
	: m_pTexture(nullptr)
	, m_pUAV(nullptr)
	, m_pShaderResourceView(nullptr)
	, m_pRenderTargetView(nullptr)
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
		tilePoolDesc.ByteWidth = 32 * 256 * 256 * 256;
		tilePoolDesc.Usage = D3D11_USAGE_DEFAULT;
		tilePoolDesc.MiscFlags = D3D11_RESOURCE_MISC_TILE_POOL;

		HRESULT result = pDevice->CreateBuffer(&tilePoolDesc, nullptr, &pTilePool);
		if (FAILED(result))
		{
			VS_LOG_VERBOSE("Failed to create tile pool");
			return false;
		}

	//	result = pContext->ResizeTilePool(pTilePool, 32 * 256 * 256 * 256);
	//	if (FAILED(result))
	//	{
	//		VS_LOG_VERBOSE("Failed to create tile pool");
	//		return false;
	//	}


		D3D11_TILED_RESOURCE_COORDINATE coord;
		coord.X = 0;
		coord.Y = 0;
		coord.Z = 0;
		coord.Subresource = 0;

		D3D11_TILE_REGION_SIZE TRS;
		TRS.bUseBox = false;
		float Width = 256 / 32;
		float Height = 256 / 32;
		float Depth = 256 / 16;
		TRS.NumTiles = Width*Height*Depth;

		UINT RangeFlags = 0;
		UINT startOffset = 0;
		
		result = pContext->UpdateTileMappings(m_pTexture, 1, &coord, &TRS, pTilePool, 1, &RangeFlags, &startOffset, nullptr, D3D11_TILE_MAPPING_NO_OVERWRITE);
		if (FAILED(result))
		{
			VS_LOG_VERBOSE("Failed to map tiles");
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

