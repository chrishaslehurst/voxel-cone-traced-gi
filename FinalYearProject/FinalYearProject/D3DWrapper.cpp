#include "D3DWrapper.h"
#include "Debugging.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

D3DWrapper::D3DWrapper()
	: m_pSwapChain(nullptr)
	, m_pDevice(nullptr)
	, m_pDeviceContext(nullptr)
	, m_pRenderTargetView(nullptr)
	, m_pDepthStencilBuffer(nullptr)
	, m_pDepthStencilState(nullptr)
	, m_pDepthStencilView(nullptr)
	, m_pRasterState(nullptr)
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

D3DWrapper::~D3DWrapper()
{
	Shutdown();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool D3DWrapper::Initialise(int iScreenWidth, int iScreenHeight, bool bVsyncEnabled, HWND hwnd, bool bFullScreenEnabled, float fScreenDepth, float fScreenNear)
{
	m_bVsyncEnabled = bVsyncEnabled;
	unsigned int numerator, denominator;

	//Get info from the graphics card and monitor..
	if (!QueryRefreshRateAndGPUInfo(iScreenWidth, iScreenHeight, numerator, denominator))
	{
		return false;
	}
	if (!SetUpSwapChainAndDevice(hwnd, bFullScreenEnabled, iScreenWidth, iScreenHeight, numerator, denominator))
	{
		return false;
	}
	if (!SetUpDepthBufferAndDepthStencilState(iScreenWidth, iScreenHeight))
	{
		return false;
	}
	if (!SetUpDepthStencilView())
	{
		return false;
	}
	if (!SetUpRasteriser())
	{
		return false;
	}
	SetUpViewPort(iScreenWidth, iScreenHeight);

	//Setup the projection matrix
	float fFieldOfView(XM_PI / 4.0f);
	float fScreenAspect((float)iScreenWidth / (float)iScreenHeight);

	m_mProjectionMatrix = XMMatrixPerspectiveFovLH(fFieldOfView, fScreenAspect, fScreenNear, fScreenDepth);
	
	//init the world matrix
	m_mWorldMatrix = XMMatrixIdentity();


	//Create ortho projection for 2D rendering
	m_mOrthoMatrix = XMMatrixOrthographicLH((float)iScreenWidth, (float)iScreenHeight, fScreenNear, fScreenDepth);

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void D3DWrapper::BeginScene(float r, float g, float b, float a)
{
	float colour[4];

	//Setup the colour to clear the buffer to
	colour[0] = r;
	colour[1] = g;
	colour[2] = b;
	colour[3] = a;

	//Clear the back buffer
	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, colour);

	//Clear the depth buffer
	m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.f, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void D3DWrapper::EndScene()
{
	//Present back buffer to screen as render is complete..
	if (m_bVsyncEnabled)
	{
		//Lock to refresh rate
		m_pSwapChain->Present(1, 0);
	}
	else
	{
		//Present as fast as possible
		m_pSwapChain->Present(0, 0);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ID3D11Device* D3DWrapper::GetDevice()
{
	return m_pDevice;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ID3D11DeviceContext* D3DWrapper::GetDeviceContext()
{
	return m_pDeviceContext;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void D3DWrapper::GetProjectionMatrix(XMMATRIX& mProjectionMatrix)
{
	mProjectionMatrix = m_mProjectionMatrix;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void D3DWrapper::GetWorldMatrix(XMMATRIX& mWorldMatrix)
{
	mWorldMatrix = m_mWorldMatrix;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void D3DWrapper::GetOrthoMatrix(XMMATRIX& mOrthoMatrix)
{
	mOrthoMatrix = m_mOrthoMatrix;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void D3DWrapper::GetVideoCardInfo(char* cardName, int& memory)
{
	strcpy_s(cardName, 128, m_videoCardDescription);
	memory = m_iVideoCardMemoryInMB;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void D3DWrapper::Shutdown()
{
	if (m_pSwapChain)
	{
		m_pSwapChain->SetFullscreenState(false, NULL);
	}
	if (m_pRasterState)
	{
		m_pRasterState->Release();
		m_pRasterState = nullptr;
	}
	if (m_pDepthStencilView)
	{
		m_pDepthStencilView->Release();
		m_pDepthStencilView = nullptr;
	}

	if (m_pDepthStencilState)
	{
		m_pDepthStencilState->Release();
		m_pDepthStencilState = nullptr;
	}

	if (m_pDepthStencilBuffer)
	{
		m_pDepthStencilBuffer->Release();
		m_pDepthStencilBuffer = nullptr;
	}

	if (m_pRenderTargetView)
	{
		m_pRenderTargetView->Release();
		m_pRenderTargetView = nullptr;
	}

	if (m_pDeviceContext)
	{
		m_pDeviceContext->Release();
		m_pDeviceContext = nullptr;
	}

	if (m_pDevice)
	{
		m_pDevice->Release();
		m_pDevice = nullptr;
	}

	if (m_pSwapChain)
	{
		m_pSwapChain->Release();
		m_pSwapChain = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool D3DWrapper::QueryRefreshRateAndGPUInfo(int iScreenWidth, int iScreenHeight, unsigned int& numerator, unsigned int& denominator)
{
	//Create directX graphics interface factory
	IDXGIFactory* pFactory;
	if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory)))
	{
		VS_LOG_VERBOSE("Failed to create DXGIFactory");
		return false;
	}

	//use the factory to create an adapter for the primary graphics interface (the gpu)
	IDXGIAdapter* pAdapter;
	if (FAILED(pFactory->EnumAdapters(0, &pAdapter)))
	{
		VS_LOG_VERBOSE("Failed to create DXGIAdapter");
		return false;
	}

	//the primary adapter output (monitor)
	IDXGIOutput* pAdapterOutput;
	if (FAILED(pAdapter->EnumOutputs(0, &pAdapterOutput)))
	{
		VS_LOG_VERBOSE("Failed to create DXGIAdapterOutput");
		return false;
	}

	// Get the number of modes that fit the DXGI_FORMAT_R8G8B8A8_UNORM display format for the monitor.
	unsigned int iNumModes;
	if (FAILED(pAdapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &iNumModes, nullptr)))
	{
		VS_LOG_VERBOSE("Failed to get the number of modes");
		return false;
	}

	//Create a list to hold all the possible display modes for this monitor/gpu combo
	DXGI_MODE_DESC* pDisplayModeList = new DXGI_MODE_DESC[iNumModes];
	if (!pDisplayModeList)
	{
		VS_LOG_VERBOSE("Failed to create DXGI Mode Desc list");
		return false;
	}

	//Now fill the display mode list
	if (FAILED(pAdapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &iNumModes, pDisplayModeList)))
	{
		VS_LOG_VERBOSE("Failed to fill the display modes list");
		return false;
	}

	//Go through the display modes and find the one that matches the screen width/heigh
	//When a match is found, store the numerator and denominator of the refresh rate..
	for (unsigned int i = 0; i < iNumModes; ++i)
	{
		if (pDisplayModeList[i].Width == (unsigned int)iScreenWidth)
		{
			if (pDisplayModeList[i].Height == (unsigned int)iScreenHeight)
			{
				numerator = pDisplayModeList[i].RefreshRate.Numerator;
				denominator = pDisplayModeList[i].RefreshRate.Denominator;
				break;
			}
		}
	}

	//Get the name of the gpu and the amount of gpu memory in MB
	DXGI_ADAPTER_DESC adapterDesc;
	if (FAILED(pAdapter->GetDesc(&adapterDesc)))
	{
		VS_LOG_VERBOSE("Failed to get adapter description")
			return false;
	}

	m_iVideoCardMemoryInMB = (int)(adapterDesc.DedicatedVideoMemory / 1024 / 1024);

	unsigned long long stringLength;
	if (wcstombs_s(&stringLength, m_videoCardDescription, 128, adapterDesc.Description, 128) != 0)
	{
		return false;
	}

	//We have the refresh rate and gpu info so the structures can be released..
	delete[] pDisplayModeList;
	pDisplayModeList = nullptr;

	pAdapterOutput->Release();
	pAdapterOutput = nullptr;

	pAdapter->Release();
	pAdapter = nullptr;

	pFactory->Release();
	pFactory = nullptr;

	return true;
}

bool D3DWrapper::SetUpSwapChainAndDevice(HWND hwnd, bool bFullScreenEnabled, int iScreenWidth, int iScreenHeight, unsigned int numerator, unsigned int denominator)
{
	//Init swap chain description
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));

	//set to a single back buffer
	swapChainDesc.BufferCount = 1;

	//Set width and height of the back buffer
	swapChainDesc.BufferDesc.Width = iScreenWidth;
	swapChainDesc.BufferDesc.Height = iScreenHeight;

	//Set 32 bit surface for the back buffer
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	//Set refresh rate for the back buffer
	if (m_bVsyncEnabled)
	{
		swapChainDesc.BufferDesc.RefreshRate.Numerator = numerator;
		swapChainDesc.BufferDesc.RefreshRate.Denominator = denominator;
	}
	else
	{
		swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
		swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	}

	//Set the usage of the back buffer
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	//Set the handle for the window we're rendring onto
	swapChainDesc.OutputWindow = hwnd;

	//Turn multisampling off
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;

	//Set fullscreen or windowed
	if (bFullScreenEnabled)
	{
		swapChainDesc.Windowed = false;
	}
	else
	{
		swapChainDesc.Windowed = true;
	}

	//Set the scan line ordering and scaling to unspecified
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	//Discard the back buffer contents after it's presented
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	//Don't set advanced flags..
	swapChainDesc.Flags = 0;

	//Set the feature level to DX12_1
	D3D_FEATURE_LEVEL featureLevel(D3D_FEATURE_LEVEL_12_1);

	//now the description is filled out, the swap chain can be created..
	//Pass D3D11_CREATE_DEVICE_DEBUG in instead of 0 to catch any problems..
	if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_DEBUG, &featureLevel, 1, D3D11_SDK_VERSION, &swapChainDesc, &m_pSwapChain, &m_pDevice, nullptr, &m_pDeviceContext)))
	{
		VS_LOG_VERBOSE("Failed to create device and swap chain");
		return false;
	}

	//Get ptr to the back buffer
	ID3D11Texture2D* pBackBuffer;
	if (FAILED(m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer)))
	{
		VS_LOG_VERBOSE("Failed to get back buffer pointer");
		return false;
	}

	//Create the render target view with the back buffer pointer
	if (FAILED(m_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pRenderTargetView)))
	{
		VS_LOG_VERBOSE("Failed to create render target view");
		return false;
	}

	//Release pointer to back buffer..
	pBackBuffer->Release();
	pBackBuffer = nullptr;
	
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool D3DWrapper::SetUpDepthBufferAndDepthStencilState(int iScreenWidth, int iScreenHeight)
{
	D3D11_TEXTURE2D_DESC depthBufferDesc;
	//Initialise the description of the depth buffer..
	ZeroMemory(&depthBufferDesc, sizeof(depthBufferDesc));

	//Setup the description of the depth buffer
	depthBufferDesc.Width = iScreenWidth;
	depthBufferDesc.Height = iScreenHeight;
	depthBufferDesc.MipLevels = 1;
	depthBufferDesc.ArraySize = 1;
	depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthBufferDesc.SampleDesc.Count = 1;
	depthBufferDesc.SampleDesc.Quality = 0;
	depthBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	depthBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthBufferDesc.CPUAccessFlags = 0;
	depthBufferDesc.MiscFlags = 0;

	//Create the texture for the depth buffer using the description..
	if (FAILED(m_pDevice->CreateTexture2D(&depthBufferDesc, nullptr, &m_pDepthStencilBuffer)))
	{
		VS_LOG_VERBOSE("Failed to created depth buffer texture");
		return false;
	}

	//Initialise the stencil state description
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

	//Setup the description of the stencil state
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
	depthStencilDesc.StencilEnable = true;
	depthStencilDesc.StencilReadMask = 0xFF;
	depthStencilDesc.StencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing.
	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Stencil operations if pixel is back-facing.
	depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	//Create depth stencil state
	if (FAILED(m_pDevice->CreateDepthStencilState(&depthStencilDesc, &m_pDepthStencilState)))
	{
		VS_LOG_VERBOSE("Failed to create the depth stencil state")
			return false;
	}
	m_pDeviceContext->OMSetDepthStencilState(m_pDepthStencilState, 1);

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool D3DWrapper::SetUpDepthStencilView()
{
	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
	ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));

	depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Texture2D.MipSlice = 0;

	//Create the depth stencil view
	if (FAILED(m_pDevice->CreateDepthStencilView(m_pDepthStencilBuffer, &depthStencilViewDesc, &m_pDepthStencilView)))
	{
		VS_LOG_VERBOSE("Failed to create depth stencil view");
		return false;
	}
	//bind render target view and depth stencil buffer to output render pipeline
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);

	return true;
}

bool D3DWrapper::SetUpRasteriser()
{
	//Setup the rasterizer
	D3D11_RASTERIZER_DESC rasterDesc;
	//Setup raster description which will determine how/which polys are drawn
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.CullMode = D3D11_CULL_BACK;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.ScissorEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;

	//Create the rasterizer state from the desc
	if (FAILED(m_pDevice->CreateRasterizerState(&rasterDesc, &m_pRasterState)))
	{
		VS_LOG_VERBOSE("Failed to create rasterizer state");
		return false;
	}

	m_pDeviceContext->RSSetState(m_pRasterState);
	return true;
}

void D3DWrapper::SetUpViewPort(int iScreenWidth, int iScreenHeight)
{
	D3D11_VIEWPORT viewport;
	viewport.Width = (float)iScreenWidth;
	viewport.Height = (float)iScreenHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;

	//Create the viewport
	m_pDeviceContext->RSSetViewports(1, &viewport);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
