#ifndef POINT_LIGHT_H
#define POINT_LIGHT_H

#include <DirectXMath.h>
#include "OmnidirectionalShadowMap.h"

using namespace DirectX;

__declspec(align(16)) class PointLight
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

	PointLight();
	~PointLight();

	void SetDiffuseColour(float r, float g, float b, float a);
	void SetPosition(float x, float y, float z);
	void SetRange(float fRange) { m_fRange = fRange; } //Range is stored as the reciprocal for use in the shader..

	XMFLOAT4 GetDiffuseColour() { return m_vDiffuseColour; }
	XMFLOAT4 GetPosition() { return m_vPosition; }
	float GetRange() { return m_fRange; }
	float GetReciprocalRange() { return 1.f/m_fRange; }

	void AddShadowMap(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, HWND hwnd, float fScreenNear, float fScreenDepth);
	OmnidirectionalShadowMap* GetShadowMap() { return m_pShadowMap; }
private:

	XMFLOAT4 m_vDiffuseColour;
	XMFLOAT4 m_vPosition;
	float m_fRange;

	OmnidirectionalShadowMap* m_pShadowMap;

};

#endif //! LIGHT_H
