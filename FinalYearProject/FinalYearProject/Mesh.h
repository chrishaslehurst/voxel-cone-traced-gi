#ifndef MESH_H
#define MESH_H

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <d3d11_3.h>
#include <DirectXMath.h>
#include <fstream>
#include "MaterialLibrary.h"
#include "Material.h"
#include <vector>

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
		XMFLOAT3 tangent;
		XMFLOAT3 binormal;

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

	__declspec(align(16)) struct ModelType
	{
		XMFLOAT3 pos;
		XMFLOAT2 tex;
		XMFLOAT3 norm;
		XMFLOAT3 tangent;
		XMFLOAT3 binormal;

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_mm_free(p);
		}
	};

	struct SubMesh
	{
		std::vector<ModelType>	  m_arrModel;
		Material*				  m_pMaterial;

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
		, m_iVertexCount(0)
		, m_iFaceCount(0)
		, m_iNormalCount(0)
		, m_iIndexCount(0)
		, m_iTextureCoordCount(0)
		{
		}
	};

	struct Face
	{
		int vIndex[3];
		int tIndex[3];
		int nIndex[3];
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
	void Render(ID3D11DeviceContext* pDeviceContext, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix, XMFLOAT3 vLightDirection, XMFLOAT4 vLightDiffuseColour, XMFLOAT4 vAmbientColour, XMFLOAT3 vCameraPos, XMFLOAT4 vPointLightPos[], PointLight arrPointLights[]);

	int GetIndexCount(int subMeshIndex);

	void SetMaterial(int subMeshIndex, Material* pMaterial) { m_arrSubMeshes[subMeshIndex]->m_pMaterial = pMaterial; }

private:

	bool LoadModelFromTextFile(ID3D11Device* pDevice, HWND hwnd, char* filename);
	bool LoadModelFromObjFile(ID3D11Device* pDevice, HWND hwnd, char* filename);
	bool FindNumSubMeshes(char* filename);
	bool ReadObjFileCounts(char* filename);
	void ReleaseModel();
	bool InitialiseBuffers(int subMeshIndex, ID3D11Device* pDevice);
	void ShutdownBuffers();
	void RenderBuffers(int subMeshIndex, ID3D11DeviceContext* pDeviceContext);

	void CalculateModelVectors();

	int m_iSubMeshCount;
	int m_iTotalVerticesCount;
	int m_iTotalTextureCoordCount;
	int m_iTotalNormalCount;
	std::vector<SubMesh*> m_arrSubMeshes;
	MaterialLibrary* m_MatLib;
	
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // !MESH_H
