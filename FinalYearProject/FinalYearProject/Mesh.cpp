#include "Mesh.h"
#include "Debugging.h"
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

	SetMaterial(0, m_MatLib->GetMaterial("roof"));
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
	m_arrSubMeshes = new SubMesh[m_iSubMeshCount];

	//read in the vert count
	fin >> m_arrSubMeshes[0].m_iVertexCount;
	m_arrSubMeshes[0].m_iIndexCount = m_arrSubMeshes[0].m_iVertexCount;

	//create the model array..
	m_arrSubMeshes[0].m_pModel = new ModelType[m_arrSubMeshes[0].m_iVertexCount];
	if (!m_arrSubMeshes[0].m_pModel)
	{
		VS_LOG_VERBOSE("Failed to load model, could not initialise model array");
		return false;
	}

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
		fin >> m_arrSubMeshes[0].m_pModel[i].x >> m_arrSubMeshes[0].m_pModel[i].y >> m_arrSubMeshes[0].m_pModel[i].z;
		fin >> m_arrSubMeshes[0].m_pModel[i].tu >> m_arrSubMeshes[0].m_pModel[i].tv;
		fin >> m_arrSubMeshes[0].m_pModel[i].nx >> m_arrSubMeshes[0].m_pModel[i].ny >> m_arrSubMeshes[0].m_pModel[i].nz;
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
	m_MatLib->LoadMaterialLibrary(pDevice, hwnd, "../Assets/Shaders/sponza.mtl");
	FindNumSubMeshes(filename);
	m_arrSubMeshes = new SubMesh[m_iSubMeshCount];
	ReadObjFileCounts(filename);
	
	ifstream fin;
	fin.open(filename);

	//did the file open?
	if (fin.fail())
	{
		VS_LOG_VERBOSE("Failed to load model, could not open file..");
		return false;
	}

	

	string s;
	string sMaterialLibName;
	int iObjectIndex(-1);
	while (fin)
	{		
		int iVertexIndex =0;
		int iNormalIndex =0;
		int iTexCoordIndex=0;
		int iFaceIndex = 0;
		char dump;
		if (s == "object")
		{
			iObjectIndex++;
			XMFLOAT3* Verts = new XMFLOAT3[m_arrSubMeshes[iObjectIndex].m_iVertexCount];
			XMFLOAT3* TexCoords = new XMFLOAT3[m_arrSubMeshes[iObjectIndex].m_iTextureCoordCount];
			XMFLOAT3* Normals = new XMFLOAT3[m_arrSubMeshes[iObjectIndex].m_iNormalCount];
			Face* Faces = new Face[m_arrSubMeshes[iObjectIndex].m_iFaceCount];
			//This is a new sub object.. start parsing..
			//object name
			fin >> s;
			//second #
			fin >> s;
			while (s != "object")
			{
				fin >> s;
				if (s == "v")
				{
					//vertex
					fin >> Verts[iVertexIndex].x >> Verts[iVertexIndex].y >> Verts[iVertexIndex].z;
					// Invert the Z vertex to change to left hand system.
					Verts[iVertexIndex].z = Verts[iVertexIndex].z * -1.0f;
					iVertexIndex++;
				}
				else if (s == "vt")
				{
					//texture coord
					fin >> TexCoords[iTexCoordIndex].x >> TexCoords[iTexCoordIndex].y;
					//Invert the v texture coord to left hand system
					TexCoords[iTexCoordIndex].y = 1.0f - TexCoords[iTexCoordIndex].y;
					iTexCoordIndex++;
				}
				else if (s == "vn")
				{
					//normal coord
					fin >> Normals[iNormalIndex].x >> Normals[iNormalIndex].y >> Normals[iNormalIndex].z;
					//Invert the z normal to change to left hand system
					Normals[iNormalIndex].z = Normals[iNormalIndex].z * -1.f;
					iNormalIndex++;
				}
				else if (s == "f")
				{
					//Reade the face data in backwards to convert to left hand system
					fin >> Faces[iFaceIndex].vIndex3 >> dump >> Faces[iFaceIndex].tIndex3 >> dump >> Faces[iFaceIndex].nIndex3
						>> Faces[iFaceIndex].vIndex2 >> dump >> Faces[iFaceIndex].tIndex2 >> dump >> Faces[iFaceIndex].nIndex2
						>> Faces[iFaceIndex].vIndex1 >> dump >> Faces[iFaceIndex].tIndex1 >> dump >> Faces[iFaceIndex].nIndex1;

						iFaceIndex++;
				}
				else if (s == "usemtl")
				{
					//get the mat name
					fin >> s;

					m_arrSubMeshes[iObjectIndex].m_pMaterial = m_MatLib->GetMaterial(s);
				}
			}

			m_arrSubMeshes[iObjectIndex].m_pModel = new ModelType[m_arrSubMeshes[iObjectIndex].m_iFaceCount * 3];
			int vIndex, tIndex, nIndex;
			for (int i = 0; i < iFaceIndex; i++)
			{
				int modelIndex = i * 3;
				vIndex = Faces[i].vIndex1 - 1;
				tIndex = Faces[i].tIndex1 - 1;
				nIndex = Faces[i].nIndex1 - 1;

				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].x = Verts[vIndex].x;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].y = Verts[vIndex].y;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].z = Verts[vIndex].z;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].tu = TexCoords[tIndex].x;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].tv = TexCoords[tIndex].y;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].nx = Normals[nIndex].x;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].ny = Normals[nIndex].y;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].nz = Normals[nIndex].z;

				modelIndex++;
				vIndex = Faces[i].vIndex2 - 1;
				tIndex = Faces[i].tIndex2 - 1;
				nIndex = Faces[i].nIndex2 - 1;

				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].x = Verts[vIndex].x;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].y = Verts[vIndex].y;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].z = Verts[vIndex].z;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].tu = TexCoords[tIndex].x;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].tv = TexCoords[tIndex].y;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].nx = Normals[nIndex].x;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].ny = Normals[nIndex].y;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].nz = Normals[nIndex].z;

				modelIndex++;
				vIndex = Faces[i].vIndex3 - 1;
				tIndex = Faces[i].tIndex3 - 1;
				nIndex = Faces[i].nIndex3 - 1;

				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].x = Verts[vIndex].x;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].y = Verts[vIndex].y;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].z = Verts[vIndex].z;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].tu = TexCoords[tIndex].x;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].tv = TexCoords[tIndex].y;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].nx = Normals[nIndex].x;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].ny = Normals[nIndex].y;
				m_arrSubMeshes[iObjectIndex].m_pModel[modelIndex].nz = Normals[nIndex].z;
			}

			delete[] Verts;
			Verts = nullptr;
			delete[] TexCoords;
			TexCoords = nullptr;
			delete[] Normals;
			Normals = nullptr;
			delete[] Faces;
			Faces = nullptr;

		}
		
		else if (s == "mtllib")
		{
			fin >> sMaterialLibName;
			fin >> s;
		}
		else
		{
			fin >> s;
		}
		
	}
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
	string sMaterialLibName;
	while (fin)
	{
		if (s == "object")
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
	int iObjectIndex( -1);
	string s;
	while (fin)
	{
		if (s == "object")
		{
			int iVertCount(0), iTexCount(0), iNormalCount(0), iFaceCount(0);
			iObjectIndex++;
			//This is a new sub object.. start parsing..
			//object name
			fin >> s;
			//second #
			fin >> s;
			while (fin && s != "object")
			{
				fin >> s;
				if (s == "v")
				{
					//vertex
					iVertCount++;
				}
				else if (s == "vt")
				{
					//texture coord
					iTexCount++;
				}
				else if (s == "vn")
				{
					//normal
					iNormalCount++;
				}
				else if (s == "f")
				{
					//face
					iFaceCount++;
				}
			}
			m_arrSubMeshes[iObjectIndex].m_iVertexCount = iVertCount;
			m_arrSubMeshes[iObjectIndex].m_iTextureCoordCount = iTexCount;
			m_arrSubMeshes[iObjectIndex].m_iNormalCount = iNormalCount;
			m_arrSubMeshes[iObjectIndex].m_iFaceCount = iFaceCount;

		}
		else
		{
			fin >> s;
		}

	}
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
	for (int i = 0; i < m_iSubMeshCount; i++)
	{
		if (m_arrSubMeshes[0].m_pModel)
		{
			delete[] m_arrSubMeshes[0].m_pModel;
			m_arrSubMeshes[0].m_pModel = nullptr;
		}
	}

	if (m_arrSubMeshes)
	{
		delete[] m_arrSubMeshes;
		m_arrSubMeshes = nullptr;
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
	vertices = new VertexType[pSubMesh->m_iVertexCount];
	if (!vertices)
	{
		return false;
	}

	//Create index array
	indices = new unsigned long[pSubMesh->m_iIndexCount];
	if (!indices)
	{
		return false;
	}


	for (int i = 0; i < pSubMesh->m_iVertexCount; i++)
	{
		// Load the vertex array with data.
		vertices[i].position = XMFLOAT3(pSubMesh->m_pModel[i].x, pSubMesh->m_pModel[i].y, pSubMesh->m_pModel[i].z);  // Bottom left.
		vertices[i].normal = XMFLOAT3(pSubMesh->m_pModel[i].nx, pSubMesh->m_pModel[i].ny, pSubMesh->m_pModel[i].nz);
		vertices[i].texture = XMFLOAT2(pSubMesh->m_pModel[i].tu, pSubMesh->m_pModel[i].tv);
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