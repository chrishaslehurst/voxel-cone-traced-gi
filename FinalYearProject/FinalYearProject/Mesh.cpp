#include "Mesh.h"
#include "Debugging.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Mesh::Mesh()
	: m_pVertexBuffer(nullptr)
	, m_pIndexBuffer(nullptr)
	, m_pMaterial(nullptr)
	, m_pModel(nullptr)
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Mesh::~Mesh()
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Mesh::Initialise(ID3D11Device* pDevice, char* filename, Material* pMaterial /*=nullptr*/)
{
	//Load in the model data
	if (!LoadModel(filename))
	{
		return false;
	}

	SetMaterial(pMaterial);
	//Initialise the buffers
	return InitialiseBuffers(pDevice);
	
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
	RenderBuffers(pDeviceContext);

	if (!m_pMaterial->Render(pDeviceContext, m_iIndexCount, mWorldMatrix, mViewMatrix, mProjectionMatrix, vLightDirection, vLightDiffuseColour))
	{
		VS_LOG_VERBOSE("Unable to render object with shader");
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Mesh::GetIndexCount()
{
	return m_iIndexCount;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Mesh::LoadModel(char* filename)
{
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

	//read in the vert count
	fin >> m_iVertexCount;
	m_iIndexCount = m_iVertexCount;

	//create the model array..
	m_pModel = new ModelType[m_iVertexCount];
	if (!m_pModel)
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
	for (int i = 0; i < m_iVertexCount; i++)
	{
		fin >> m_pModel[i].x >> m_pModel[i].y >> m_pModel[i].z;
		fin >> m_pModel[i].tu >> m_pModel[i].tv;
		fin >> m_pModel[i].nx >> m_pModel[i].ny >> m_pModel[i].nz;
	}

	fin.close();

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::ReleaseModel()
{
	if (m_pModel)
	{
		delete[] m_pModel;
		m_pModel = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Mesh::InitialiseBuffers(ID3D11Device* pDevice)
{
	VertexType* vertices;
	unsigned long* indices;

	D3D11_BUFFER_DESC vertexBufferDesc, indexBufferDesc;
	D3D11_SUBRESOURCE_DATA vertexData, indexData;
	HRESULT result;

	//Create vert array
	vertices = new VertexType[m_iVertexCount];
	if (!vertices)
	{
		return false;
	}

	//Create index array
	indices = new unsigned long[m_iIndexCount];
	if (!indices)
	{
		return false;
	}


	for (int i = 0; i < m_iVertexCount; i++)
	{
		// Load the vertex array with data.
		vertices[i].position = XMFLOAT3(m_pModel[i].x, m_pModel[i].y, m_pModel[i].z);  // Bottom left.
		vertices[i].normal = XMFLOAT3(m_pModel[i].nx, m_pModel[i].ny, m_pModel[i].nz);
		vertices[i].texture = XMFLOAT2(m_pModel[i].tu, m_pModel[i].tv);
		vertices[i].color = XMFLOAT4(1.f, 1.f, 1.f, 1.f);

		indices[i] = i;
	}

	

	//Setup the description of the static vertex buffer.
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(VertexType) * m_iVertexCount;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	//Give the subresource struct a pointer to the vert data
	vertexData.pSysMem = vertices;
	vertexData.SysMemPitch = 0;
	vertexData.SysMemSlicePitch = 0;

	//Create the Vertex buffer..
	result = pDevice->CreateBuffer(&vertexBufferDesc, &vertexData, &m_pVertexBuffer);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create vertex buffer");
		return false;
	}

	//Setup the description of the static index buffer
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(unsigned long) * m_iIndexCount;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;

	//Give the subresource a pointer to the index data.
	indexData.pSysMem = indices;
	indexData.SysMemPitch = 0;
	indexData.SysMemSlicePitch = 0;

	//Create the index buffer
	result = pDevice->CreateBuffer(&indexBufferDesc, &indexData, &m_pIndexBuffer);
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
	if (m_pIndexBuffer)
	{
		m_pIndexBuffer->Release();
		m_pIndexBuffer = nullptr;
	}
	if (m_pVertexBuffer)
	{
		m_pVertexBuffer->Release();
		m_pVertexBuffer = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Mesh::RenderBuffers(ID3D11DeviceContext* pDeviceContext)
{
	unsigned int stride;
	unsigned int offset;

	//Set vertex buffer stride and offset.
	stride = sizeof(VertexType);
	offset = 0;

	// Set the vertex buffer to active in the input assembler so it can be rendered.
	pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);

	// Set the index buffer to active in the input assembler so it can be rendered.
	pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// Set the type of primitive that should be rendered from this vertex buffer, in this case triangles.
	pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////