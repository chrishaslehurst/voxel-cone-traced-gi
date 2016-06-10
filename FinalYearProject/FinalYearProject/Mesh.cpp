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
	if (!LoadModel(pDevice, hwnd, filename))
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

bool Mesh::LoadModel(ID3D11Device* pDevice, HWND hwnd, char* filename)
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