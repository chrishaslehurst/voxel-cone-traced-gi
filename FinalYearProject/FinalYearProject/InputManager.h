#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#define DIRECTINPUT_VERSION 0x0800

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

#include <dinput.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class InputManager
{

public:
	static InputManager* Get()
	{
		if (!s_pInstance)
		{
			s_pInstance = new InputManager();
		}
		return s_pInstance;
	}

	bool Initialise(HINSTANCE hInstance, HWND hwnd, int screenWidth, int screenHeight);
	void Shutdown();
	bool Update();

	bool IsEscapePressed();
	void GetMouseLocation(int& x, int &y);
	bool IsKeyPressed(char input);

private:

	static InputManager* s_pInstance;

	InputManager();
	~InputManager();

	bool ReadKeyboard();
	bool ReadMouse();
	void ProcessInput();
	
	HWND m_hwnd;

	IDirectInput8* m_pDirectInput;
	IDirectInputDevice8* m_pKeyboard;
	IDirectInputDevice8* m_pMouse;

	unsigned char m_keyboardState[256];
	DIMOUSESTATE m_mouseState;

	int m_iScreenWidth, m_iScreenHeight;
	int m_iMouseX, m_iMouseY;

};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif //!INPUT_MANAGER_H
