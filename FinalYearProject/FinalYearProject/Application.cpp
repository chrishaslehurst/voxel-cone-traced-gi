/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Application.h"
#include "Debugging.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Application* Application::s_pApp = nullptr;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Application::Run()
{
	MSG msg;
	bool bDone(false);

	ZeroMemory(&msg, sizeof(MSG));

	//Application loop
	while (!bDone)
	{
		//Handle windows messages
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		//if windows signals to end the app then exit..
		if (msg.message == WM_QUIT)
		{
			bDone = true;
		}
		else
		{
			//else, loop..
			if (!Update())
			{
				bDone = true;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Application::Shutdown()
{
	//Clean everything up
	if (m_pRenderer)
	{
		delete m_pRenderer;
		m_pRenderer = nullptr;
	}

	

	//Shutdown window
	ShutdownWindows();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Application::GetWidthAndHeight(int& width, int& height)
{ 
	RECT rect;
	
	GetWindowRect(m_hwnd, &rect);
	width = rect.right - rect.left;
	height = rect.bottom - rect.top;

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Application::Application()
	: m_pInput(nullptr)
	, m_pRenderer(nullptr)
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Application::~Application()
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Application::Initialise()
{
	int iScreenWidth( 0), iScreenHeight( 0);
	
	//Initialise the windows API
	InitialiseWindows(iScreenWidth, iScreenHeight);

	//Create input manager
	m_pInput = InputManager::Get();
	if (!m_pInput)
	{
		VS_LOG("Failed to get Input Manager");
		return false;
	}
	if (!m_pInput->Initialise(m_hInstance, m_hwnd, iScreenWidth, iScreenHeight))
	{
		MessageBox(m_hwnd, L"Could not initialize the input object.", L"Error", MB_OK);
		return false;
	}
	
	//Create renderer
	m_pRenderer = new Renderer;
	if (!m_pRenderer)
	{
		VS_LOG("Failed to create Renderer");
		return false;
	}
	
	if (!m_pRenderer->Initialise(iScreenWidth, iScreenHeight, m_hwnd))
	{
		VS_LOG("Failed to initialise Renderer")
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Application::Update()
{
	//Check if the user wants to quit..
	if (m_pInput->IsEscapePressed())
	{
		return false;
	}

	//update input manager
	if (!m_pInput->Update())
	{
		return false;
	}

	int mouseX, mouseY;
	//Get mouse location from input
	m_pInput->GetMouseLocation(mouseX, mouseY);


	//Update renderer
	return m_pRenderer->Update(m_hwnd);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Application::InitialiseWindows(int& iScreenWidth, int& iScreenHeight)
{
	WNDCLASSEX wc;
	DEVMODE dmScreenSettings;
	int posX, posY;


	// Get an external pointer to this object.
	s_pApp = this;

	// Get the instance of this application.
	m_hInstance = GetModuleHandle(NULL);

	// Give the application a name.
	m_sApplicationName = L"FYP: Voxel Cone Tracing";

	// Setup the windows class with default settings.
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = m_hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hIconSm = wc.hIcon;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = m_sApplicationName;
	wc.cbSize = sizeof(WNDCLASSEX);

	// Register the window class.
	RegisterClassEx(&wc);

	// Determine the resolution of the clients desktop screen.
	iScreenWidth = GetSystemMetrics(SM_CXSCREEN);
	iScreenHeight = GetSystemMetrics(SM_CYSCREEN);

	// Setup the screen settings depending on whether it is running in full screen or in windowed mode.
	if (FULL_SCREEN)
	{
		// If full screen set the screen to maximum size of the users desktop and 32bit.
		memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
		dmScreenSettings.dmSize = sizeof(dmScreenSettings);
		dmScreenSettings.dmPelsWidth = (unsigned long)iScreenWidth;
		dmScreenSettings.dmPelsHeight = (unsigned long)iScreenHeight;
		dmScreenSettings.dmBitsPerPel = 32;
		dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

		// Change the display settings to full screen.
		ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN);

		// Set the position of the window to the top left corner.
		posX = posY = 0;

		// Create the window with the screen settings and get the handle to it.
		m_hwnd = CreateWindowEx(WS_EX_APPWINDOW, m_sApplicationName, m_sApplicationName,
			WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP,
			posX, posY, iScreenWidth, iScreenHeight, NULL, NULL, m_hInstance, NULL);
	}
	else
	{
		// If windowed then set it to 1920x1080 resolution.
		iScreenWidth = 1280;
		iScreenHeight = 720;

		// Place the window in the middle of the screen.
		posX = (GetSystemMetrics(SM_CXSCREEN) - iScreenWidth) / 2;
		posY = (GetSystemMetrics(SM_CYSCREEN) - iScreenHeight) / 2;

		// Create the window with the screen settings and get the handle to it.
		m_hwnd = CreateWindowEx(WS_EX_APPWINDOW, m_sApplicationName, m_sApplicationName,
			WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
			posX, posY, iScreenWidth, iScreenHeight, NULL, NULL, m_hInstance, NULL);
	}

	

	// Bring the window up on the screen and set it as main focus.
	ShowWindow(m_hwnd, SW_SHOW);
	SetForegroundWindow(m_hwnd);
	SetFocus(m_hwnd);

	// Hide the mouse cursor.
	ShowCursor(false);

	return;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Application::ShutdownWindows()
{
	ShowCursor(true);
	if (FULL_SCREEN)
	{
		ChangeDisplaySettings(NULL, 0);
	}

	//Remove the window
	DestroyWindow(m_hwnd);
	m_hwnd = nullptr;

	UnregisterClass(m_sApplicationName, m_hInstance);
	m_hInstance = nullptr;

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK Application::MessageHandler(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hwnd, umsg, wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK WndProc(HWND hwnd, UINT umsg, WPARAM wParam, LPARAM lParam)
{
	switch (umsg)
	{
		//Check if the window is being destroyed
		case WM_DESTROY:
		case WM_CLOSE:
		{
			PostQuitMessage(0);
			return 0;
		}
		
		//Any other messages are passed to our message handler..
		default:
		{
			return Application::Get()->MessageHandler(hwnd, umsg, wParam, lParam);
		}
	}
}
