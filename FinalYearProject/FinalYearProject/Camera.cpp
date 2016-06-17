#include "Camera.h"
#include "InputManager.h"
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
		int newMouseX, newMouseY;
		pInput->GetMouseLocation(newMouseX, newMouseY);
		if (pInput->IsKeyPressed(DIK_W))
		{
			SetPosition(m_vPosition.x, m_vPosition.y, m_vPosition.z + 1);
		}
		if (pInput->IsKeyPressed(DIK_S))
		{
			SetPosition(m_vPosition.x, m_vPosition.y, m_vPosition.z - 1);
		}
		if (pInput->IsKeyPressed(DIK_A))
		{
			SetPosition(m_vPosition.x- 1, m_vPosition.y, m_vPosition.z );
		}
		if (pInput->IsKeyPressed(DIK_D))
		{
			SetPosition(m_vPosition.x+1, m_vPosition.y, m_vPosition.z );
		}
		if (pInput->IsKeyPressed(DIK_P))
		{
			SetPosition(m_vPosition.x, m_vPosition.y+1, m_vPosition.z);
		}
		if (pInput->IsKeyPressed(DIK_L))
		{
			SetPosition(m_vPosition.x, m_vPosition.y-1, m_vPosition.z);
		}
		if (pInput->IsKeyPressed(DIK_M))
		{
			SetRotation(m_vRotation.x , m_vRotation.y + 0.1f, m_vRotation.z);
		}
		if (pInput->IsKeyPressed(DIK_N))
		{
			SetRotation(m_vRotation.x, m_vRotation.y - 0.1f, m_vRotation.z);
		}

		if (newMouseX != m_iPrevMouseX)
		{
			SetRotation(m_vRotation.x, m_vRotation.y + ((newMouseX - m_iPrevMouseX) * 0.1f), m_vRotation.z);
		}
		//This will depend on the forward vector..
// 		if (newMouseY != m_iPrevMouseY)
// 		{
// 			SetRotation(m_vRotation.x + ((newMouseY - m_iPrevMouseY) * 0.001f), m_vRotation.y  , m_vRotation.z);
// 		}

		m_iPrevMouseX = newMouseX;
		m_iPrevMouseY = newMouseY;
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

	XMMATRIX rotation = XMMatrixRotationRollPitchYaw(pitch, yaw, roll);

	lookAt = XMVector3TransformCoord(lookAt, rotation);
	up = XMVector3TransformCoord(up, rotation);

	lookAt = pos + lookAt;

	m_mViewMatrix = XMMatrixLookAtLH(pos, lookAt, up);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Camera::GetViewMatrix(XMMATRIX& mViewMatrix)
{
	mViewMatrix = m_mViewMatrix;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
