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

	//Calculate the binormals and tangent vectors
	
	CalculateModelVectors();
	
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

void Mesh::Render(ID3D11DeviceContext* pDeviceContext, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix, XMFLOAT3 vLightDirection, XMFLOAT4 vLightDiffuseColour, XMFLOAT4 vAmbientColour, XMFLOAT3 vCameraPos)
{
	//Put the vertex and index buffers in the graphics pipeline so they can be drawn
	for (int i = 0; i < m_iSubMeshCount; i++)
	{
		RenderBuffers(i, pDeviceContext);

		if (!m_arrSubMeshes[i]->m_pMaterial->Render(pDeviceContext, m_arrSubMeshes[i]->m_iIndexCount, mWorldMatrix, mViewMatrix, mProjectionMatrix, vLightDirection, vLightDiffuseColour, vAmbientColour, vCameraPos))
		{
			VS_LOG_VERBOSE("Unable to render object with shader");
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Mesh::GetIndexCount(int subMeshIndex)
{
	return m_arrSubMeshes[subMeshIndex]->m_iIndexCount;
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

	
	m_arrSubMeshes.push_back(new SubMesh());
	

	//read in the vert count
	fin >> m_arrSubMeshes[0]->m_iVertexCount;
	m_arrSubMeshes[0]->m_iIndexCount = m_arrSubMeshes[0]->m_iVertexCount;

	//create the model array..
	m_arrSubMeshes[0]->m_arrModel.reserve(m_arrSubMeshes[0]->m_iVertexCount);

	//Read up to the beginning of the data
	fin.get(input);
	while (input != ':')
	{
		fin.get(input);
	}
	fin.get(input);
	fin.get(input);

	//Read in the vertex data
	for (int i = 0; i < m_arrSubMeshes[0]->m_iVertexCount; i++)
	{
		if (i + 1 > m_arrSubMeshes[0]->m_arrModel.size())
		{
			m_arrSubMeshes[0]->m_arrModel.push_back(ModelType());
		}
		fin >> m_arrSubMeshes[0]->m_arrModel[i].pos.x >> m_arrSubMeshes[0]->m_arrModel[i].pos.y >> m_arrSubMeshes[0]->m_arrModel[i].pos.z;
		fin >> m_arrSubMeshes[0]->m_arrModel[i].tex.x >> m_arrSubMeshes[0]->m_arrModel[i].tex.y;
		fin >> m_arrSubMeshes[0]->m_arrModel[i].norm.x >> m_arrSubMeshes[0]->m_arrModel[i].norm.y >> m_arrSubMeshes[0]->m_arrModel[i].norm.z;
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
				
				m_arrSubMeshes[iObjectIndex]->m_pMaterial = m_MatLib->GetMaterial(s);
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
				m_MatLib->LoadMaterialLibrary(pDevice, hwnd, sMaterialLibName.c_str());
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


	m_iTotalVerticesCount = Verts.size();
	m_iTotalNormalCount = Normals.size();
	m_iTotalTextureCoordCount = TexCoords.size();
	m_iSubMeshCount = m_arrSubMeshes.size();
	double dEndTime = Timer::Get()->GetCurrentTime();
	stringstream output;
	output << "Time to process data: " << (dEndTime - dStartTime) << '\n' << "Time To read in faces: " << dFaceReadTime << '\n' << "Time to create models: " << dModelCreateTime;
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
				m_arrSubMeshes[iSubMeshCount]->m_iFaceCount = iFaceIndex;
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
	m_arrSubMeshes[iSubMeshCount]->m_iFaceCount = iFaceIndex;
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
		vertices[i].color = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
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
	for (int i = 0; i < m_iSubMeshCount; i++)
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
		if (m_arrSubMeshes[i]->m_pMaterial->UsesNormalMaps())
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