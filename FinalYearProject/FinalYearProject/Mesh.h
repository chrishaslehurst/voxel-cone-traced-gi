#ifndef MESH_H
#define MESH_H

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <d3d11_3.h>
#include <DirectXMath.h>
#include <fstream>
#include "MaterialLibrary.h"

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

	struct SubMesh
	{
		ModelType*	  m_pModel;
		Material*	  m_pMaterial;

		ID3D11Buffer* m_pVertexBuffer;
		ID3D11Buffer* m_pIndexBuffer;
		int			  m_iVertexCount;
		int			  m_iTextureCoordCount;
		int			  m_iNormalCount;
		int			  m_iFaceCount;
		int			  m_iIndexCount;

		SubMesh() 
		: m_pVertexBuffer(nullptr)
		, m_pIndexBuffer(nullptr)
		, m_pMaterial(nullptr)
		, m_pModel(nullptr)
		{

		}
	};

	struct Face
	{
		int vIndex1, vIndex2, vIndex3;
		int tIndex1, tIndex2, tIndex3;
		int nIndex1, nIndex2, nIndex3;
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

	bool Initialise(ID3D11Device* pDevice, HWND hwnd, char* modelFilename);
	void Shutdown();
	void Render(ID3D11DeviceContext* pDeviceContext, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix, XMFLOAT3 vLightDirection, XMFLOAT4 vLightDiffuseColour);

	int GetIndexCount(int subMeshIndex);

	void SetMaterial(int subMeshIndex, Material* pMaterial) { m_arrSubMeshes[subMeshIndex].m_pMaterial = pMaterial; }

private:

	bool LoadModelFromTextFile(ID3D11Device* pDevice, HWND hwnd, char* filename);
	bool LoadModelFromObjFile(ID3D11Device* pDevice, HWND hwnd, char* filename);
	bool FindNumSubMeshes(char* filename);
	bool ReadObjFileCounts(char* filename);
	void ReleaseModel();
	bool InitialiseBuffers(int subMeshIndex, ID3D11Device* pDevice);
	void ShutdownBuffers();
	void RenderBuffers(int subMeshIndex, ID3D11DeviceContext* pDeviceContext);

	int m_iSubMeshCount;

	SubMesh* m_arrSubMeshes;
	MaterialLibrary* m_MatLib;
	
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // !MESH_H
