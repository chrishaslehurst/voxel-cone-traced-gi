#include "ColourShader.h"
#include "Debugging.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ColourShader::ColourShader()
	: m_pVertexShader(nullptr)
	, m_pPixelShader(nullptr)
	, m_pLayout(nullptr)
	, m_pMatrixBuffer(nullptr)
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ColourShader::~ColourShader()
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ColourShader::Initialise(ID3D11Device* pDevice, HWND hwnd)
{
	return InitialiseShader(pDevice, hwnd, L"Colour.hlsl");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ColourShader::Shutdown()
{
	ShutdownShader();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ColourShader::Render(ID3D11DeviceContext* pDeviceContext, int iIndexCount, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix)
{
	bool result;
	result = SetShaderParameters(pDeviceContext, mWorldMatrix, mViewMatrix, mProjectionMatrix);
	if (!result)
	{
		VS_LOG_VERBOSE("Failed to set shader parameters");
		return false;
	}

	//render the prepared buffers with the shader..
	RenderShader(pDeviceContext, iIndexCount);

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ColourShader::InitialiseShader(ID3D11Device* pDevice, HWND hwnd, WCHAR* sShaderFilename)
{
	HRESULT result;
	ID3D10Blob* pErrorMessage( nullptr);
	ID3D10Blob* pVertexShaderBuffer( nullptr);
	ID3D10Blob* pPixelShaderBuffer( nullptr);
	
	D3D11_BUFFER_DESC matrixBufferDesc;

	//Compile the vertex shader code
	result = D3DCompileFromFile(sShaderFilename, nullptr, nullptr, "VSMain", "vs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, &pVertexShaderBuffer, &pErrorMessage);
	if (FAILED(result))
	{
		if (pErrorMessage)
		{
			//If the shader failed to compile it should have written something to error message, so we output that here
			OutputShaderErrorMessage(pErrorMessage, hwnd, sShaderFilename);
		}
		else
		{
			//if it hasn't, then it couldn't find the shader file..
			MessageBox(hwnd, sShaderFilename, L"Missing Shader File", MB_OK);
		}
		return false;
	}

	//compile the pixel shader..
	result = D3DCompileFromFile(sShaderFilename, nullptr, nullptr, "PSMain", "ps_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, &pPixelShaderBuffer, &pErrorMessage);
	if (FAILED(result))
	{
		if (pErrorMessage)
		{
			//If the shader failed to compile it should have written something to error message, so we output that here
			OutputShaderErrorMessage(pErrorMessage, hwnd, sShaderFilename);
		}
		else
		{
			//if it hasn't, then it couldn't find the shader file..
			MessageBox(hwnd, sShaderFilename, L"Missing Shader File", MB_OK);
		}
		return false;
	}

	result = pDevice->CreateVertexShader(pVertexShaderBuffer->GetBufferPointer(), pVertexShaderBuffer->GetBufferSize(), nullptr, &m_pVertexShader);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create the vertex shader.");
		return false;
	}

	result = pDevice->CreatePixelShader(pPixelShaderBuffer->GetBufferPointer(), pPixelShaderBuffer->GetBufferSize(), nullptr, &m_pPixelShader);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create the pixel shader.");
		return false;
	}

	D3D11_INPUT_ELEMENT_DESC polyLayout[2];
	//Setup data layout for the shader, needs to match the VertexType struct in the Mesh class and in the shader code.
	polyLayout[0].SemanticName = "POSITION";
	polyLayout[0].SemanticIndex = 0;
	polyLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polyLayout[0].InputSlot = 0;
	polyLayout[0].AlignedByteOffset = 0;
	polyLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polyLayout[0].InstanceDataStepRate = 0;

	polyLayout[1].SemanticName = "COLOR";
	polyLayout[1].SemanticIndex = 0;
	polyLayout[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	polyLayout[1].InputSlot = 0;
	polyLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polyLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polyLayout[1].InstanceDataStepRate = 0;

	//Get the number of elements in the layout
	unsigned int iNumElements(sizeof(polyLayout) / sizeof(polyLayout[0]));

	//Create the vertex input layout..
	result = pDevice->CreateInputLayout(polyLayout, iNumElements, pVertexShaderBuffer->GetBufferPointer(), pVertexShaderBuffer->GetBufferSize(), &m_pLayout);

	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create input layout");
		return false;
	}

	//Finished with shader buffers now so they can be released
	pVertexShaderBuffer->Release();
	pVertexShaderBuffer = nullptr;

	pPixelShaderBuffer->Release();
	pPixelShaderBuffer = nullptr;

	//Setup the description of the dynamic matrix constant buffer that is in the shader..
	matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	matrixBufferDesc.ByteWidth = sizeof(MatrixBuffer);
	matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	matrixBufferDesc.MiscFlags = 0;
	matrixBufferDesc.StructureByteStride = 0;

	result = pDevice->CreateBuffer(&matrixBufferDesc, NULL, &m_pMatrixBuffer);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create matrix buffer");
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ColourShader::ShutdownShader()
{
	if (m_pMatrixBuffer)
	{
		m_pMatrixBuffer->Release();
		m_pMatrixBuffer = nullptr;
	}
	if (m_pLayout)
	{
		m_pLayout->Release();
		m_pLayout = nullptr;
	}
	if (m_pPixelShader)
	{
		m_pPixelShader->Release();
		m_pPixelShader = nullptr;
	}
	if (m_pVertexShader)
	{
		m_pVertexShader->Release();
		m_pVertexShader = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ColourShader::OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename)
{
	//Get ptr to error message text buffer
	char* compileErrors = (char*)(errorMessage->GetBufferPointer());

	//get the length of the message..
	unsigned long u_iBufferSize = errorMessage->GetBufferSize();
	
	ofstream fout;
	fout.open("shader-error.txt");
	//Write out the error to the file..

	for (unsigned long i = 0; i < u_iBufferSize; i++)
	{
		fout << compileErrors[i];
	}
	//Close file
	fout.close();

	errorMessage->Release();
	errorMessage = nullptr;

	// Pop a message up on the screen to notify the user to check the text file for compile errors.
	MessageBox(hwnd, L"Error compiling shader. Check shader-error.txt for message.", shaderFilename, MB_OK);

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ColourShader::SetShaderParameters(ID3D11DeviceContext* pDeviceContext, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix)
{
	
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	MatrixBuffer* pData;
	unsigned int u_iBufferNumber;

	//Matrices need to be transposed before sending them into the shader for dx11..
	mWorldMatrix = XMMatrixTranspose(mWorldMatrix);
	mViewMatrix = XMMatrixTranspose(mViewMatrix);
	mProjectionMatrix = XMMatrixTranspose(mProjectionMatrix);

	//Lock the matrixBuffer, set new matrices inside it, then unlock it.
	HRESULT result = pDeviceContext->Map(m_pMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to lock the matrix buffer");
		return false;
	}

	//Get pointer to the buffer data
	pData = (MatrixBuffer*)mappedResource.pData;

	pData->world = mWorldMatrix;
	pData->view = mViewMatrix;
	pData->projection = mProjectionMatrix;

	//Unlock the buffer
	pDeviceContext->Unmap(m_pMatrixBuffer, 0);

	//Set the position of the buffer in the HLSL shader
	u_iBufferNumber = 0;

	//Finally, set the buffer in the shader to the new values
	pDeviceContext->VSSetConstantBuffers(u_iBufferNumber, 1, &m_pMatrixBuffer);

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ColourShader::RenderShader(ID3D11DeviceContext* pDeviceContext, int iIndexCount)
{
	//Set the vertex input layout
	pDeviceContext->IASetInputLayout(m_pLayout);

	//Set the shaders that are used to render the triangle..
	pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

	//Render the triangle
	pDeviceContext->DrawIndexed(iIndexCount, 0, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
