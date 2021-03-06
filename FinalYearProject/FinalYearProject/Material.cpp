#include "Material.h"
#include "Debugging.h"
#include "LightManager.h"



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Material::Material()
	: m_pRenderToBuffersPass(nullptr)
	, m_pMatrixBuffer(nullptr)
	, m_pDiffuseTexture(nullptr)
	, m_pNormalMap(nullptr)
	, m_pRoughnessMap(nullptr)
	, m_pSpecularMap(nullptr)
	, m_pMetallicMap(nullptr)
	, m_pAlphaMask(nullptr)
	, m_pSampleState(nullptr)
	, m_bHasSpecularMap(false)
	, m_bHasNormalMap(false)
	, m_bHasAlphaMask(false)
	, m_pShadowMapSampler(nullptr)
	, m_bHasDiffuseTexture(false)
{
	
	m_vSpecularColour = XMFLOAT4(1.f, 1.f, 1.f, 1.f);
	m_fSpecularPower = 1.f;

	m_defines[USE_TEXTURE].Name = "USE_TEXTURE";
	m_defines[USE_TEXTURE].Definition = "0";
	m_defines[USE_NORMAL_MAPS].Name = "USE_NORMAL_MAPS";
	m_defines[USE_NORMAL_MAPS].Definition = "0";
	m_defines[USE_SPECULAR_MAPS].Name = "USE_SPECULAR_MAPS";
	m_defines[USE_SPECULAR_MAPS].Definition = "0";
	m_defines[USE_ALPHA_MASKS].Name = "USE_ALPHA_MASKS";
	m_defines[USE_ALPHA_MASKS].Definition = "0";
	m_defines[USE_PHYSICALLY_BASED_SHADING].Name = "USE_PHYSICALLY_BASED_SHADING";
	m_defines[USE_PHYSICALLY_BASED_SHADING].Definition = "0";
	m_defines[NULLS].Name = nullptr;
	m_defines[NULLS].Definition = nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Material::~Material()
{
	Shutdown();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Material::Initialise(ID3D11Device3* pDevice, ID3D11DeviceContext* pContext, HWND hwnd)
{
	if (m_bHasRoughnessMap && m_bHasMetallicMap)
	{
		m_defines[USE_PHYSICALLY_BASED_SHADING].Definition = "1";
	}
	if (!InitialiseShader(pDevice, hwnd, L"../Assets/Shaders/DeferredShader.hlsl"))
	{
		return false;
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Material::Shutdown()
{
	ReleaseTextures();

	ShutdownShader();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Material::Render(ID3D11DeviceContext* pDeviceContext, int iIndexCount, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix)
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

void Material::ReloadShader(ID3D11Device3* pDevice, HWND hwnd)
{
	ShutdownShader();
	InitialiseShader(pDevice, hwnd, L"../Assets/Shaders/DeferredShader.hlsl");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Material::SetSpecularProperties(float r, float g, float b, float power)
{
	m_vSpecularColour.x = r;
	m_vSpecularColour.y = g;
	m_vSpecularColour.z = b;
	m_fSpecularPower = power;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Material::SetDiffuseTexture(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* diffuseTexFilename)
{
	m_pDiffuseTexture = LoadTexture(pDevice, pContext, diffuseTexFilename);
	m_defines[USE_TEXTURE].Definition = "1";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Material::SetSpecularMap(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* specMapFilename)
{
	m_pSpecularMap = LoadTexture(pDevice, pContext, specMapFilename);
	m_defines[USE_SPECULAR_MAPS].Definition = "1";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Material::SetNormalMap(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* normalMapFilename)
{
	m_pNormalMap = LoadTexture(pDevice, pContext, normalMapFilename);
	m_defines[USE_NORMAL_MAPS].Definition = "1";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Material::SetAlphaMask(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* alphaMaskFilename)
{
	m_pAlphaMask = LoadTexture(pDevice, pContext, alphaMaskFilename);
	m_defines[USE_ALPHA_MASKS].Definition = "1";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Material::SetRoughnessMap(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* roughnessMapFilename)
{
	m_pRoughnessMap = LoadTexture(pDevice, pContext, roughnessMapFilename);
	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Material::SetMetallicMap(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* metallicMapFilename)
{
	m_pMetallicMap = LoadTexture(pDevice, pContext, metallicMapFilename);
	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Texture2D* Material::LoadTexture(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, WCHAR* filename)
{
	Texture2D* pTexture = new Texture2D;
	if(!pTexture)
	{
		VS_LOG_VERBOSE("Failed to create new texture");
		return false;
	}

	if (!pTexture->LoadTextureFromFile(pDevice, pContext, filename))
	{
		VS_LOG_VERBOSE("Failed to load texture from file");
		return false;
	}

	return pTexture;
	
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Material::ReleaseTextures()
{
	if (m_pDiffuseTexture)
	{
		delete m_pDiffuseTexture;
		m_pDiffuseTexture = nullptr;
	}

	if (m_pNormalMap)
	{
		delete m_pNormalMap;
		m_pNormalMap = nullptr;
	}

	if (m_pSpecularMap)
	{
		delete m_pSpecularMap;
		m_pSpecularMap = nullptr;
	}

	if (m_pRoughnessMap)
	{
		delete m_pRoughnessMap;
		m_pRoughnessMap = nullptr;
	}

	if (m_pMetallicMap)
	{
		delete m_pMetallicMap;
		m_pMetallicMap = nullptr;
	}

	if (m_pAlphaMask)
	{
		delete m_pAlphaMask;
		m_pAlphaMask = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Material::InitialiseShader(ID3D11Device3* pDevice, HWND hwnd, WCHAR* sShaderFilename)
{
	HRESULT result;
	

	D3D11_INPUT_ELEMENT_DESC polyLayout[5];
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

	polyLayout[3].SemanticName = "TANGENT";
	polyLayout[3].SemanticIndex = 0;
	polyLayout[3].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polyLayout[3].InputSlot = 0;
	polyLayout[3].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polyLayout[3].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polyLayout[3].InstanceDataStepRate = 0;

	polyLayout[4].SemanticName = "BINORMAL";
	polyLayout[4].SemanticIndex = 0;
	polyLayout[4].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polyLayout[4].InputSlot = 0;
	polyLayout[4].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polyLayout[4].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polyLayout[4].InstanceDataStepRate = 0;

	//Get the number of elements in the layout
	unsigned int iNumElements(sizeof(polyLayout) / sizeof(polyLayout[0]));

	m_pRenderToBuffersPass = new RenderPass;
	m_pRenderToBuffersPass->Initialise(pDevice, hwnd, polyLayout, iNumElements, sShaderFilename, "VSMain", nullptr, "PSMain");

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

	if (m_pSampleState)
	{
		m_pSampleState->Release();
		m_pSampleState = nullptr;
	}
	if (m_pShadowMapSampler)
	{
		m_pShadowMapSampler->Release();
		m_pShadowMapSampler = nullptr;
	}

	if (m_pMatrixBuffer)
	{
		m_pMatrixBuffer->Release();
		m_pMatrixBuffer = nullptr;
	}
	if (m_pRenderToBuffersPass)
	{
		m_pRenderToBuffersPass->Shutdown();
		delete m_pRenderToBuffersPass;
		m_pRenderToBuffersPass = nullptr;
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

bool Material::SetShaderParameters(ID3D11DeviceContext* pDeviceContext, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix)
{

	pixelShaderResourceCount = 0;
	
	if (m_pDiffuseTexture)
	{
		ID3D11ShaderResourceView* pDiffuseTexture = m_pDiffuseTexture->GetShaderResourceView();
		pDeviceContext->PSSetShaderResources(pixelShaderResourceCount, 1, &pDiffuseTexture);
		pixelShaderResourceCount++;
	}
	if (m_bHasNormalMap)
	{
		ID3D11ShaderResourceView* pNormalMapTexture = m_pNormalMap->GetShaderResourceView();
		pDeviceContext->PSSetShaderResources(pixelShaderResourceCount, 1, &pNormalMapTexture);
		pixelShaderResourceCount++;
	}
	
	if (m_bHasRoughnessMap)
	{
		ID3D11ShaderResourceView* pRoughnessMap = m_pRoughnessMap->GetShaderResourceView();
		pDeviceContext->PSSetShaderResources(pixelShaderResourceCount, 1, &pRoughnessMap);
		pixelShaderResourceCount++;
	}
	if (m_bHasMetallicMap)
	{
		ID3D11ShaderResourceView* pMetallicMap = m_pMetallicMap->GetShaderResourceView();
		pDeviceContext->PSSetShaderResources(pixelShaderResourceCount, 1, &pMetallicMap);
		pixelShaderResourceCount++;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Material::RenderShader(ID3D11DeviceContext* pDeviceContext, int iIndexCount)
{
	//Render the triangle
	pDeviceContext->DrawIndexed(iIndexCount, 0, 0);

}

bool Material::SetPerFrameShaderParameters(ID3D11DeviceContext* pDeviceContext, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix)
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

	//Set the position of the buffer in the HLSL shader
	unsigned int u_iBufferNumber = 0;

	//Finally, set the buffers in the shader to the new values
	pDeviceContext->VSSetConstantBuffers(u_iBufferNumber, 1, &m_pMatrixBuffer);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Material::SetShadersAndSamplers(ID3D11DeviceContext3* pDeviceContext)
{
	m_pRenderToBuffersPass->SetActiveRenderPass(pDeviceContext);

	//Set the sampler state
	pDeviceContext->PSSetSamplers(0, 1, &m_pSampleState);
}