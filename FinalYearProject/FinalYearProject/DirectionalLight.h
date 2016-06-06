#ifndef DIRECTIONAL_LIGHT_H
#define DIRECTIONAL_LIGHT_H

#include <DirectXMath.h>

using namespace DirectX;

__declspec(align(16)) class DirectionalLight
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

	DirectionalLight();
	~DirectionalLight();

	void SetDiffuseColour(float r, float g, float b, float a);
	void SetDirection(float x, float y, float z);

	XMFLOAT4 GetDiffuseColour();
	XMFLOAT3 GetDirection();

private:

	XMFLOAT4 m_vDiffuseColour;
	XMFLOAT3 m_vDirection;

};

#endif //! LIGHT_H
