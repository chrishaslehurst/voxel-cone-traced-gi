#include "RenderPass.h"
#include "Debugging.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RenderPass::RenderPass()
	: m_pVertexShader(nullptr)
	, m_pGeometryShader(nullptr)
	, m_pPixelShader (nullptr)
	, m_pLayout(nullptr)
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT RenderPass::Initialise(ID3D11Device3* pDevice, ID3D11DeviceContext* pContext, HWND hwnd, D3D11_INPUT_ELEMENT_DESC* pPolyLayout, int iNumLayoutElements, WCHAR* sShaderFilename, char* sVSEntry, char* sGSEntry, char* sPSEntry)
{
	ID3D10Blob* pErrorMessage(nullptr);

	ID3D10Blob* pVertexShaderBuffer(nullptr);
	ID3D10Blob* pGeometryShaderBuffer(nullptr);
	ID3D10Blob* pPixelShaderBuffer(nullptr);

	//Compile the voxelisation pass vertex shader code
	HRESULT result = D3DCompileFromFile(sShaderFilename, nullptr, nullptr, sVSEntry, "vs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, &pVertexShaderBuffer, &pErrorMessage);
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

	//Compile the voxelisation pass geometry shader code
	result = D3DCompileFromFile(sShaderFilename, nullptr, nullptr, sGSEntry, "gs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, &pGeometryShaderBuffer, &pErrorMessage);
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

	result = pDevice->CreateGeometryShader(pGeometryShaderBuffer->GetBufferPointer(), pGeometryShaderBuffer->GetBufferSize(), nullptr, &m_pGeometryShader);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create the geometry shader.");
		return false;
	}

	//Compile the voxelisation pass pixel shader code
	result = D3DCompileFromFile(sShaderFilename, nullptr, nullptr, sPSEntry, "ps_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, &pPixelShaderBuffer, &pErrorMessage);
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

	result = pDevice->CreatePixelShader(pPixelShaderBuffer->GetBufferPointer(), pPixelShaderBuffer->GetBufferSize(), nullptr, &m_pPixelShader);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create the pixel shader.");
		return false;
	}

	//Create the vertex input layout..
	result = pDevice->CreateInputLayout(pPolyLayout, iNumLayoutElements, pVertexShaderBuffer->GetBufferPointer(), pVertexShaderBuffer->GetBufferSize(), &m_pLayout);
	if (FAILED(result))
	{
		VS_LOG_VERBOSE("Failed to create input layout");
		return false;
	}

	pErrorMessage->Release();
	pErrorMessage = nullptr;

	pVertexShaderBuffer->Release();
	pVertexShaderBuffer = nullptr;

	pPixelShaderBuffer->Release();
	pPixelShaderBuffer = nullptr;

	pGeometryShaderBuffer->Release();
	pGeometryShaderBuffer = nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RenderPass::SetActiveRenderPass(ID3D11DeviceContext3* pDeviceContext)
{
	pDeviceContext->IASetInputLayout(m_pLayout);
	pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	pDeviceContext->GSSetShader(m_pGeometryShader, nullptr, 0);
	pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RenderPass::Shutdown()
{
	if (m_pVertexShader)
	{
		m_pVertexShader->Release();
		m_pVertexShader = nullptr;
	}
	if (m_pPixelShader)
	{
		m_pPixelShader->Release();
		m_pPixelShader = nullptr;
	}
	if (m_pGeometryShader)
	{
		m_pGeometryShader->Release();
		m_pGeometryShader = nullptr;
	}
	if (m_pLayout)
	{
		m_pLayout->Release();
		m_pLayout = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RenderPass::OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename)
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
