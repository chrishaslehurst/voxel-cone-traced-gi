#include "Camera.h"
#include "InputManager.h"
#include "Application.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Camera::Camera()
{
	m_vPosition.x = 0.f;
	m_vPosition.y = 0.f;
	m_vPosition.z = 0.f;
	m_vRotation.x = 0.f;
	m_vRotation.y = 0.f;
	m_vRotation.z = 0.f;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Camera::~Camera()
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Camera::SetPosition(float x, float y, float z)
{
	m_vPosition.x = x;
	m_vPosition.y = y;
	m_vPosition.z = z;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Camera::SetRotation(float x, float y, float z)
{
	m_vRotation.x = x;
	m_vRotation.y = y;
	m_vRotation.z = z;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DirectX::XMFLOAT3 Camera::GetPosition()
{
	return m_vPosition;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DirectX::XMFLOAT3 Camera::GetRotation()
{
	return m_vRotation;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Camera::Update()
{
	InputManager* pInput = InputManager::Get();
	if (pInput)
	{
		int iNewMouseX, iNewMouseY;
		pInput->GetMouseLocation(iNewMouseX, iNewMouseY);

		//Reset the cursor to the centre..
		int iWindowWidth(0), iWindowHeight(0);
		Application::Get()->GetWidthAndHeight(iWindowWidth, iWindowHeight);
		int iWindowCentreX = iWindowWidth / 2;
		int iWindowCentreY = iWindowHeight / 2;
		if (Application::Get()->AppInFocus())
		{
			SetCursorPos(iWindowCentreX, iWindowCentreY);
		}

		//Rotate the camera based on the mouse position
		SetRotation(m_vRotation.x + ((iNewMouseY - iWindowCentreY) * 0.1f), m_vRotation.y + ((iNewMouseX - iWindowCentreX) * 0.1f), m_vRotation.z);
		//constrain x rotation to stop camera turning upside down..
		if (m_vRotation.x > 89.f)
		{
			m_vRotation.x = 89.f;
		}
		else if (m_vRotation.x < -89.f)
		{
			m_vRotation.x = -89.f;
		}
		
		//Move the camera..
		if (pInput->IsKeyPressed(DIK_W))
		{
			m_vPosition.x += m_vForward.x;
			m_vPosition.y += m_vForward.y;
			m_vPosition.z += m_vForward.z;	
		}
		if (pInput->IsKeyPressed(DIK_S))
		{
			m_vPosition.x -= m_vForward.x;
			m_vPosition.y -= m_vForward.y;
			m_vPosition.z -= m_vForward.z;
		}
		if (pInput->IsKeyPressed(DIK_A))
		{
			SetPosition(m_vPosition.x- 1, m_vPosition.y, m_vPosition.z );
		}
		if (pInput->IsKeyPressed(DIK_D))
		{
			SetPosition(m_vPosition.x+1, m_vPosition.y, m_vPosition.z );
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Camera::Render()
{
	XMFLOAT3 f3up, f3lookAt;
	f3up.x = 0.f;
	f3up.y = 1.f;
	f3up.z = 0.f;

	f3lookAt.x = 0.f;
	f3lookAt.y = 0.f;
	f3lookAt.z = 1.f;
	
	XMVECTOR up, pos, lookAt;
	up = XMLoadFloat3(&f3up);
	lookAt = XMLoadFloat3(&f3lookAt);
	pos = XMLoadFloat3(&m_vPosition);

	float yaw, pitch, roll;

	pitch = XMConvertToRadians(m_vRotation.x);
	yaw = XMConvertToRadians(m_vRotation.y);
	roll = XMConvertToRadians(m_vRotation.z);

	m_mRotationMatrix = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

	lookAt = XMVector3TransformCoord(lookAt, m_mRotationMatrix);
	XMStoreFloat3(&m_vForward, lookAt);
	up = XMVector3TransformCoord(up, m_mRotationMatrix);

	lookAt = pos + lookAt;

	m_mViewMatrix = XMMatrixLookAtLH(pos, lookAt, up);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Camera::GetViewMatrix(XMMATRIX& mViewMatrix)
{
	mViewMatrix = m_mViewMatrix;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
