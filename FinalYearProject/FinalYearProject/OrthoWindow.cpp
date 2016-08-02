#include "OrthoWindow.h"
#include "Debugging.h"

OrthoWindow::OrthoWindow()
	: m_vertexBuffer(nullptr)
	, m_indexBuffer(nullptr)
{

}

OrthoWindow::~OrthoWindow()
{
	Shutdown();
}

bool OrthoWindow::Initialize(ID3D11Device* pDevice, int windowWidth, int windowHeight)
{
	return InitialiseBuffers(pDevice, windowWidth, windowHeight);
}

void OrthoWindow::Shutdown()
{
	ShutdownBuffers();
}

void OrthoWindow::Render(ID3D11DeviceContext* pContext)
{
	RenderBuffers(pContext);
}

int OrthoWindow::GetIndexCount()
{
	return m_indexCount;
}

bool OrthoWindow::InitialiseBuffers(ID3D11Device* pDevice, int windowWidth, int windowHeight)
{
	float left, right, top, bottom;
	// Calculate the screen coordinates for the edges of the window
	left = (float)((windowWidth / 2) * -1);
	right = left + (float)windowWidth;
	top = (float)(windowHeight / 2);
	bottom = top - (float)windowHeight;

	// Set the number of vertices in the vertex array.
	m_vertexCount = 6;

	// Set the number of indices in the index array.
	m_indexCount = m_vertexCount;

	// Create the vertex array.
	VertexType* vertices = new VertexType[m_vertexCount];
	if (!vertices)
	{
		return false;
	}

	// Create the index array.
	unsigned long* indices = new unsigned long[m_indexCount];
	if (!indices)
	{
		return false;
	}

	D3D11_BUFFER_DESC vertexBufferDesc, indexBufferDesc;
	D3D11_SUBRESOURCE_DATA vertexData, indexData;
	HRESULT result;

	vertices[0].vPosition = XMFLOAT3(left, top, 0.0f);  // Top left.
	vertices[0].vTexture = XMFLOAT2(0.0f, 0.0f);

	vertices[1].vPosition = XMFLOAT3(right, bottom, 0.0f);  // Bottom right.
	vertices[1].vTexture = XMFLOAT2(1.0f, 1.0f);

	vertices[2].vPosition = XMFLOAT3(left, bottom, 0.0f);  // Bottom left.
	vertices[2].vTexture = XMFLOAT2(0.0f, 1.0f);

	// Second triangle.
	vertices[3].vPosition = XMFLOAT3(left, top, 0.0f);  // Top left.
	vertices[3].vTexture = XMFLOAT2(0.0f, 0.0f);

	vertices[4].vPosition = XMFLOAT3(right, top, 0.0f);  // Top right.
	vertices[4].vTexture = XMFLOAT2(1.0f, 0.0f);

	vertices[5].vPosition = XMFLOAT3(right, bottom, 0.0f);  // Bottom right.
	vertices[5].vTexture = XMFLOAT2(1.0f, 1.0f);

	// Load the index array with data.
	for (int i = 0; i < m_indexCount; i++)
	{
		indices[i] = i;
	}

	// Set up the description of the vertex buffer.
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(VertexType) * m_vertexCount;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the vertex data.
	vertexData.pSysMem = vertices;
	vertexData.SysMemPitch = 0;
	vertexData.SysMemSlicePitch = 0;

	// Now finally create the vertex buffer.
	result = pDevice->CreateBuffer(&vertexBufferDesc, &vertexData, &m_vertexBuffer);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create buffer")
		return false;
	}

	// Set up the description of the index buffer.
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(unsigned long) * m_indexCount;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the index data.
	indexData.pSysMem = indices;
	indexData.SysMemPitch = 0;
	indexData.SysMemSlicePitch = 0;

	// Create the index buffer.
	result = pDevice->CreateBuffer(&indexBufferDesc, &indexData, &m_indexBuffer);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create buffer")
		return false;
	}

	delete[] vertices;
	vertices = nullptr;

	delete[] indices;
	indices = nullptr;

	return true;
}

void OrthoWindow::ShutdownBuffers()
{
	// Release the index buffer.
	if (m_indexBuffer)
	{
		m_indexBuffer->Release();
		m_indexBuffer = nullptr;
	}

	// Release the vertex buffer.
	if (m_vertexBuffer)
	{
		m_vertexBuffer->Release();
		m_vertexBuffer = nullptr;
	}

}

void OrthoWindow::RenderBuffers(ID3D11DeviceContext* pContext)
{
	unsigned int stride;
	unsigned int offset;

	stride = sizeof(VertexType);
	offset = 0;

	pContext->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
	pContext->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

}

