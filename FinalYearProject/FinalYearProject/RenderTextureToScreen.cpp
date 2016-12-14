#include "RenderTextureToScreen.h"
#include "Debugging.h"


HRESULT RenderTextureToScreen::Initialise(ID3D11Device3* pDevice, ID3D11DeviceContext* pContext, HWND hwnd, int iTextureWidth, int iTextureHeight, float fScreenDepth, float fScreenNear)
{
	D3D11_BUFFER_DESC matrixBufferDesc;
	//Setup the description of the dynamic matrix constant buffer that is in the shader..
	matrixBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	matrixBufferDesc.ByteWidth = sizeof(DeferredRender::MatrixBuffer);
	matrixBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	matrixBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	matrixBufferDesc.MiscFlags = 0;
	matrixBufferDesc.StructureByteStride = 0;

	//Create the buffer so we can access it
	if (FAILED(pDevice->CreateBuffer(&matrixBufferDesc, nullptr, &m_pMatrixBuffer)))
	{
		VS_LOG_VERBOSE("Failed to create matrix buffer");
		return false;
	}

	// Setup the viewport for rendering.
	m_viewport.Width = (float)iTextureWidth;
	m_viewport.Height = (float)iTextureHeight;
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;
	m_viewport.TopLeftX = 0.0f;
	m_viewport.TopLeftY = 0.0f;

	

	// Create a texture sampler state description.
	D3D11_SAMPLER_DESC samplerDesc;
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samplerDesc.BorderColor[0] = 0;
	samplerDesc.BorderColor[1] = 0;
	samplerDesc.BorderColor[2] = 0;
	samplerDesc.BorderColor[3] = 0;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// Create the texture sampler state.
	if (FAILED(pDevice->CreateSamplerState(&samplerDesc, &m_pSampleState)))
	{
		VS_LOG_VERBOSE("Failed to create texture sampler state");
		return false;
	}

	InitialiseShader(pDevice, hwnd, L"../Assets/Shaders/Debug/RenderTexToScreen.hlsl");
}

void RenderTextureToScreen::Shutdown()
{
	if (m_pMatrixBuffer)
	{
		m_pMatrixBuffer->Release();
		m_pMatrixBuffer = nullptr;
	}

	if (m_pSampleState)
	{
		m_pSampleState->Release();
		m_pSampleState = nullptr;
	}
	if (m_pRenderTexturePass)
	{
		m_pRenderTexturePass->Shutdown();
		delete m_pRenderTexturePass;
		m_pRenderTexturePass = nullptr;
	}
}

bool RenderTextureToScreen::RenderTexture(ID3D11DeviceContext3* pContext, int iIndexCount, XMMATRIX mWorld, XMMATRIX mView, XMMATRIX mProjection, const XMFLOAT3& vCamPos, Texture2D* pTexture)
{
	if (!SetShaderParameters(pContext, mWorld, mView, mProjection, vCamPos, pTexture))
	{
		return false;
	}

	RenderShader(pContext, iIndexCount);

	return true;
}

bool RenderTextureToScreen::InitialiseShader(ID3D11Device3* pDevice, HWND hwnd, WCHAR* sShaderFilename)
{
	
	//Create vertex input layout description..
	D3D11_INPUT_ELEMENT_DESC polygonLayout[2];
	
	polygonLayout[0].SemanticName = "POSITION";
	polygonLayout[0].SemanticIndex = 0;
	polygonLayout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	polygonLayout[0].InputSlot = 0;
	polygonLayout[0].AlignedByteOffset = 0;
	polygonLayout[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[0].InstanceDataStepRate = 0;

	polygonLayout[1].SemanticName = "TEXCOORD";
	polygonLayout[1].SemanticIndex = 0;
	polygonLayout[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	polygonLayout[1].InputSlot = 0;
	polygonLayout[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	polygonLayout[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	polygonLayout[1].InstanceDataStepRate = 0;

	unsigned int numElements = sizeof(polygonLayout) / sizeof(polygonLayout[0]);

	m_pRenderTexturePass = new RenderPass;
	m_pRenderTexturePass->Initialise(pDevice, hwnd, polygonLayout, numElements, sShaderFilename, "VSMain", nullptr, "PSMain");

	return true;
}

bool RenderTextureToScreen::SetShaderParameters(ID3D11DeviceContext* pContext, XMMATRIX mWorld, XMMATRIX mView, XMMATRIX mProjection, const XMFLOAT3& vCameraPos, Texture2D* pTexture)
{
	mWorld = XMMatrixTranspose(mWorld);
	mView = XMMatrixTranspose(mView);
	mProjection = XMMatrixTranspose(mProjection);


	D3D11_MAPPED_SUBRESOURCE mappedResource;

	if (FAILED(pContext->Map(m_pMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
	{
		VS_LOG_VERBOSE("Failed to lock the matrix buffer");
		return false;
	}
	DeferredRender::MatrixBuffer* pMatrixData;
	//Get pointer to the buffer data
	pMatrixData = (DeferredRender::MatrixBuffer*)mappedResource.pData;

	pMatrixData->mWorld = mWorld;
	pMatrixData->mView = mView;
	pMatrixData->mProjection = mProjection;

	//Unlock the buffer
	pContext->Unmap(m_pMatrixBuffer, 0);

	unsigned int u_iBufferNumber = 0;
	pContext->VSSetConstantBuffers(u_iBufferNumber, 1, &m_pMatrixBuffer);

	ID3D11ShaderResourceView* pResource = pTexture->GetShaderResourceView();
	pContext->PSSetShaderResources(0, 1, &pResource);

	return true;
}

void RenderTextureToScreen::RenderShader(ID3D11DeviceContext3* pContext, int iIndexCount)
{
	m_pRenderTexturePass->SetActiveRenderPass(pContext);

	// Set the sampler state in the pixel shader.
	pContext->PSSetSamplers(0, 1, &m_pSampleState);

	// Render the geometry.
	pContext->DrawIndexed(iIndexCount, 0, 0);

	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
	pContext->PSSetShaderResources(0, 1, nullSRV);

	ID3D11SamplerState* sample[1] = { nullptr };
	pContext->PSSetSamplers(0, 1, sample);
}

void RenderTextureToScreen::OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename)
{
	//Get ptr to error message text buffer
	char* compileErrors = (char*)(errorMessage->GetBufferPointer());

	//get the length of the message..
	size_t u_iBufferSize = errorMessage->GetBufferSize();

	std::ofstream fout;
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