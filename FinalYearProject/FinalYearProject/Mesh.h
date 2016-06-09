#ifndef MESH_H
#define MESH_H

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <d3d11_3.h>
#include <DirectXMath.h>
#include <fstream>
#include "Material.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace DirectX;
using namespace std;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

__declspec(align(16)) class Mesh
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//The vertex type definition.. __declspec(align(16)) needed where XMFLOAT's are stored in the class
	__declspec(align(16)) struct VertexType
	{
		XMFLOAT3 position;
		XMFLOAT3 normal;
		XMFLOAT2 texture;
		XMFLOAT4 color;

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_mm_free(p);
		}
	};
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	struct ModelType
	{
		float x, y, z;
		float tu, tv;
		float nx, ny, nz;
	};

public:
	void* operator new(size_t i)
	{
		return _mm_malloc(i, 16);
	}

		void operator delete(void* p)
	{
		_mm_free(p);
	}

	Mesh();
	~Mesh();

	bool Initialise(ID3D11Device* pDevice, char* modelFilename, Material* pMaterial = nullptr);
	void Shutdown();
	void Render(ID3D11DeviceContext* pDeviceContext, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix, XMFLOAT3 vLightDirection, XMFLOAT4 vLightDiffuseColour);

	int GetIndexCount();

	void SetMaterial(Material* pMaterial) { m_pMaterial = pMaterial; }

private:

	bool LoadModel(char* filename);
	void ReleaseModel();
	bool InitialiseBuffers(ID3D11Device* pDevice);
	void ShutdownBuffers();
	void RenderBuffers(ID3D11DeviceContext* pDeviceContext);

	ID3D11Buffer* m_pVertexBuffer;
	ID3D11Buffer* m_pIndexBuffer;
	int			  m_iVertexCount;
	int			  m_iIndexCount;

	Material*	  m_pMaterial;

	ModelType*	  m_pModel;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // !MESH_H
