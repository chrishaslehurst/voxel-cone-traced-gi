#include "Camera.h"
#include "InputManager.h"
#include "Application.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Camera::Camera()
	: m_vPosition(XMFLOAT3(0.f,0.f,0.f))
	, m_vRotation(XMFLOAT3(0.f,0.f,0.f))
	, m_vUpVector(XMFLOAT3(0.f, 1.f, 0.f))
	, m_fCameraSpeed(5.f)
{
	
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
			m_vPosition.x += (m_vForward.x * m_fCameraSpeed);
			m_vPosition.y += (m_vForward.y * m_fCameraSpeed);
			m_vPosition.z += (m_vForward.z * m_fCameraSpeed);
		}
		if (pInput->IsKeyPressed(DIK_S))
		{
			m_vPosition.x -= (m_vForward.x * m_fCameraSpeed);
			m_vPosition.y -= (m_vForward.y * m_fCameraSpeed);
			m_vPosition.z -= (m_vForward.z * m_fCameraSpeed);
		}
		if (pInput->IsKeyPressed(DIK_A))
		{
			m_vPosition.x += (m_vLeftVector.x * m_fCameraSpeed);
			m_vPosition.y += (m_vLeftVector.y * m_fCameraSpeed);
			m_vPosition.z += (m_vLeftVector.z * m_fCameraSpeed);
		}
		if (pInput->IsKeyPressed(DIK_D))
		{
			m_vPosition.x -= (m_vLeftVector.x * m_fCameraSpeed);
			m_vPosition.y -= (m_vLeftVector.y * m_fCameraSpeed);
			m_vPosition.z -= (m_vLeftVector.z * m_fCameraSpeed);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Camera::Render()
{
	XMFLOAT3 f3lookAt;

	f3lookAt.x = 0.f;
	f3lookAt.y = 0.f;
	f3lookAt.z = 1.f;
	
	XMVECTOR up, pos, lookAt, right;
	up = XMLoadFloat3(&m_vUpVector);
	lookAt = XMLoadFloat3(&f3lookAt);
	pos = XMLoadFloat3(&m_vPosition);

	float yaw, pitch, roll;

	pitch = XMConvertToRadians(m_vRotation.x);
	yaw = XMConvertToRadians(m_vRotation.y);
	roll = XMConvertToRadians(m_vRotation.z);

	m_mRotationMatrix = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

	lookAt = XMVector3TransformCoord(lookAt, m_mRotationMatrix);
	XMStoreFloat3(&m_vForward, lookAt);
	right = XMVector3Cross(lookAt, up);
	XMStoreFloat3(&m_vLeftVector, right);


	lookAt = pos + lookAt;

	m_mViewMatrix = XMMatrixLookAtLH(pos, lookAt, up);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Camera::RenderBaseViewMatrix()
{
	XMFLOAT3 position = XMFLOAT3(0.f, 0.f, -1.f);
	XMFLOAT3 f3lookAt;

	f3lookAt.x = 0.f;
	f3lookAt.y = 0.f;
	f3lookAt.z = 1.f;

	XMVECTOR up, pos, lookAt;
	up = XMLoadFloat3(&m_vUpVector);
	lookAt = XMLoadFloat3(&f3lookAt);
	pos = XMLoadFloat3(&position);

	m_mBaseView = XMMatrixLookAtLH(pos, lookAt, up);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Camera::GetBaseViewMatrix(XMMATRIX& mBaseView)
{
	mBaseView = m_mBaseView;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Camera::GetViewMatrix(XMMATRIX& mViewMatrix)
{
	mViewMatrix = m_mViewMatrix;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
