#include "InputManager.h"
#include "Debugging.h"

InputManager* InputManager::s_pInstance = nullptr;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

InputManager::InputManager()
	: m_pDirectInput(nullptr)
	, m_pKeyboard(nullptr)
	, m_pMouse(nullptr)
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

InputManager::~InputManager()
{
	Shutdown();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputManager::Initialise(HINSTANCE hInstance, HWND hwnd, int screenWidth, int screenHeight)
{
	HRESULT res;

	m_iScreenWidth = screenWidth;
	m_iScreenHeight = screenHeight;

	//Init mouse location
	m_iMouseX = 0;
	m_iMouseY = 0;

	//Initialise main direct input interface
	res = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&m_pDirectInput, nullptr);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to create Direct Input interface");
		return false;
	}

	//Init dinput interface for the keyboard
	res = m_pDirectInput->CreateDevice(GUID_SysKeyboard, &m_pKeyboard, nullptr);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to init keyboard interface");
		return false;
	}

	//Set the data format
	res = m_pKeyboard->SetDataFormat(&c_dfDIKeyboard);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Unable to set keyboard data format");
		return false;
	}

	//Set the cooperative level of this keyboard to not share with other programs..
	//TODO: In future make this share, then check for losing/reaquiring focus etc..
	res = m_pKeyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_EXCLUSIVE);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Unable to set keyboard cooperative level");
		return false;
	}

	//Now aquire the keyboard for our use..
	res = m_pKeyboard->Acquire();
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to acquire keyboard");
		return false;
	}

	//Init the direct input interface for the mouse
	res = m_pDirectInput->CreateDevice(GUID_SysMouse, &m_pMouse, nullptr);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Unable to create direct input interface for mouse");
		return false;
	}

	//Set data format for the mouse interface
	res = m_pMouse->SetDataFormat(&c_dfDIMouse);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Unable to set data format for mouse");
		return false;
	}

	//Set the cooperative level of the mouse to share with other programs
	res = m_pMouse->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to set coop level of mouse");
		return false;
	}

	// Acquire the mouse.
	res = m_pMouse->Acquire();
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to acquire the mouse");
		return false;
	}

	return true;

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void InputManager::Shutdown()
{
	if (m_pMouse)
	{
		m_pMouse->Unacquire();
		m_pMouse->Release();
		m_pMouse = nullptr;
	}

	if (m_pKeyboard)
	{
		m_pKeyboard->Unacquire();
		m_pKeyboard->Release();
		m_pKeyboard = nullptr;
	}

	//Release the main interface to direct input
	if (m_pDirectInput)
	{
		m_pDirectInput->Release();
		m_pDirectInput = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputManager::Update()
{
	if (!ReadKeyboard())
	{
		return false;
	}

	if (!ReadMouse())
	{
		return false;
	}

	ProcessInput();

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputManager::IsEscapePressed()
{
	if (m_keyboardState[DIK_ESCAPE] & 0x80)
	{
		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void InputManager::GetMouseLocation(int& x, int &y)
{
	x = m_iMouseX;
	y = m_iMouseY;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputManager::IsKeyPressed(char input)
{
	if (m_keyboardState[input] & 0xFF)
	{
		return true;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputManager::ReadKeyboard()
{
	HRESULT res;
	//Read the keyboard device..
	res = m_pKeyboard->GetDeviceState(sizeof(m_keyboardState), (LPVOID)&m_keyboardState);
	if (FAILED(res))
	{
		// If the keyboard lost focus or was not acquired then try to get control back.
		if ((res == DIERR_INPUTLOST) || (res == DIERR_NOTACQUIRED))
		{
			m_pKeyboard->Acquire();
		}
		else
		{
			VS_LOG_VERBOSE("Failed to get keyboard device state");
			return false;
		}
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InputManager::ReadMouse()
{
	HRESULT res;
	res = m_pMouse->GetDeviceState(sizeof(DIMOUSESTATE), (LPVOID)&m_mouseState);
	if (FAILED(res))
	{
		//If the mouse lost focus or was not acquired, try to get control back..
		if ((res == DIERR_INPUTLOST) || (res == DIERR_NOTACQUIRED))
		{
			m_pMouse->Acquire();
		}
		else
		{
			VS_LOG_VERBOSE("Failed to get mouse device state");
			return false;
		}
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void InputManager::ProcessInput()
{
	// Update location of mouse from change of mouse location during the frame.
	m_iMouseX += m_mouseState.lX;
	m_iMouseY += m_mouseState.lY;

	// Ensure the mouse location doesn't exceed the screen width or height.
 	if (m_iMouseX < 0) { m_iMouseX = 0; }
 	if (m_iMouseY < 0) { m_iMouseY = 0; }

	if (m_iMouseX > m_iScreenWidth) { m_iMouseX = m_iScreenWidth; }
 	if (m_iMouseY > m_iScreenHeight) { m_iMouseY = m_iScreenHeight; }

	return;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

