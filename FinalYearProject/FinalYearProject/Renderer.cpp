#include "Renderer.h"
#include "Debugging.h"
#include "InputManager.h"
#include "../FW1FontWrapper/FW1FontWrapper.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Renderer::Renderer()
	: m_pD3D(nullptr)
	, m_pCamera(nullptr)
	, m_pModel(nullptr)
	, m_pFullScreenWindow(nullptr)
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Renderer::~Renderer()
{
	Shutdown();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Renderer::Initialise(int iScreenWidth, int iScreenHeight, HWND hwnd)
{
	m_pD3D = new D3DWrapper;
	if (!m_pD3D)
	{
		VS_LOG_VERBOSE("Failed to create D3DWrapper");
		return false;
	}

	//Initialise D3DWrapper..
	if (!m_pD3D->Initialise(iScreenWidth, iScreenHeight, VSYNC_ENABLED, hwnd, FULL_SCREEN, SCREEN_DEPTH, SCREEN_NEAR))
	{
		MessageBox(hwnd, L"Could not initialise the D3DWrapper", L"Error", MB_OK);
		return false;
	}

	m_pCamera = new Camera;
	if (!m_pCamera)
	{
		VS_LOG_VERBOSE("Could not create Camera");
		return false;
	}
	m_pCamera->SetPosition(0.f, 0.f, -10.f);

	m_pFullScreenWindow = new OrthoWindow;
	if (!m_pFullScreenWindow)
	{
		VS_LOG_VERBOSE("Failed to create orthographic window")
		return false;
	}
	if (FAILED(m_pFullScreenWindow->Initialize(m_pD3D->GetDevice(), iScreenWidth, iScreenHeight)))
	{
		VS_LOG_VERBOSE("Failed to initialise orthographic window");
		return false;
	}

	if (FAILED(m_DeferredRender.Initialise(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, iScreenWidth, iScreenHeight, SCREEN_DEPTH, SCREEN_NEAR)))
	{
		VS_LOG_VERBOSE("Failed to initialise deferred buffers");
		return false;
	}

	//Create the mesh..
	m_pModel = new Mesh;
	if (!m_pModel)
	{
		VS_LOG_VERBOSE("Could not create Mesh");
		return false;
	}

	if (!m_pModel->Initialise(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, "../Assets/Models/sponza_tri1.obj"))
	{
		VS_LOG_VERBOSE("Unable to initialise mesh");
		return false;
	}

	LightManager* pLightManager = LightManager::Get();
	if (pLightManager)
	{
		pLightManager->SetAmbientLightColour(XMFLOAT4(0.1f, 0.1f, 0.1f, 1.f));

		pLightManager->AddPointLight(XMFLOAT3(0.f, 750.f, 0.f), XMFLOAT4(1.f, 1.f, 0.8f, 1.f), 3000.f);
		pLightManager->GetPointLight(0)->AddShadowMap(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, SCREEN_NEAR, pLightManager->GetPointLight(0)->GetRange());
		pLightManager->AddPointLight(XMFLOAT3(1250.f, 625.f, -425.f), XMFLOAT4(0.f, 0.f, 1.f, 1.f), 3000.f);
		pLightManager->GetPointLight(1)->AddShadowMap(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, SCREEN_NEAR, pLightManager->GetPointLight(1)->GetRange());
 	//	pLightManager->AddPointLight(XMFLOAT3(-1270.f, 625.f, 425.f), XMFLOAT4(0.f, 1.f, 1.f, 1.f), 400.f);
	//	pLightManager->GetPointLight(2)->AddShadowMap(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, SCREEN_NEAR, pLightManager->GetPointLight(2)->GetRange());
 	//	pLightManager->AddPointLight(XMFLOAT3(1250.f, 625.f, 425.f), XMFLOAT4(1.f, 1.f, 0.f, 1.f), 400.f);
	//	pLightManager->GetPointLight(3)->AddShadowMap(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, SCREEN_NEAR, pLightManager->GetPointLight(3)->GetRange());

		pLightManager->AddDirectionalLight();
		pLightManager->SetDirectionalLightColour(XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f));
		pLightManager->SetDirectionalLightDirection(XMFLOAT3(0.2f, -0.1f, 0.2f));
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::Shutdown()
{
	m_DeferredRender.Shutdown();

	//deallocate the model
	if (m_pModel)
	{
		delete m_pModel;
		m_pModel = nullptr;
	}

	//deallocate the camera
	if (m_pCamera)
	{
		delete m_pCamera;
		m_pCamera = nullptr;
	}

	

	if (m_pFullScreenWindow)
	{
		delete m_pFullScreenWindow;
		m_pFullScreenWindow = nullptr;
	}

	LightManager::Get()->Shutdown();

	//deallocate the D3DWrapper
	if (m_pD3D)
	{
		delete m_pD3D;
		m_pD3D = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Renderer::Update(HWND hwnd)
{

	m_pCamera->Update();

	if (InputManager::Get()->IsKeyPressed(DIK_R))
	{
		m_pModel->ReloadShaders(m_pD3D->GetDevice(), hwnd);
	}

	//Render the scene
	if (!Render())
	{
		VS_LOG_VERBOSE("Render function failed")
		return false;
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Renderer::Render()
{
	//Generate view matrix based on camera position
	m_pCamera->Render();

	XMMATRIX mView, mProjection, mWorld, mOrtho, mBaseView;
	//Get the world, view and proj matrix from the camera and d3d objects..
	m_pCamera->GetViewMatrix(mView);
	m_pD3D->GetWorldMatrix(mWorld);
	m_pD3D->GetProjectionMatrix(mProjection);

	m_DeferredRender.SetRenderTargets(m_pD3D->GetDeviceContext());
	m_DeferredRender.ClearRenderTargets(m_pD3D->GetDeviceContext(), 0.f, 0.f, 0.f, 1.f);
	
	//Render the model to the deferred buffers
	m_pModel->RenderToBuffers(m_pD3D->GetDeviceContext(), mWorld, mView, mProjection);

	//Render the model to the shadow maps
	m_pModel->RenderShadows(m_pD3D->GetDeviceContext(), mWorld, mView, mProjection, LightManager::Get()->GetDirectionalLightDirection(), LightManager::Get()->GetDirectionalLightColour(), XMFLOAT4(0.1f, 0.1f, 0.1f, 1.f), m_pCamera->GetPosition());
	
	//Clear buffers to begin the scene
	m_pD3D->BeginScene(0.5f, 0.5f, 0.5f, 1.f);

	//Prep for 2D rendering..
	m_pD3D->GetOrthoMatrix(mOrtho);
	m_pCamera->RenderBaseViewMatrix();
	m_pCamera->GetBaseViewMatrix(mBaseView);

	m_pD3D->SetRenderOutputToScreen();
	m_pD3D->TurnZBufferOff();
	m_pFullScreenWindow->Render(m_pD3D->GetDeviceContext());

	m_DeferredRender.RenderLightingPass(m_pD3D->GetDeviceContext(), m_pFullScreenWindow->GetIndexCount(), mWorld, mBaseView, mOrtho, m_pCamera->GetPosition());

	IFW1Factory *pFW1Factory;
	HRESULT hResult = FW1CreateFactory(FW1_VERSION, &pFW1Factory);

	IFW1FontWrapper *pFontWrapper;
	hResult = pFW1Factory->CreateFontWrapper(m_pD3D->GetDevice(), L"Arial", &pFontWrapper);

	pFontWrapper->DrawString(
		m_pD3D->GetDeviceContext(),
		L"Text",// String
		128.0f,// Font size
		100.0f,// X position
		50.0f,// Y position
		0xff0099ff,// Text color, 0xAaBbGgRr
		0// Flags (for example FW1_RESTORESTATE to keep context states unchanged)
		);
	m_pD3D->GetDeviceContext()->GSSetShader(nullptr, nullptr, 0);
	pFontWrapper->Release();
	pFW1Factory->Release();

	//Go back to 3D rendering
	m_pD3D->TurnZBufferOn();

	//Present the rendered scene to the screen
	m_pD3D->EndScene();

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////