#include "Mesh.h"
#include "Debugging.h"
#include "Timer.h"
#include <sstream>
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Mesh::Mesh()
	: m_MatLib(nullptr)
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Mesh::~Mesh()
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Mesh::Initialise(ID3D11Device* pDevice, HWND hwnd, char* filename)
{
	//Load in the model data
	if (!LoadModelFromObjFile(pDevice, hwnd, filename))
	{
		return false;
	}

	//SetMaterial(0, m_MatLib->GetMaterial("roof"));
	//Initialise the buffers
	bool result(true);
	for (int i = 0; i < m_iSubMeshCount; i++)
	{
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

void Mesh::Render(ID3D11DeviceContext* pDeviceContext, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix, XMFLOAT3 vLightDirection, XMFLOAT4 vLightDiffuseColour)
{
	//Put the vertex and index buffers in the graphics pipeline so they can be drawn
	for (int i = 0; i < m_iSubMeshCount; i++)
	{
		RenderBuffers(i, pDeviceContext);

		if (!m_arrSubMeshes[i].m_pMaterial->Render(pDeviceContext, m_arrSubMeshes[i].m_iIndexCount, mWorldMatrix, mViewMatrix, mProjectionMatrix, vLightDirection, vLightDiffuseColour))
		{
			VS_LOG_VERBOSE("Unable to render object with shader");
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Mesh::GetIndexCount(int subMeshIndex)
{
	return m_arrSubMeshes[subMeshIndex].m_iIndexCount;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Mesh::LoadModelFromTextFile(ID3D11Device* pDevice, HWND hwnd, char* filename)
{
	m_MatLib = new MaterialLibrary;
	if (!m_MatLib)
	{
		VS_LOG_VERBOSE("Unable to create new material library");
		return false;
	}
	m_MatLib->LoadMaterialLibrary(pDevice, hwnd, "../Assets/Shaders/sponza.mtl");
	
	ifstream fin;
	fin.open(filename);
	
	//did the file open?
	if (fin.fail())
	{
		VS_LOG_VERBOSE("Failed to load model, could not open file..");
		return false;
	}
	
	char input;
	//read up to the value of vertex count.
	fin.get(input);
	while (input != ':')
	{
		fin.get(input);
	}

	m_iSubMeshCount = 1;
	for (int i = 0; i < m_iSubMeshCount; i++)
	{
		m_arrSubMeshes.push_back(SubMesh());
	}

	//read in the vert count
	fin >> m_arrSubMeshes[0].m_iVertexCount;
	m_arrSubMeshes[0].m_iIndexCount = m_arrSubMeshes[0].m_iVertexCount;

	//create the model array..
	m_arrSubMeshes[0].m_arrModel.reserve(m_arrSubMeshes[0].m_iVertexCount);

	//Read up to the beginning of the data
	fin.get(input);
	while (input != ':')
	{
		fin.get(input);
	}
	fin.get(input);
	fin.get(input);

	//Read in the vertex data
	for (int i = 0; i < m_arrSubMeshes[0].m_iVertexCount; i++)
	{
		if (i + 1 > m_arrSubMeshes[0].m_arrModel.size())
		{
			m_arrSubMeshes[0].m_arrModel.push_back(ModelType());
		}
		fin >> m_arrSubMeshes[0].m_arrModel[i].pos.x >> m_arrSubMeshes[0].m_arrModel[i].pos.y >> m_arrSubMeshes[0].m_arrModel[i].pos.z;
		fin >> m_arrSubMeshes[0].m_arrModel[i].tex.x >> m_arrSubMeshes[0].m_arrModel[i].tex.y;
		fin >> m_arrSubMeshes[0].m_arrModel[i].norm.x >> m_arrSubMeshes[0].m_arrModel[i].norm.y >> m_arrSubMeshes[0].m_arrModel[i].norm.z;
	}

	fin.close();

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Mesh::LoadModelFromObjFile(ID3D11Device* pDevice, HWND hwnd, char* filename)
{
	m_MatLib = new MaterialLibrary;
	if (!m_MatLib)
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

	int iVertexIndex = 0;
	int iNormalIndex = 0;
	int iTexCoordIndex = 0;

	string s;
	
	int iObjectIndex( 0);
	std::vector<Face> Faces;
	int iFaceIndex = 0;
	while (fin)
	{
		char dump;
		if (s == "v")
		{
			//vertex
			Verts.push_back(XMFLOAT3());
			fin >> Verts[iVertexIndex].x >> Verts[iVertexIndex].y >> Verts[iVertexIndex].z;
			// Invert the Z vertex to change to left hand system.
			Verts[iVertexIndex].z = Verts[iVertexIndex].z * -1.0f;
			iVertexIndex++;
			fin >> s;
		}
		else if (s == "vt")
		{
			TexCoords.push_back(XMFLOAT2());
			//texture coord
			fin >> TexCoords[iTexCoordIndex].x >> TexCoords[iTexCoordIndex].y;
			//Invert the v texture coord to left hand system
			TexCoords[iTexCoordIndex].y = 1.0f - TexCoords[iTexCoordIndex].y;
			iTexCoordIndex++;
			fin >> s;
		}
		else if (s == "vn")
		{
			Normals.push_back(XMFLOAT3());
			//normal coord
			fin >> Normals[iNormalIndex].x >> Normals[iNormalIndex].y >> Normals[iNormalIndex].z;
			//Invert the z normal to change to left hand system
			Normals[iNormalIndex].z = Normals[iNormalIndex].z * -1.f;
			iNormalIndex++;
			fin >> s;
		}
		else if (s == "f")
		{
			//Reade the face data in backwards to convert to left hand system
			if (iFaceIndex + 1 > Faces.size())
			{
				Faces.push_back(Face());
			}
			int v4, t4, n4;
			fin >> Faces[iFaceIndex].vIndex3 >> dump >> Faces[iFaceIndex].tIndex3 >> dump >> Faces[iFaceIndex].nIndex3
				>> Faces[iFaceIndex].vIndex2 >> dump >> Faces[iFaceIndex].tIndex2 >> dump >> Faces[iFaceIndex].nIndex2
				>> Faces[iFaceIndex].vIndex1 >> dump >> Faces[iFaceIndex].tIndex1 >> dump >> Faces[iFaceIndex].nIndex1;

			iFaceIndex++;
			fin >> s;
		}
		else if (s == "usemtl")
		{
			//get the mat name
			fin >> s;
			
			if (iFaceIndex > 0)
			{
				int vIndex, tIndex, nIndex;
				m_arrSubMeshes[iObjectIndex].m_arrModel.reserve(Faces.size() * 3);
				for (int i = 0; i < iFaceIndex; i++)
				{
					int modelIndex = i * 3;
					vIndex = Faces[i].vIndex1 - 1;
					tIndex = Faces[i].tIndex1 - 1;
					nIndex = Faces[i].nIndex1 - 1;

					m_arrSubMeshes[iObjectIndex].m_arrModel.push_back(ModelType());
					m_arrSubMeshes[iObjectIndex].m_arrModel[modelIndex].pos = Verts[vIndex];
					m_arrSubMeshes[iObjectIndex].m_arrModel[modelIndex].tex = TexCoords[tIndex];
					m_arrSubMeshes[iObjectIndex].m_arrModel[modelIndex].norm = Normals[nIndex];

					modelIndex++;
					vIndex = Faces[i].vIndex2 - 1;
					tIndex = Faces[i].tIndex2 - 1;
					nIndex = Faces[i].nIndex2 - 1;

					m_arrSubMeshes[iObjectIndex].m_arrModel.push_back(ModelType());
					m_arrSubMeshes[iObjectIndex].m_arrModel[modelIndex].pos = Verts[vIndex];
					m_arrSubMeshes[iObjectIndex].m_arrModel[modelIndex].tex = TexCoords[tIndex];
					m_arrSubMeshes[iObjectIndex].m_arrModel[modelIndex].norm = Normals[nIndex];

					modelIndex++;
					vIndex = Faces[i].vIndex3 - 1;
					tIndex = Faces[i].tIndex3 - 1;
					nIndex = Faces[i].nIndex3 - 1;

					m_arrSubMeshes[iObjectIndex].m_arrModel.push_back(ModelType());
					m_arrSubMeshes[iObjectIndex].m_arrModel[modelIndex].pos = Verts[vIndex];
					m_arrSubMeshes[iObjectIndex].m_arrModel[modelIndex].tex = TexCoords[tIndex];
					m_arrSubMeshes[iObjectIndex].m_arrModel[modelIndex].norm = Normals[nIndex];
				}

				iObjectIndex++;
			}
			iFaceIndex = 0;
			if (Faces.size() > 0)
			{
				Faces.empty();
			}
			if (iObjectIndex + 1 > m_arrSubMeshes.size())
			{
				m_arrSubMeshes.push_back(SubMesh());
			}
			m_arrSubMeshes[iObjectIndex].m_pMaterial = m_MatLib->GetMaterial(s);
			fin >> s;
		}
		else if (s == "mtllib")
		{
			string sMaterialLibName;
			fin >> sMaterialLibName;

			sMaterialLibName = "../Assets/Shaders/" + sMaterialLibName;
 			m_MatLib->LoadMaterialLibrary(pDevice, hwnd, sMaterialLibName.c_str());
			fin >> s;
		}
		else
		{
			fin >> s;
		}
	}
	//append the last face
	if (iFaceIndex > 0)
	{
		int vIndex, tIndex, nIndex;
		for (int i = 0; i < iFaceIndex; i++)
		{
			int modelIndex = i * 3;
			vIndex = Faces[i].vIndex1 - 1;
			tIndex = Faces[i].tIndex1 - 1;
			nIndex = Faces[i].nIndex1 - 1;

			m_arrSubMeshes[iObjectIndex].m_arrModel.push_back(ModelType());
			m_arrSubMeshes[iObjectIndex].m_arrModel[modelIndex].pos = Verts[vIndex];
			m_arrSubMeshes[iObjectIndex].m_arrModel[modelIndex].tex = TexCoords[tIndex];
			m_arrSubMeshes[iObjectIndex].m_arrModel[modelIndex].norm = Normals[nIndex];

			modelIndex++;
			vIndex = Faces[i].vIndex2 - 1;
			tIndex = Faces[i].tIndex2 - 1;
			nIndex = Faces[i].nIndex2 - 1;

			m_arrSubMeshes[iObjectIndex].m_arrModel.push_back(ModelType());
			m_arrSubMeshes[iObjectIndex].m_arrModel[modelIndex].pos = Verts[vIndex];
			m_arrSubMeshes[iObjectIndex].m_arrModel[modelIndex].tex = TexCoords[tIndex];
			m_arrSubMeshes[iObjectIndex].m_arrModel[modelIndex].norm = Normals[nIndex];

			modelIndex++;
			vIndex = Faces[i].vIndex3 - 1;
			tIndex = Faces[i].tIndex3 - 1;
			nIndex = Faces[i].nIndex3 - 1;

			m_arrSubMeshes[iObjectIndex].m_arrModel.push_back(ModelType());
			m_arrSubMeshes[iObjectIndex].m_arrModel[modelIndex].pos = Verts[vIndex];
			m_arrSubMeshes[iObjectIndex].m_arrModel[modelIndex].tex = TexCoords[tIndex];
			m_arrSubMeshes[iObjectIndex].m_arrModel[modelIndex].norm = Normals[nIndex];
		}

		iObjectIndex++;
	}

	m_iTotalVerticesCount = Verts.size();
	m_iTotalNormalCount = Normals.size();
	m_iTotalTextureCoordCount = TexCoords.size();
	m_iSubMeshCount = m_arrSubMeshes.size();
	double dEndTime = Timer::Get()->GetCurrentTime();
	stringstream output;
	output << "Time to process data: " << (dEndTime - dStartTime);
	VS_LOG(output.str().c_str());

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Mesh::FindNumSubMeshes(char* filename)
{
	m_iSubMeshCount = 0;
	ifstream fin;
	fin.open(filename);

	//did the file open?
	if (fin.fail())
	{
		VS_LOG_VERBOSE("Failed to load model, could not open file..");
		return false;
	}
	string s;
	while (fin)
	{
		if (s == "usemtl")
		{
			m_iSubMeshCount++;
		}
		fin >> s;

	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Mesh::ReadObjFileCounts(char* filename)
{
	fstream fin;
	fin.open(filename);

	//did the file open?
	if (fin.fail())
	{
		VS_LOG_VERBOSE("Failed to load model, could not open file..");
		return false;
	}
	int iFaceIndex = 0;
	int iSubMeshCount = 0;
	string s;
	while (fin)
	{
		
		if (s == "v")
		{
			//vertex
			m_iTotalVerticesCount++;
			fin >> s;
		}
		else if (s == "vt")
		{
			m_iTotalTextureCoordCount++;
			fin >> s;
		}
		else if (s == "vn")
		{
			m_iTotalNormalCount++;
			fin >> s;
		}
		else if (s == "usemtl")
		{
			//get the mat name
			fin >> s;

			if (iFaceIndex > 0)
			{
				m_arrSubMeshes[iSubMeshCount].m_iFaceCount = iFaceIndex;
				iSubMeshCount++;
			}
			iFaceIndex = 0;
			fin >> s;
		}
		else if (s == "f")
		{
			iFaceIndex++;
			fin >> s;
		}
		else
		{
			fin >> s;
		}
	}
	//append the last face
	m_arrSubMeshes[iSubMeshCount].m_iFaceCount = iFaceIndex;
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::ReleaseModel()
{
	if (m_MatLib)
	{
		delete m_MatLib;
		m_MatLib = nullptr;
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

	SubMesh* pSubMesh = &m_arrSubMeshes[subMeshIndex];
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
		vertices[i].color = XMFLOAT4(1.f, 1.f, 1.f, 1.f);

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
	for (int i = 0; i < m_iSubMeshCount; i++)
	{
		
		if (m_arrSubMeshes[i].m_pIndexBuffer)
		{
			m_arrSubMeshes[i].m_pIndexBuffer->Release();
			m_arrSubMeshes[i].m_pIndexBuffer = nullptr;
		}
		if (m_arrSubMeshes[i].m_pVertexBuffer)
		{
			m_arrSubMeshes[i].m_pVertexBuffer->Release();
			m_arrSubMeshes[i].m_pVertexBuffer = nullptr;
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
	pDeviceContext->IASetVertexBuffers(0, 1, &m_arrSubMeshes[subMeshIndex].m_pVertexBuffer, &stride, &offset);

	// Set the index buffer to active in the input assembler so it can be rendered.
	pDeviceContext->IASetIndexBuffer(m_arrSubMeshes[subMeshIndex].m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// Set the type of primitive that should be rendered from this vertex buffer, in this case triangles.
	pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////