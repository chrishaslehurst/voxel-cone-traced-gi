#ifndef MESH_H
#define MESH_H

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <d3d11_3.h>
#include <DirectXMath.h>
#include <fstream>
#include "MaterialLibrary.h"
#include "Material.h"
#include <vector>
#include "D3DWrapper.h"
#include "AABB.h"
#include "Camera.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace DirectX;
using namespace std;

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
	int			  m_iIndexCount;

	AABB		  m_BoundingBox;
	float		  m_fDistanceToCamera;
	int			  m_iBufferIndex;

	void CalculateBoundingBox();
	void CalculateDistanceToCamera(Camera* pCamera);

	bool operator<(const SubMesh& A) const
	{
		return m_fDistanceToCamera < A.m_fDistanceToCamera;
	}

	SubMesh()
		: m_pVertexBuffer(nullptr)
		, m_pIndexBuffer(nullptr)
		, m_pMaterial(nullptr)
		, m_iVertexCount(0)
		, m_iIndexCount(0)
	{
	}

	int GetNumPolys() { return m_iVertexCount / 3; }
};

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

	bool InitialiseFromObj(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, HWND hwnd, char* modelFilename);
	bool InitialiseCubeFromTxt(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, HWND hwnd);
	void Shutdown();	
	void RenderToBuffers(ID3D11DeviceContext* pDeviceContext, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix, Camera* pCamera);
	
	void RenderShadows(ID3D11DeviceContext* pDeviceContext, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix, XMFLOAT3 vLightDirection, XMFLOAT4 vLightDiffuseColour, XMFLOAT4 vAmbientColour, XMFLOAT3 vCameraPos);
	void RenderBuffers(int subMeshIndex, ID3D11DeviceContext* pDeviceContext);
	const int GetIndexCount(int subMeshIndex) const;

	void SetMaterial(int subMeshIndex, Material* pMaterial) { m_arrSubMeshes[subMeshIndex]->m_pMaterial = pMaterial; }
	void ReloadShaders(ID3D11Device* pDevice, HWND hwnd);

	const AABB& GetWholeModelAABB() const { return m_WholeModelBounds; }

	const std::vector<SubMesh*>& GetMeshArray() { return m_arrSubMeshes; }
private:

	bool LoadCubeFromTextFile(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, HWND hwnd);
	bool LoadModelFromObjFile(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, HWND hwnd, char* filename);
	void ReleaseModel();
	bool InitialiseBuffers(int subMeshIndex, ID3D11Device* pDevice);
	void ShutdownBuffers();
	

	void CalculateModelVectors();

	AABB m_WholeModelBounds;

	std::vector<SubMesh*> m_arrSubMeshes;
	std::vector<SubMesh*> m_arrMeshesToRender;
	MaterialLibrary* m_pMatLib;
	
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // !MESH_H
