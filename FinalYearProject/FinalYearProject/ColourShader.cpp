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
	return InitialiseShader(pDevice, hwnd, L"Colour.hlsl", L"Colour.hlsl");
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

bool ColourShader::InitialiseShader(ID3D11Device* pDevice, HWND hwnd, WCHAR* sVSFilename, WCHAR* sPSFilename)
{
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ColourShader::ShutdownShader()
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ColourShader::OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename)
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ColourShader::SetShaderParameters(ID3D11DeviceContext* pDeviceContext, XMMATRIX mWorldMatrix, XMMATRIX mViewMatrix, XMMATRIX mProjectionMatrix)
{
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ColourShader::RenderShader(ID3D11DeviceContext* pDeviceContext, int iIndexCount)
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
