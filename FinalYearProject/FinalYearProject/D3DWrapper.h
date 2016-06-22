#ifndef D3D_WRAPPER_H
#define D3D_WRAPPER_H

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <d3d11_3.h>

//#include <dxgi.h>
//#include <d3dcompiler.h>
#include <directxmath.h>

using namespace DirectX;

class D3DWrapper
{

public:
	D3DWrapper();
	~D3DWrapper();

	bool Initialise(int iScreenWidth, int iScreenHeight, bool bVsyncEnabled, HWND hwnd, bool bFullScreenEnabled, float fScreenDepth, float fScreenNear);
	void BeginScene(float r, float g, float b, float a);
	void EndScene();

	ID3D11Device3* GetDevice();
	ID3D11DeviceContext3* GetDeviceContext();

	void GetProjectionMatrix(XMMATRIX& mProjectionMatrix);
	void GetWorldMatrix(XMMATRIX& mWorldMatrix);
	void GetOrthoMatrix(XMMATRIX& mOrthoMatrix);

	void GetVideoCardInfo(char* cardName, int& memory);

	void TurnOnAlphaBlending();
	void TurnOffAlphaBlending();

private:
	void Shutdown();

	bool QueryRefreshRateAndGPUInfo(int iScreenWidth, int iScreenHeight, unsigned int& numerator, unsigned int& denominator);
	bool SetUpSwapChainAndDevice(HWND hwnd, bool bFullScreenEnabled, int iScreenWidth, int iScreenHeight, unsigned int numerator, unsigned int denominator);
	bool SetUpDepthBufferAndDepthStencilState(int iScreenWidth, int iScreenHeight);
	bool SetUpDepthStencilView();
	bool SetUpRasteriser();
	bool CreateBlendState();
	void SetUpViewPort(int iScreenWidth, int iScreenHeight);


	bool						m_bVsyncEnabled;
	int							m_iVideoCardMemoryInMB;
	char						m_videoCardDescription[128];
	IDXGISwapChain*				m_pSwapChain;
	ID3D11Device3*				m_pDevice;
	ID3D11DeviceContext3*		m_pDeviceContext;
	ID3D11RenderTargetView*		m_pRenderTargetView;
	ID3D11Texture2D*			m_pDepthStencilBuffer;
	ID3D11DepthStencilState*	m_pDepthStencilState;
	ID3D11DepthStencilView*		m_pDepthStencilView;

	ID3D11BlendState*			m_alphaEnableBlendingState;
	ID3D11BlendState*			m_alphaDisableBlendingState;
	ID3D11RasterizerState*		m_pRasterState;
	XMMATRIX					m_mProjectionMatrix;
	XMMATRIX					m_mWorldMatrix;
	XMMATRIX					m_mOrthoMatrix;

};

#endif 
