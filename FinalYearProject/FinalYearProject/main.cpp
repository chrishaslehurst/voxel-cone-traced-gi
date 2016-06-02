/**/

#include "Application.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pScmdline, int iCmdshow)
{
	Application* pApp;
	//Create the app
	pApp = Application::Get();
	if (!pApp)
	{
		return 0;
	}

	//Initialize and run the app..
	if (pApp)
	{
		pApp->Run();
	}

	//Cleanup
	if (pApp)
	{
		pApp->Shutdown();
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////