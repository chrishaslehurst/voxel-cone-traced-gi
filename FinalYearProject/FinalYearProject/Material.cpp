#include "Material.h"
#include "Debugging.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Material::Material()
	: m_pVertexShader(nullptr)
	, m_pPixelShader(nullptr)
	, m_pLayout(nullptr)
	, m_pMatrixBuffer(nullptr)
	, m_pTexture(nullptr)
	, m_pSampleState(nullptr)
	, m_pLightBuffer(nullptr)
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Material::~Material()
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Material::Initialise(ID3D11Device* pDevice, HWND hwnd, WCHAR* textureFileName)
{
	if (!InitialiseShader(pDevice, hwnd, L"Material.hlsl"))
	{
		return false;
	}

	if (!LoadTexture(pDevice, textureFileName))
	{
		return false;
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Material::Shutdown()
{
	ReleaseTexture();

	ShutdownShader();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Material::Render(ID3D11DeviceContext* pDeviceContext, int iIndexCount, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix, XMFLOAT3 vLightDirection, XMFLOAT4 vDiffuseColour)
{
	bool result;
	result = SetShaderParameters(pDeviceContext, mWorldMatrix, mViewMatrix, mProjectionMatrix, vLightDirection, vDiffuseColour);
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

bool Material::LoadTexture(ID3D11Device* pDevice, WCHAR* filename)
{
	m_pTexture = new Texture;
	if(!m_pTexture)
	{
		VS_LOG_VERBOSE("Failed to create new texture");
		return false;
	}

	if (!m_pTexture->LoadTexture(pDevice, filename))
	{
		VS_LOG_VERBOSE("Failed to load texture from file");
		return false;
	}

	return true;
	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Material::ReleaseTexture()
{
	if (m_pTexture)
	{
		m_pTexture->Shutdown();
		delete m_pTexture;
		m_pTexture = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Material::InitialiseShader(ID3D11Device* pDevice, HWND hwnd, WCHAR* sShaderFilename)
{
	HRESULT result;
	ID3D10Blob* pErrorMessage( nullptr);
	ID3D10Blob* pVertexShaderBuffer( nullptr);
	ID3D10Blob* pPixelShaderBuffer( nullptr);

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

	D3D11_INPUT_ELEMENT_DESC polyLayout[4];
	//Setup data layout for the shader, needs to match the VertexType struct in the Mesh class and in the shader code.
	polyLayout[0].SemanticName = "POSITION";
	polyLayout[0].SemanticIndex = 0;
	polyLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polyLayout[0].InputSlot = 0;
	polyLayout[0].AlignedByteOffset = 0;
	polyLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polyLayout[0].InstanceDataStepRate = 0;

	polyLayout[1].SemanticName = "NORMAL";
	polyLayout[1].SemanticIndex = 0;
	polyLayout[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polyLayout[1].InputSlot = 0;
	polyLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polyLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polyLayout[1].InstanceDataStepRate = 0;


	polyLayout[2].SemanticName = "TEXCOORD";
	polyLayout[2].SemanticIndex = 0;
	polyLayout[2].Format = DXGI_FORMAT_R32G32_FLOAT;
	polyLayout[2].InputSlot = 0;
	polyLayout[2].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polyLayout[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polyLayout[2].InstanceDataStepRate = 0;


	polyLayout[3].SemanticName = "COLOR";
	polyLayout[3].SemanticIndex = 0;
	polyLayout[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	polyLayout[3].InputSlot = 0;
	polyLayout[3].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polyLayout[3].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polyLayout[3].InstanceDataStepRate = 0;

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

	D3D11_BUFFER_DESC matrixBufferDesc;
	//Setup the description of the dynamic matrix constant buffer that is in the shader..
	matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	matrixBufferDesc.ByteWidth = sizeof(MatrixBuffer);
	matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	matrixBufferDesc.MiscFlags = 0;
	matrixBufferDesc.StructureByteStride = 0;

	//Create the buffer so we can access it from within this class
	result = pDevice->CreateBuffer(&matrixBufferDesc, NULL, &m_pMatrixBuffer);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create matrix buffer");
		return false;
	}

	D3D11_BUFFER_DESC lightBufferDesc;
	//Setup description of the light dynamic constant buffer in the pixel shader
	lightBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	lightBufferDesc.ByteWidth = sizeof(LightBuffer);
	lightBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	lightBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	lightBufferDesc.MiscFlags = 0;
	lightBufferDesc.StructureByteStride = 0;

	//Create the buffer so we can access it from within this class
	result = pDevice->CreateBuffer(&lightBufferDesc, nullptr, &m_pLightBuffer);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create light buffer");
		return false;
	}

	D3D11_SAMPLER_DESC samplerDesc;
	// Create a texture sampler state description.
	samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 16;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samplerDesc.BorderColor[0] = 0;
	samplerDesc.BorderColor[1] = 0;
	samplerDesc.BorderColor[2] = 0;
	samplerDesc.BorderColor[3] = 0;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// Create the texture sampler state.
	result = pDevice->CreateSamplerState(&samplerDesc, &m_pSampleState);
	if (FAILED(result))
	{
		return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Material::ShutdownShader()
{
	if (m_pLightBuffer)
	{
		m_pLightBuffer->Release();
		m_pLightBuffer = nullptr;
	}

	if (m_pSampleState)
	{
		m_pSampleState->Release();
		m_pSampleState = nullptr;
	}

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

void Material::OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename)
{
	//Get ptr to error message text buffer
	char* compileErrors = (char*)(errorMessage->GetBufferPointer());

	//get the length of the message..
	size_t u_iBufferSize = errorMessage->GetBufferSize();
	
	ofstream fout;
	fout.open("shader-error.txt");
	//Write out the error to the file..

	for (size_t i = 0; i < u_iBufferSize; i++)
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

bool Material::SetShaderParameters(ID3D11DeviceContext* pDeviceContext, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix, XMFLOAT3 vLightDirection, XMFLOAT4 vDiffuseColour)
{
	
	//Matrices need to be transposed before sending them into the shader for dx11..
	mWorldMatrix = XMMatrixTranspose(mWorldMatrix);
	mViewMatrix = XMMatrixTranspose(mViewMatrix);
	mProjectionMatrix = XMMatrixTranspose(mProjectionMatrix);

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	//Lock the matrixBuffer, set new matrices inside it, then unlock it.
	HRESULT result = pDeviceContext->Map(m_pMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to lock the matrix buffer");
		return false;
	}
	MatrixBuffer* pMatrixData;
	//Get pointer to the buffer data
	pMatrixData = (MatrixBuffer*)mappedResource.pData;

	pMatrixData->world = mWorldMatrix;
	pMatrixData->view = mViewMatrix;
	pMatrixData->projection = mProjectionMatrix;

	//Unlock the buffer
	pDeviceContext->Unmap(m_pMatrixBuffer, 0);

	LightBuffer* pLightData;
	//Lock the lightBuffer, set the lighting info and then unlock it
	result = pDeviceContext->Map(m_pLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to lock the light buffer");
		return false;
	}

	pLightData = (LightBuffer*)mappedResource.pData;
	pLightData->diffuseColour = vDiffuseColour;
	pLightData->lightDirection = vLightDirection;
	pLightData->padding = 0.f;

	//Unlock the buffer
	pDeviceContext->Unmap(m_pLightBuffer, 0);

	//Set the position of the buffer in the HLSL shader
	unsigned int u_iBufferNumber = 0;

	//Finally, set the buffer in the shader to the new values
	pDeviceContext->VSSetConstantBuffers(u_iBufferNumber, 1, &m_pMatrixBuffer);

	pDeviceContext->PSSetConstantBuffers(u_iBufferNumber, 1, &m_pLightBuffer);

	ID3D11ShaderResourceView* pTex = m_pTexture->GetTexture();
	pDeviceContext->PSSetShaderResources(0, 1, &pTex);

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Material::RenderShader(ID3D11DeviceContext* pDeviceContext, int iIndexCount)
{
	//Set the vertex input layout
	pDeviceContext->IASetInputLayout(m_pLayout);

	//Set the shaders that are used to render the triangle..
	pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

	//Set the sampler state
	pDeviceContext->PSSetSamplers(0, 1, &m_pSampleState);

	//Render the triangle
	pDeviceContext->DrawIndexed(iIndexCount, 0, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
