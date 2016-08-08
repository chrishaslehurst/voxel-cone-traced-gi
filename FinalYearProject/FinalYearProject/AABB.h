#ifndef AABB_H
#define AABB_H

#include <DirectXMath.h>

using namespace DirectX;

__declspec(align(16)) class AABB
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

	AABB();
	~AABB();

	XMFLOAT3 Min;
	XMFLOAT3 Max;

};

#endif
