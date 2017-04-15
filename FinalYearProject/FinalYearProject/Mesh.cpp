#include "Mesh.h"
#include "Debugging.h"
#include "Timer.h"
#include <sstream>
#include "DebugLog.h"
#include "InputManager.h"

bool SortByDistanceToCameraAscending(const SubMesh* lhs, const SubMesh* rhs) { return lhs->m_fDistanceToCamera < rhs->m_fDistanceToCamera; }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Mesh::Mesh()
	: m_pMatLib(nullptr)
	, m_bIsPatrolling(false)
	, m_iCurrentPatrolIndex(0)
{
	m_mWorldMat = XMMatrixIdentity();
	m_mScaleMat = XMMatrixIdentity();
	m_vWorldPos = XMFLOAT3(0.f, 0.f, 0.f);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Mesh::~Mesh()
{
	Shutdown();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Mesh::InitialiseFromObj(ID3D11Device3* pDevice, ID3D11DeviceContext3* pContext, HWND hwnd, char* filename)
{
	//Load in the model data
	if (!LoadModelFromObjFile(pDevice, pContext, hwnd, filename))
	{
		return false;
	}

	//Calculate the binormals and tangent vectors
	CalculateModelVectors();
	
	m_WholeModelBounds.Min = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
	m_WholeModelBounds.Max = XMFLOAT3(0.f, 0.f, 0.f);

	//Initialise the buffers and calculate bounding boxes
	bool result(true);
	for (int i = 0; i < m_arrSubMeshes.size(); i++)
	{
		m_arrSubMeshes[i]->CalculateBoundingBox();

		//Check the new bounding box to construct the aabb for the whole model..
		if (m_arrSubMeshes[i]->m_BoundingBox.Min.x < m_WholeModelBounds.Min.x)
		{
			m_WholeModelBounds.Min.x = m_arrSubMeshes[i]->m_BoundingBox.Min.x;
		}
		if (m_arrSubMeshes[i]->m_BoundingBox.Min.y < m_WholeModelBounds.Min.y)
		{
			m_WholeModelBounds.Min.y = m_arrSubMeshes[i]->m_BoundingBox.Min.y;
		}
		if (m_arrSubMeshes[i]->m_BoundingBox.Min.z < m_WholeModelBounds.Min.z)
		{
			m_WholeModelBounds.Min.z = m_arrSubMeshes[i]->m_BoundingBox.Min.z;
		}
		if (m_arrSubMeshes[i]->m_BoundingBox.Max.x > m_WholeModelBounds.Max.x)
		{
			m_WholeModelBounds.Max.x = m_arrSubMeshes[i]->m_BoundingBox.Max.x;
		}
		if (m_arrSubMeshes[i]->m_BoundingBox.Max.y > m_WholeModelBounds.Max.y)
		{
			m_WholeModelBounds.Max.y = m_arrSubMeshes[i]->m_BoundingBox.Max.y;
		}
		if (m_arrSubMeshes[i]->m_BoundingBox.Max.z > m_WholeModelBounds.Max.z)
		{
			m_WholeModelBounds.Max.z = m_arrSubMeshes[i]->m_BoundingBox.Max.z;
		}

		result = InitialiseBuffers(i, pDevice);
		if (!result)
		{
			break;
		}
	}
	return result;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::Shutdown()
{
	//Release buffers
	ShutdownBuffers();

	ReleaseModel();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::RenderToBuffers(ID3D11DeviceContext3* pDeviceContext, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix, Camera* pCamera)
{
	m_arrMeshesToRender.clear();
	//Put the vertex and index buffers in the graphics pipeline so they can be drawn
	
	//TODO: This could be tidier.. bit of a hack to only set the shader once since they all use the same one..
	m_arrSubMeshes[0]->m_pMaterial->SetShadersAndSamplers(pDeviceContext);
	m_arrSubMeshes[0]->m_pMaterial->SetPerFrameShaderParameters(pDeviceContext, mWorldMatrix, mViewMatrix, mProjectionMatrix);

	int iModelsRenderedInGBufferPass = 0;
	int iNumPolysRenderedInGBufferPass = 0;
	for (int i = 0; i < m_arrSubMeshes.size(); i++)
	{
		
		if (!m_arrSubMeshes[i]->m_pMaterial->UsesAlphaMaps() && pCamera->CheckBoundingBoxInsideViewFrustum(m_vWorldPos, m_arrSubMeshes[i]->m_BoundingBox))
		{
			iModelsRenderedInGBufferPass++;
			iNumPolysRenderedInGBufferPass += m_arrSubMeshes[i]->GetNumPolys();
			m_arrSubMeshes[i]->CalculateDistanceToCamera(pCamera);
			m_arrSubMeshes[i]->m_iBufferIndex = i;
			m_arrMeshesToRender.push_back(m_arrSubMeshes[i]);
		}
	}

	//This sorts the meshes we want to render by their distance to the camera, to allow for early z discards.
	std::sort(m_arrMeshesToRender.begin(), m_arrMeshesToRender.end(), SortByDistanceToCameraAscending);
	for (int i = 0; i < m_arrMeshesToRender.size(); i++)
	{
		RenderBuffers(m_arrMeshesToRender[i]->m_iBufferIndex, pDeviceContext);

		if (!m_arrMeshesToRender[i]->m_pMaterial->Render(pDeviceContext, m_arrMeshesToRender[i]->m_iIndexCount, mWorldMatrix, mViewMatrix, mProjectionMatrix))
		{
			VS_LOG_VERBOSE("Unable to render object with shader");
		}
	}

}


void Mesh::RenderShadows(ID3D11DeviceContext3* pDeviceContext, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix, XMFLOAT3 vLightDirection, XMFLOAT4 vLightDiffuseColour, XMFLOAT4 vAmbientColour, XMFLOAT3 vCameraPos)
{
	//Render meshes to the shadow maps..
	for (int i = 0; i < NUM_LIGHTS; i++)
	{
		PointLight* pLight = LightManager::Get()->GetPointLight(i);
		if (pLight)
		{
			OmnidirectionalShadowMap* pShadowMap = pLight->GetShadowMap();
			if (pShadowMap)
			{
				pShadowMap->SetRenderOutputToShadowMap(pDeviceContext);
				pShadowMap->SetShaderParams(pDeviceContext, pLight->GetPosition(), pLight->GetRange(), mWorldMatrix);
				pShadowMap->SetRenderStart(pDeviceContext);
				for (int i = 0; i < m_arrSubMeshes.size(); i++)
				{
					RenderBuffers(i, pDeviceContext);
					pShadowMap->Render(pDeviceContext, m_arrSubMeshes[i]->m_iIndexCount);
				}
				pShadowMap->SetRenderFinished(pDeviceContext);
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const int Mesh::GetIndexCount(int subMeshIndex) const
{
	return m_arrSubMeshes[subMeshIndex]->m_iIndexCount;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::ReloadShaders(ID3D11Device3* pDevice, HWND hwnd)
{
	if (m_pMatLib)
	{
		m_pMatLib->ReloadShaders(pDevice, hwnd);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::SetMeshScale(float fScaleFactor)
{
	m_mScaleMat *= XMMatrixScaling(fScaleFactor, fScaleFactor, fScaleFactor);
	
	//Also need to scale the bounding boxes..
	m_WholeModelBounds.Max.x *= fScaleFactor;
	m_WholeModelBounds.Max.y *= fScaleFactor;
	m_WholeModelBounds.Max.z *= fScaleFactor;
	m_WholeModelBounds.Min.x *= fScaleFactor;
	m_WholeModelBounds.Min.y *= fScaleFactor;
	m_WholeModelBounds.Min.z *= fScaleFactor;
	for (int i = 0; i < m_arrSubMeshes.size(); i++)
	{
		m_arrSubMeshes[i]->m_BoundingBox.Max.x *= fScaleFactor;
		m_arrSubMeshes[i]->m_BoundingBox.Max.y *= fScaleFactor;
		m_arrSubMeshes[i]->m_BoundingBox.Max.z *= fScaleFactor;
		m_arrSubMeshes[i]->m_BoundingBox.Min.x *= fScaleFactor;
		m_arrSubMeshes[i]->m_BoundingBox.Min.y *= fScaleFactor;
		m_arrSubMeshes[i]->m_BoundingBox.Min.z *= fScaleFactor;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::SetMeshPosition(XMFLOAT3 vTranslation)
{
	m_vWorldPos = vTranslation;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::StartPatrol()
{
	m_bIsPatrolling = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::AddPatrolPoint(const XMFLOAT3& vPos)
{
	m_arrPatrolRoute.push_back(vPos);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::Update()
{
	if (m_bIsPatrolling)
	{
		XMVECTOR vDest, vOrigin, vDir, vCurrentPos;
		vCurrentPos = XMLoadFloat3(&m_vWorldPos);
		vDest = XMLoadFloat3(&m_arrPatrolRoute[((m_iCurrentPatrolIndex + 1) % m_arrPatrolRoute.size())]);
		vOrigin = XMLoadFloat3(&m_arrPatrolRoute[m_iCurrentPatrolIndex]);

		vDir = vDest - vOrigin;
		vDir = XMVector3Normalize(vDir);
		vCurrentPos += vDir * 5;
		if (XMVector3LengthSq(vDest - vCurrentPos).m128_f32[0] < 5.f)
		{
			m_iCurrentPatrolIndex = (m_iCurrentPatrolIndex + 1) % m_arrPatrolRoute.size();
		}
		XMStoreFloat3(&m_vWorldPos, vCurrentPos);
	}
	UpdateMatrices();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::UpdateMatrices()
{
	m_mWorldMat = XMMatrixIdentity() * m_mScaleMat;
	m_mWorldMat = m_mWorldMat * XMMatrixTranslation(m_vWorldPos.x, m_vWorldPos.y, m_vWorldPos.z);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Mesh::LoadModelFromObjFile(ID3D11Device3* pDevice, ID3D11DeviceContext3* pContext, HWND hwnd, char* filename)
{
	m_pMatLib = new MaterialLibrary;
	if (!m_pMatLib)
	{
		VS_LOG_VERBOSE("Unable to create new material library");
		return false;
	}
	
	double dStartTime = Timer::Get()->GetCurrentTime();
	ifstream fin;
	fin.open(filename);

	//did the file open?
	if (fin.fail())
	{
		VS_LOG_VERBOSE("Failed to load model, could not open file..");
		return false;
	}

	std::vector<XMFLOAT3> Verts;
	std::vector<XMFLOAT2> TexCoords;
	std::vector<XMFLOAT3> Normals;

	Verts.reserve(10000);
	TexCoords.reserve(200000);
	Normals.reserve(300000);
	
	string s;
	
	double dFaceReadTime = 0;
	double dModelCreateTime = 0;

	int iObjectIndex( -1);

	int iFaceIndex = 0;
	char checkChar;
	//use this to read in floats and then atof to convert. sstream >> float is very slow..
	char nums[64];
	//fin >> s;
	while (fin)
	{
		checkChar = fin.get();
		switch (checkChar)
		{
		case '#':
		case 'g':
		case 's':
			checkChar = fin.get();
			while (checkChar != '\n')
				checkChar = fin.get();
			break;
		case 'v':
		{
			checkChar = fin.get();
			if (checkChar == ' ')
			{
				//vertex
				float x, y, z;
				fin >> nums;
				x = atof(nums);
				fin >> nums;
				y = atof(nums);
				fin >> nums;
				z = atof(nums);
				// Invert the Z vertex to change to left hand system.
				Verts.push_back(XMFLOAT3(x, y, -z));
			}
			else if (checkChar == 't')
			{
				//texture coord
				float u, v;
				fin >> nums;
				u = atof(nums);
				fin >> nums;
				v = atof(nums);
				//Invert the v texture coord to left hand system
				TexCoords.push_back(XMFLOAT2(u, 1.f - v));
			}
			else if (checkChar == 'n')
			{
				//normal coord
				float x, y, z;
				fin >> nums;
				x = atof(nums);
				fin >> nums;
				y = atof(nums);
				fin >> nums;
				z = atof(nums);
				//Invert the z normal to change to left hand system
				Normals.push_back(XMFLOAT3(x, y, -z));
			}
			break;
		}
		case 'f':
		{
			double dFaceReadStartTime = Timer::Get()->GetCurrentTime();

			Face f;

			fin >> f.vIndex[2] >> checkChar >> f.tIndex[2] >> checkChar >> f.nIndex[2]
				>> f.vIndex[1] >> checkChar >> f.tIndex[1] >> checkChar >> f.nIndex[1]
				>> f.vIndex[0] >> checkChar >> f.tIndex[0] >> checkChar >> f.nIndex[0];

			int iModelIndex = iFaceIndex * 3;
			for (int j = 0; j < 3; j++)
			{
				m_arrSubMeshes[iObjectIndex]->m_arrModel.push_back(ModelType());
				m_arrSubMeshes[iObjectIndex]->m_arrModel[iModelIndex + j].pos = Verts[f.vIndex[j] - 1];
				m_arrSubMeshes[iObjectIndex]->m_arrModel[iModelIndex + j].tex = TexCoords[f.tIndex[j] - 1];
				m_arrSubMeshes[iObjectIndex]->m_arrModel[iModelIndex + j].norm = Normals[f.nIndex[j] - 1];
			}



			iFaceIndex++;
			double dFaceReadEndTime = Timer::Get()->GetCurrentTime();
			dFaceReadTime += dFaceReadEndTime - dFaceReadStartTime;
			break;
		}
		case 'u':
		{
			fin >> s;
			if (s == "semtl")
			{
				//get the mat name
				fin >> s;
				double dModelCreateStartTime = Timer::Get()->GetCurrentTime();
				
				iObjectIndex++;
				iFaceIndex = 0;
				m_arrSubMeshes.push_back(new SubMesh());
				
				m_arrSubMeshes[iObjectIndex]->m_pMaterial = m_pMatLib->GetMaterial(s);
				double dModelCreateEndTime = Timer::Get()->GetCurrentTime();
				dModelCreateTime += dModelCreateEndTime - dModelCreateStartTime;
			}
 			else
 			{
 				checkChar = fin.get();
 				while (checkChar != '\n')
 					checkChar = fin.get();
 				break;
 			}
			break;
		}
		case 'm':
		{
			fin >> s;
			if (s == "tllib")
			{
				string sMaterialLibName;
				fin >> sMaterialLibName;

				sMaterialLibName = "../Assets/Shaders/" + sMaterialLibName;
				m_pMatLib->LoadMaterialLibrary(pDevice, pContext, hwnd, sMaterialLibName.c_str());
			}
 			else
 			{
 				checkChar = fin.get();
 				while (checkChar != '\n')
 					checkChar = fin.get();
 				break;
 			}
			break;
		}
		}
	}

	double dEndTime = Timer::Get()->GetCurrentTime();
	stringstream output;
	output << "Time to process data: " << (dEndTime - dStartTime) << '\n' << "Time To read in faces: " << dFaceReadTime << '\n' << "Time to create models: " << dModelCreateTime;
	VS_LOG(output.str().c_str());

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::ReleaseModel()
{
	if (m_pMatLib)
	{
		delete m_pMatLib;
		m_pMatLib = nullptr;
	}

	for (int i = 0; i < m_arrSubMeshes.size(); i++)
	{
		delete m_arrSubMeshes[i];
		m_arrSubMeshes[i] = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Mesh::InitialiseBuffers(int subMeshIndex, ID3D11Device* pDevice)
{
	VertexType* vertices;
	unsigned long* indices;

	D3D11_BUFFER_DESC vertexBufferDesc, indexBufferDesc;
	D3D11_SUBRESOURCE_DATA vertexData, indexData;
	HRESULT result;

	SubMesh* pSubMesh = m_arrSubMeshes[subMeshIndex];
	if (!pSubMesh)
	{
		return false;
	}
	//Create vert array
	vertices = new VertexType[pSubMesh->m_arrModel.size()];
	if (!vertices)
	{
		return false;
	}

	//Create index array
	indices = new unsigned long[pSubMesh->m_arrModel.size()];
	if (!indices)
	{
		return false;
	}

	pSubMesh->m_iVertexCount = pSubMesh->m_iIndexCount = pSubMesh->m_arrModel.size();

	for (int i = 0; i < pSubMesh->m_arrModel.size(); i++)
	{
		// Load the vertex array with data.
		vertices[i].position = pSubMesh->m_arrModel[i].pos;  // Bottom left.
		vertices[i].normal = pSubMesh->m_arrModel[i].norm;
		vertices[i].texture = pSubMesh->m_arrModel[i].tex;
		vertices[i].tangent = pSubMesh->m_arrModel[i].tangent;
		vertices[i].binormal = pSubMesh->m_arrModel[i].binormal;

		indices[i] = i;
	}

	

	//Setup the description of the static vertex buffer.
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(VertexType) * pSubMesh->m_iVertexCount;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	//Give the subresource struct a pointer to the vert data
	vertexData.pSysMem = vertices;
	vertexData.SysMemPitch = 0;
	vertexData.SysMemSlicePitch = 0;

	//Create the Vertex buffer..
	result = pDevice->CreateBuffer(&vertexBufferDesc, &vertexData, &pSubMesh->m_pVertexBuffer);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create vertex buffer");
		return false;
	}

	//Setup the description of the static index buffer
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(unsigned long) * pSubMesh->m_iIndexCount;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;

	//Give the subresource a pointer to the index data.
	indexData.pSysMem = indices;
	indexData.SysMemPitch = 0;
	indexData.SysMemSlicePitch = 0;

	//Create the index buffer
	result = pDevice->CreateBuffer(&indexBufferDesc, &indexData, &pSubMesh->m_pIndexBuffer);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create index buffer");
		return false;
	}

	m_arrSubMeshes[subMeshIndex]->m_arrModel.clear();

	//Release the arrays now that the vertex and index buffers have been created and loaded..
	delete[] vertices;
	vertices = nullptr;

	delete[] indices;
	indices = nullptr;

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::ShutdownBuffers()
{
	for (int i = 0; i < m_arrSubMeshes.size(); i++)
	{
		if (m_arrSubMeshes[i]->m_pIndexBuffer)
		{
			m_arrSubMeshes[i]->m_pIndexBuffer->Release();
			m_arrSubMeshes[i]->m_pIndexBuffer = nullptr;
		}
		if (m_arrSubMeshes[i]->m_pVertexBuffer)
		{
			m_arrSubMeshes[i]->m_pVertexBuffer->Release();
			m_arrSubMeshes[i]->m_pVertexBuffer = nullptr;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::RenderBuffers(int subMeshIndex, ID3D11DeviceContext* pDeviceContext)
{
	unsigned int stride;
	unsigned int offset;

	//Set vertex buffer stride and offset.
	stride = sizeof(VertexType);
	offset = 0;

	// Set the vertex buffer to active in the input assembler so it can be rendered.
	pDeviceContext->IASetVertexBuffers(0, 1, &m_arrSubMeshes[subMeshIndex]->m_pVertexBuffer, &stride, &offset);

	// Set the index buffer to active in the input assembler so it can be rendered.
	pDeviceContext->IASetIndexBuffer(m_arrSubMeshes[subMeshIndex]->m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// Set the type of primitive that should be rendered from this vertex buffer, in this case triangles.
	pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::CalculateModelVectors()
{
	for (int i = 0; i < m_arrSubMeshes.size(); i++)
	{
		if (m_arrSubMeshes[i]->m_pMaterial && m_arrSubMeshes[i]->m_pMaterial->UsesNormalMaps())
		{
			for (int j = 0; j < m_arrSubMeshes[i]->m_arrModel.size() / 3; j++)
			{
				XMFLOAT3 binormal, tangent;
				XMVECTOR vBinormal, vTangent, vNormal;

				XMFLOAT3 vVec1, vVec2;
				XMFLOAT2 vTUVec, vTVVec;

				ModelType *pVert1, *pVert2, *pVert3;

				pVert1 = &m_arrSubMeshes[i]->m_arrModel[(j * 3)];
				pVert2 = &m_arrSubMeshes[i]->m_arrModel[(j * 3) + 1];
				pVert3 = &m_arrSubMeshes[i]->m_arrModel[(j * 3) + 2];

				//Get two vectors for this face
				vVec1.x = pVert2->pos.x - pVert1->pos.x;
				vVec1.y = pVert2->pos.y - pVert1->pos.y;
				vVec1.z = pVert2->pos.z - pVert1->pos.z;
				vVec2.x = pVert3->pos.x - pVert1->pos.x;
				vVec2.y = pVert3->pos.y - pVert1->pos.y;
				vVec2.z = pVert3->pos.z - pVert1->pos.z;

				vTUVec.x = pVert2->tex.x - pVert1->tex.x;
				vTUVec.y = pVert3->tex.x - pVert1->tex.x;

				vTVVec.x = pVert2->tex.y - pVert1->tex.y;
				vTVVec.y = pVert3->tex.y - pVert1->tex.y;

				// Calculate the denominator of the tangent/binormal equation.
				float den = 1.f / ((vTUVec.x * vTVVec.y) - (vTUVec.y * vTVVec.x));

				//Calculate the tangent and binormal
				tangent.x = (vTVVec.y * vVec1.x - vTVVec.x * vVec2.x) * den;
				tangent.y = (vTVVec.y * vVec1.y - vTVVec.x * vVec2.y) * den;
				tangent.z = (vTVVec.y * vVec1.z - vTVVec.x * vVec2.z) * den;

				binormal.x = (vTUVec.x * vVec2.x - vTUVec.y * vVec1.x) * den;
				binormal.y = (vTUVec.x * vVec2.y - vTUVec.y * vVec1.y) * den;
				binormal.z = (vTUVec.x * vVec2.z - vTUVec.y * vVec1.z) * den;

				vTangent = XMLoadFloat3(&tangent);
				vBinormal = XMLoadFloat3(&binormal);

				vTangent = XMVector3Normalize(vTangent);
				vBinormal = XMVector3Normalize(vBinormal);

				//Store them back in the model types
				XMStoreFloat3(&pVert1->binormal, vBinormal);
				XMStoreFloat3(&pVert1->tangent, vTangent);
		
				XMStoreFloat3(&pVert2->binormal, vBinormal);
				XMStoreFloat3(&pVert2->tangent, vTangent);
		
				XMStoreFloat3(&pVert3->binormal, vBinormal);
				XMStoreFloat3(&pVert3->tangent, vTangent);
			
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SubMesh::CalculateBoundingBox()
{
	XMFLOAT3 min = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
	XMFLOAT3 max = XMFLOAT3(0.f, 0.f, 0.f);
	for (int i = 0; i < m_arrModel.size(); i++)
	{
		XMFLOAT3 pos = m_arrModel[i].pos;
		if (pos.x > max.x)
		{
			max.x = pos.x;
		}
		if (pos.x < min.x)
		{
			min.x = pos.x;
		}
		if (pos.y > max.y)
		{
			max.y = pos.y;
		}
		if (pos.y < min.y)
		{
			min.y = pos.y;
		}
		if (pos.z > max.z)
		{
			max.z = pos.z;
		}
		if (pos.z < min.z)
		{
			min.z = pos.z;
		}
	}

	m_BoundingBox.Max = max;
	m_BoundingBox.Min = min;
}

void SubMesh::CalculateDistanceToCamera(Camera* pCamera)
{
	XMVECTOR vMin, vMax, vMid, vHalfSize, vCam, vToCam;
	vMax = XMLoadFloat3(&m_BoundingBox.Max);
	vMin = XMLoadFloat3(&m_BoundingBox.Min);
	vHalfSize = (vMax - vMin) * 0.5f;
	vMid = vHalfSize + vMin;

	vCam = XMLoadFloat3(&pCamera->GetPosition());

	vToCam = vCam - vMid;

	m_fDistanceToCamera = abs(XMVector3Length(vToCam).m128_f32[0] - XMVector3Length(vHalfSize).m128_f32[0]);
}
