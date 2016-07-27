#ifndef ORTHO_WINDOW_H
#define ORTHO_WINDOW_H

#include <d3d11.h>
#include <DirectXMath.h>

using namespace DirectX;

class OrthoWindow
{
	__declspec(align(16)) struct VertexType
	{
		XMFLOAT3 vPosition;
		XMFLOAT2 vTexture;

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_mm_free(p);
		}
	};

public:
	OrthoWindow();
	~OrthoWindow();

	bool Initialize(ID3D11Device* pDevice, int windowWidth, int windowHeight);
	void Shutdown();
	void Render(ID3D11DeviceContext* pContext);

	int GetIndexCount();

private:
	
	bool InitialiseBuffers(ID3D11Device* pDevice, int windowWidth, int windowHeight);
	void ShutdownBuffers();
	void RenderBuffers(ID3D11DeviceContext* pContext);

	ID3D11Buffer *m_vertexBuffer;
	ID3D11Buffer* m_indexBuffer;
	int m_vertexCount, m_indexCount;

};


#endif // !ORTHO_WINDOW_H

