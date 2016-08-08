#ifndef CAMERA_H
#define CAMERA_H

#include <DirectXMath.h>
#include "AABB.h"

using namespace DirectX;

__declspec(align(16)) class Camera
{

public:
	
	void* operator new(size_t i)
	{
		return _mm_malloc(i, 16);
	}

		void operator delete(void* p)
	{
		_mm_free(p);
	}

	Camera();
	~Camera();

	void SetPosition(float x, float y, float z);
	void SetRotation(float x, float y, float z);

	XMFLOAT3 GetPosition();
	XMFLOAT3 GetRotation();


	void Update();
	void Render();
	void RenderBaseViewMatrix();
	void GetBaseViewMatrix(XMMATRIX& mBaseView);
	void GetViewMatrix(XMMATRIX& mViewMatrix);

	void CalculateViewFrustum(float fScreenDepth, XMMATRIX mProjectionMatrix);
	bool CheckPointInsidePlane(int index, float x, float y, float z);
	bool CheckBoundingBoxInsideViewFrustum(const AABB& boundingBox);
private:

	int m_iPrevMouseX, m_iPrevMouseY;

	float m_fCameraSpeed;

	XMFLOAT3 m_vPosition;
	XMFLOAT3 m_vRotation;

	XMMATRIX m_mViewMatrix;
	XMMATRIX m_mBaseView;
	XMMATRIX m_mRotationMatrix;

	XMFLOAT3 m_vForward;
	XMFLOAT3 m_vUpVector;
	XMFLOAT3 m_vLeftVector;
	
	XMFLOAT4 m_viewFrustumPlanes[6];

};

#endif // !CAMERA_H

