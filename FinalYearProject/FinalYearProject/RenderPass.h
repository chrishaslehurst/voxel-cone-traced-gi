#ifndef RENDER_PASS_H
#define RENDER_PASS_H
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <d3d11_3.h>
#include <DirectXMath.h>
#include <D3DCompiler.h>
#include <fstream>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace DirectX;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class RenderPass
{

public:
	RenderPass();
	HRESULT Initialise(ID3D11Device3* pDevice, HWND hwnd, D3D11_INPUT_ELEMENT_DESC* pPolyLayout, int iNumLayoutElements, WCHAR* sShaderFilename, char* sVSEntry, char* sGSEntry, char* sPSEntry);
	void SetActiveRenderPass(ID3D11DeviceContext3* pDeviceContext);
	//void SetBuffersAndResources();
	void Shutdown();
private:
	void OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename);

	ID3D11VertexShader*		m_pVertexShader;
	ID3D11PixelShader*		m_pPixelShader;
	ID3D11GeometryShader*	m_pGeometryShader;

	ID3D11InputLayout*		m_pLayout;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif