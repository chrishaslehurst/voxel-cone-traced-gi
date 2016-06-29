#ifndef POINT_LIGHT_H
#define POINT_LIGHT_H

#include <DirectXMath.h>

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
	void SetRange(float fRange) { m_fRange = 1.f/fRange; } //Range is stored as the reciprocal for use in the shader..

	XMFLOAT4 GetDiffuseColour() { return m_vDiffuseColour; }
	XMFLOAT4 GetPosition() { return m_vPosition; }
	float GetRange() { return m_fRange; }

private:

	XMFLOAT4 m_vDiffuseColour;
	XMFLOAT4 m_vPosition;
	float m_fRange;



};

#endif //! LIGHT_H
