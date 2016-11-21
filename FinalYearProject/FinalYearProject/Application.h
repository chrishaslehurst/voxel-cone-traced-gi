#ifndef APPLICATION_H
#define APPLICATION_H

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define WIN32_LEAN_AND_MEAN

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "InputManager.h"
#include "Renderer.h"

#define TEST_MODE 1

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct TestType
{
	int iResolution;
	RenderMode eRenderMode;
};

class Application
{

public:
	
	static Application* Get()
	{
		bool bInit(true);
		if (!s_pApp)
		{
			s_pApp = new Application();
			bInit = s_pApp->Initialise();
		}
		
		return bInit ? s_pApp : nullptr;
	}

	void Run();
	void Shutdown();
	void GetWidthAndHeight(int& width, int& height);
	bool AppInFocus() { return GetFocus() == m_hwnd; }

	LRESULT CALLBACK MessageHandler(HWND, UINT, WPARAM, LPARAM);
	
private:

	static Application*		s_pApp;
	Application();
	~Application();

	int m_iScreenHeight;
	int m_iScreenWidth;
	int m_iTestIndex;
	std::vector<TestType> m_arrTests;

	bool Initialise();
	bool Update();
	void InitialiseWindows(int& width, int& height);
	void ShutdownWindows();


	LPCWSTR					m_sApplicationName;
	HINSTANCE				m_hInstance;
	HWND					m_hwnd;
	InputManager*			m_pInput;
	Renderer*				m_pRenderer;

};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif //!APPLICATION_H
