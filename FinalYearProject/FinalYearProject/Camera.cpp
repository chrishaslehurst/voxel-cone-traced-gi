#include "Camera.h"
#include "InputManager.h"
#include "Application.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Camera::Camera()
	: m_vPosition(XMFLOAT3(0.f,0.f,0.f))
	, m_vRotation(XMFLOAT3(0.f,0.f,0.f))
	, m_vUpVector(XMFLOAT3(0.f, 1.f, 0.f))
	, m_fCameraSpeed(5.f)
	, m_bFollowingRoute(false)
	, m_bFinishedRouteThisFrame(false)
{
	m_arrRoute.push_back(XMFLOAT3(-1250, 200, 475));
	m_arrRoute.push_back(XMFLOAT3( 1150, 200, 475));
//	m_arrRoute.push_back(XMFLOAT3( 1150, 200, -450));
//	m_arrRoute.push_back(XMFLOAT3(-1250, 200, -450));
//	m_arrRoute.push_back(XMFLOAT3(-1250, 200, 475));
//	m_arrRoute.push_back(XMFLOAT3(-1250, 600, 475));
//	m_arrRoute.push_back(XMFLOAT3(1150, 600, 475));
//	m_arrRoute.push_back(XMFLOAT3(1150, 600, -450));
//	m_arrRoute.push_back(XMFLOAT3(-1250, 600, -450));
//	m_arrRoute.push_back(XMFLOAT3(-1250, 600, 475));
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

void Camera::TraverseRoute()
{
	if (!m_bFollowingRoute)
	{
		m_bFollowingRoute = true;
		m_vPosition = m_arrRoute[0];
		m_vRotation = XMFLOAT3(0.f, 0.f, 0.f);
		m_iCurrentRouteIndex = 1;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Camera::Update()
{
	if (!m_bFollowingRoute)
	{
		m_bFinishedRouteThisFrame = false;
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
	else
	{
		if (m_iCurrentRouteIndex < m_arrRoute.size())
		{
			XMVECTOR vOrigin, vDestination, vCurrentPos;
			vDestination = XMLoadFloat3(&m_arrRoute[m_iCurrentRouteIndex]);
			vOrigin = XMLoadFloat3(&m_arrRoute[m_iCurrentRouteIndex-1]);
			vCurrentPos = XMLoadFloat3(&m_vPosition);
			//if we are very close to destination..
			if (XMVector3LengthSq(vDestination - vCurrentPos).m128_f32[0] < 5 * 5)
			{
				m_iCurrentRouteIndex++; //go on to the next position
				if (m_iCurrentRouteIndex >= m_arrRoute.size())
				{
					//gone past the end of the route, stop!
					m_bFollowingRoute = false;
					m_bFinishedRouteThisFrame = true;
				}
			}
			else
			{
				XMVECTOR vDir = vDestination - vOrigin;
				XMStoreFloat3(&m_vPosition, vCurrentPos + vDir * 0.001f);
				m_vRotation = XMFLOAT3(0.f, m_vRotation.y + 1.f, 0.f);
			}
		}
	}
	RenderBaseViewMatrix();
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

void Camera::CalculateViewFrustum(float fScreenDepth, XMMATRIX mProjectionMatrix)
{
	XMFLOAT4X4 projection;
	XMStoreFloat4x4(&projection, mProjectionMatrix);

	float zMin = -projection._43 / projection._33;
	float r = fScreenDepth / (fScreenDepth - zMin);

	projection._33 = r;
	projection._43 = -r * zMin;

	XMMATRIX proj = XMLoadFloat4x4(&projection);
	XMMATRIX mFrustumMatrix = XMMatrixMultiply(m_mViewMatrix, proj);
	XMFLOAT4X4 frustumMatrix;
	XMStoreFloat4x4(&frustumMatrix, mFrustumMatrix);

	XMVECTOR plane;
	// Calculate near plane of frustum.
	m_viewFrustumPlanes[0].x = frustumMatrix._13;
	m_viewFrustumPlanes[0].y = frustumMatrix._23;
	m_viewFrustumPlanes[0].z = frustumMatrix._33;
	m_viewFrustumPlanes[0].w = frustumMatrix._43;

	plane = XMLoadFloat4(&m_viewFrustumPlanes[0]);
	plane = XMPlaneNormalize(plane);
	XMStoreFloat4(&m_viewFrustumPlanes[0], plane);

	// Calculate far plane of frustum.
	m_viewFrustumPlanes[1].x = frustumMatrix._14 - frustumMatrix._13;
	m_viewFrustumPlanes[1].y = frustumMatrix._24 - frustumMatrix._23;
	m_viewFrustumPlanes[1].z = frustumMatrix._34 - frustumMatrix._33;
	m_viewFrustumPlanes[1].w = frustumMatrix._44 - frustumMatrix._43;

	plane = XMLoadFloat4(&m_viewFrustumPlanes[1]);
	plane = XMPlaneNormalize(plane);
	XMStoreFloat4(&m_viewFrustumPlanes[1], plane);

	// Calculate left plane of frustum.
	m_viewFrustumPlanes[2].x = frustumMatrix._14 + frustumMatrix._11;
	m_viewFrustumPlanes[2].y = frustumMatrix._24 + frustumMatrix._21;
	m_viewFrustumPlanes[2].z = frustumMatrix._34 + frustumMatrix._31;
	m_viewFrustumPlanes[2].w = frustumMatrix._44 + frustumMatrix._41;
	plane = XMLoadFloat4(&m_viewFrustumPlanes[2]);
	plane = XMPlaneNormalize(plane);
	XMStoreFloat4(&m_viewFrustumPlanes[2], plane);

	// Calculate right plane of frustum.
	m_viewFrustumPlanes[3].x = frustumMatrix._14 - frustumMatrix._11;
	m_viewFrustumPlanes[3].y = frustumMatrix._24 - frustumMatrix._21;
	m_viewFrustumPlanes[3].z = frustumMatrix._34 - frustumMatrix._31;
	m_viewFrustumPlanes[3].w = frustumMatrix._44 - frustumMatrix._41;
	plane = XMLoadFloat4(&m_viewFrustumPlanes[3]);
	plane = XMPlaneNormalize(plane);
	XMStoreFloat4(&m_viewFrustumPlanes[3], plane);

	// Calculate top plane of frustum.
	m_viewFrustumPlanes[4].x = frustumMatrix._14 - frustumMatrix._12;
	m_viewFrustumPlanes[4].y = frustumMatrix._24 - frustumMatrix._22;
	m_viewFrustumPlanes[4].z = frustumMatrix._34 - frustumMatrix._32;
	m_viewFrustumPlanes[4].w = frustumMatrix._44 - frustumMatrix._42;
	plane = XMLoadFloat4(&m_viewFrustumPlanes[4]);
	plane = XMPlaneNormalize(plane);
	XMStoreFloat4(&m_viewFrustumPlanes[4], plane);

	// Calculate bottom plane of frustum.
	m_viewFrustumPlanes[5].x = frustumMatrix._14 + frustumMatrix._12;
	m_viewFrustumPlanes[5].y = frustumMatrix._24 + frustumMatrix._22;
	m_viewFrustumPlanes[5].z = frustumMatrix._34 + frustumMatrix._32;
	m_viewFrustumPlanes[5].w = frustumMatrix._44 + frustumMatrix._42;
	plane = XMLoadFloat4(&m_viewFrustumPlanes[5]);
	plane = XMPlaneNormalize(plane);
	XMStoreFloat4(&m_viewFrustumPlanes[5], plane);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Camera::CheckPointInsidePlane(int index, float x, float y, float z)
{
	XMFLOAT4 vPoint(x, y, z, 1.f);
	
	XMVECTOR point, plane;
	point = XMLoadFloat4(&vPoint);
	plane = XMLoadFloat4(&m_viewFrustumPlanes[index]);
	
	return (XMPlaneDotCoord(plane, point).m128_f32[0] >= 0.f);
}

bool Camera::CheckBoundingBoxInsideViewFrustum(const XMFLOAT3& vPos, const AABB& boundingBox)
{

	for (int i = 0; i < 6; i++)
	{
		//Check all the corners of the box to see if any are inside the frustum..
		if (CheckPointInsidePlane(i, boundingBox.Min.x + vPos.x, boundingBox.Min.y + vPos.y, boundingBox.Min.z + vPos.z))
		{
			continue;
		}
		if (CheckPointInsidePlane(i, boundingBox.Min.x + vPos.x, boundingBox.Max.y + vPos.y, boundingBox.Min.z + vPos.z))
		{
			continue;
		}
		if (CheckPointInsidePlane(i, boundingBox.Min.x + vPos.x, boundingBox.Min.y + vPos.y, boundingBox.Max.z + vPos.z))
		{
			continue;
		}
		if (CheckPointInsidePlane(i, boundingBox.Min.x + vPos.x, boundingBox.Max.y + vPos.y, boundingBox.Max.z + vPos.z))
		{
			continue;
		}
		if (CheckPointInsidePlane(i, boundingBox.Max.x + vPos.x, boundingBox.Min.y + vPos.y, boundingBox.Min.z + vPos.z))
		{
			continue;
		}
		if (CheckPointInsidePlane(i, boundingBox.Max.x + vPos.x, boundingBox.Max.y + vPos.y, boundingBox.Min.z + vPos.z))
		{
			continue;
		}
		if (CheckPointInsidePlane(i, boundingBox.Max.x + vPos.x, boundingBox.Min.y + vPos.y, boundingBox.Max.z + vPos.z))
		{
			continue;
		}
		if (CheckPointInsidePlane(i, boundingBox.Max.x + vPos.x, boundingBox.Max.y + vPos.y, boundingBox.Max.z + vPos.z))
		{
			continue;
		}
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
