#include "Renderer.h"
#include "Debugging.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Renderer::Renderer()
	: m_pD3D(nullptr)
	, m_pCamera(nullptr)
	, m_pModel(nullptr)
	, m_pDirectionalLight(nullptr)
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Renderer::~Renderer()
{

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

	//Create the mesh..
	m_pModel = new Mesh;
	if (!m_pModel)
	{
		VS_LOG_VERBOSE("Could not create Mesh");
		return false;
	}

	if (!m_pModel->Initialise(m_pD3D->GetDevice(), hwnd, "../Assets/Models/sponza_tri1.obj"))
	{
		VS_LOG_VERBOSE("Unable to initialise mesh");
		return false;
	}

	//Create the directional light
	m_pDirectionalLight = new DirectionalLight;
	if (!m_pDirectionalLight)
	{
		VS_LOG_VERBOSE("Unable to create Directional Light");
		return false;
	}
	m_pDirectionalLight->SetDiffuseColour(1.0f, 1.0f, 1.0f, 1.0f);
	m_pDirectionalLight->SetDirection(0.0f, -0.5f, 0.5f);

	LightManager* pLightManager = LightManager::Get();
	if (pLightManager)
	{
		pLightManager->AddPointLight(XMFLOAT3(0.f, 900.f, 0.f), XMFLOAT4(1.f, 1.f, 1.f, 1.f), 1800.f);
		pLightManager->AddPointLight(XMFLOAT3(1250.f, 625.f, -425.f), XMFLOAT4(0.f, 0.f, 1.f, 1.f), 400.f);
		pLightManager->AddPointLight(XMFLOAT3(-1270.f, 625.f, 425.f), XMFLOAT4(0.f, 1.f, 1.f, 1.f), 400.f);
		pLightManager->AddPointLight(XMFLOAT3(1250.f, 625.f, 425.f), XMFLOAT4(1.f, 1.f, 0.f, 1.f), 400.f);
	}


	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::Shutdown()
{
	if (m_pDirectionalLight)
	{
		delete m_pDirectionalLight;
		m_pDirectionalLight = nullptr;
	}

	if (m_pModel)
	{
		m_pModel->Shutdown();
		delete m_pModel;
		m_pModel = nullptr;
	}

	if (m_pCamera)
	{
		delete m_pCamera;
		m_pCamera = nullptr;
	}

	//deallocate the D3DWrapper
	if (m_pD3D)
	{
		delete m_pD3D;
		m_pD3D = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Renderer::Update()
{
	m_pCamera->Update();
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

	//Clear buffers to begin the scene
	m_pD3D->BeginScene(0.5f, 0.5f, 0.5f, 1.f);

	//Generate view matrix based on camera position
	m_pCamera->Render();


	XMMATRIX mView, mProjection, mWorld;
	//Get the world, view and proj matrix from the camera and d3d objects..
	m_pCamera->GetViewMatrix(mView);
	m_pD3D->GetWorldMatrix(mWorld);
	m_pD3D->GetProjectionMatrix(mProjection);

	
	//////////////////////////////////////////////////

	//Put the model vert and ind buffers on the graphics pipeline to prep them for drawing..
	m_pModel->Render(m_pD3D->GetDeviceContext(), mWorld, mView, mProjection, m_pDirectionalLight->GetDirection(), m_pDirectionalLight->GetDiffuseColour(), XMFLOAT4(0.0f, 0.0f, 0.0f, 1.f), m_pCamera->GetPosition());

	//Present the rendered scene to the screen
	m_pD3D->EndScene();

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////