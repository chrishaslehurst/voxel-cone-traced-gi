#include "Renderer.h"
#include "Debugging.h"
#include "InputManager.h"
#include "GPUProfiler.h"
#include "DebugLog.h"
#include "Timer.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Renderer::Renderer()
	: m_pD3D(nullptr)
	, m_pCamera(nullptr)
	, m_pModel(nullptr)
	, m_pFullScreenWindow(nullptr)
{
	m_dCPUFrameStartTime = 0;
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
	m_pCamera->SetPosition(0.f, 0.f, 0.f);

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

	m_pCube = new Mesh;
	if (!m_pCube)
	{
		VS_LOG_VERBOSE("Could not create Mesh");
		return false;
	}

	if (!m_pModel->InitialiseFromObj(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, "../Assets/Models/sponza_tri1.obj"))
	{
		VS_LOG_VERBOSE("Unable to initialise mesh");
		return false;
	}

	LightManager* pLightManager = LightManager::Get();
	if (pLightManager)
	{
		pLightManager->SetAmbientLightColour(XMFLOAT4(0.05f, 0.05f, 0.05f, 1.f));

		pLightManager->AddPointLight(XMFLOAT3(0.f, 1250.f, 0.f), XMFLOAT4(1.f, 1.f, 0.8f, 1.f), 3000.f);
		pLightManager->GetPointLight(0)->AddShadowMap(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, SCREEN_NEAR, pLightManager->GetPointLight(0)->GetRange());
	//	pLightManager->AddPointLight(XMFLOAT3(1250.f, 625.f, -425.f), XMFLOAT4(0.f, 0.f, 1.f, 1.f), 3000.f);
	//	pLightManager->GetPointLight(1)->AddShadowMap(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, SCREEN_NEAR, pLightManager->GetPointLight(1)->GetRange());
 	//	pLightManager->AddPointLight(XMFLOAT3(-1270.f, 625.f, 425.f), XMFLOAT4(0.f, 1.f, 1.f, 1.f), 400.f);
	//	pLightManager->GetPointLight(2)->AddShadowMap(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, SCREEN_NEAR, pLightManager->GetPointLight(2)->GetRange());
 	//	pLightManager->AddPointLight(XMFLOAT3(1250.f, 625.f, 425.f), XMFLOAT4(1.f, 1.f, 0.f, 1.f), 400.f);
	//	pLightManager->GetPointLight(3)->AddShadowMap(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, SCREEN_NEAR, pLightManager->GetPointLight(3)->GetRange());

		pLightManager->AddDirectionalLight();
		pLightManager->SetDirectionalLightColour(XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f));
		pLightManager->SetDirectionalLightDirection(XMFLOAT3(0.2f, -0.1f, 0.2f));
	}
	m_DebugRenderTexture.Initialise(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, iScreenWidth, iScreenHeight, SCREEN_DEPTH, SCREEN_NEAR);
	m_VoxelisePass.Initialise(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, m_pModel->GetWholeModelAABB(), iScreenWidth, iScreenHeight);

	GPUProfiler::Get()->Initialise(m_pD3D->GetDevice());
	DebugLog::Get()->Initialise(m_pD3D->GetDevice());

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

	if (m_pCube)
	{
		delete m_pCube;
		m_pCube = nullptr;
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

	m_DebugRenderTexture.Shutdown();
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

	double dCPUFrameTime = (m_dCPUFrameEndTime - m_dCPUFrameStartTime) * 1000;
	m_dCPUFrameStartTime = Timer::Get()->GetCurrentTime();

	ID3D11DeviceContext3* pContext = m_pD3D->GetDeviceContext();

	//Generate view matrix based on camera position
	GPUProfiler::Get()->BeginFrame(pContext);
	m_pCamera->Render();

	XMMATRIX mView, mProjection, mWorld, mOrtho, mBaseView;
	//Get the world, view and proj matrix from the camera and d3d objects..
	m_pCamera->GetViewMatrix(mView);
	m_pD3D->GetWorldMatrix(mWorld);
	m_pD3D->GetProjectionMatrix(mProjection);
	m_pCamera->CalculateViewFrustum(SCREEN_DEPTH, mProjection);

	
	//Prep for 2D rendering..	
	m_pD3D->GetOrthoMatrix(mOrtho);
	m_pCamera->RenderBaseViewMatrix();
	m_pCamera->GetBaseViewMatrix(mBaseView);
	

	m_pD3D->TurnZBufferOff();
	m_pD3D->TurnOffAlphaBlending();

	//Clear the voxel volume..
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psVoxeliseClear);
	m_VoxelisePass.RenderClearVoxelsPass(pContext);
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psVoxeliseClear);

	//Render the scene to the volume..
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psVoxelisePass);
	m_VoxelisePass.RenderMesh(pContext, mWorld, mBaseView, mProjection, m_pCamera->GetPosition(), m_pModel);
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psVoxelisePass);

	m_pD3D->TurnZBufferOn();

	m_DeferredRender.SetRenderTargets(pContext);
	m_DeferredRender.ClearRenderTargets(pContext, 0.f, 0.f, 0.f, 1.f);

	//Render the model to the deferred buffers
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psRenderToBuffer);
	
	m_pModel->RenderToBuffers(pContext, mWorld, mView, mProjection, m_pCamera);
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psRenderToBuffer);

	

	//Render the model to the shadow maps
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psShadowRender);
	m_pD3D->RenderBackFacesOn();
	m_pModel->RenderShadows(pContext, mWorld, mView, mProjection, LightManager::Get()->GetDirectionalLightDirection(), LightManager::Get()->GetDirectionalLightColour(), XMFLOAT4(0.1f, 0.1f, 0.1f, 1.f), m_pCamera->GetPosition());
	m_pD3D->RenderBackFacesOff();
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psShadowRender);

	//Clear buffers to begin the scene
	m_pD3D->BeginScene(0.5f, 0.5f, 0.5f, 1.f);

	m_pD3D->SetRenderOutputToScreen();
	m_pD3D->TurnZBufferOff();
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psLightingPass);
	m_pFullScreenWindow->Render(m_pD3D->GetDeviceContext());


	//m_DebugRenderTexture.RenderTexture(pContext, m_pFullScreenWindow->GetIndexCount(), mWorld, mBaseView, mOrtho, m_pCamera->GetPosition(), m_DeferredRender.GetTexture(btNormals));
	m_DeferredRender.RenderLightingPass(pContext, m_pFullScreenWindow->GetIndexCount(), mWorld, mBaseView, mOrtho, m_pCamera->GetPosition());
	//m_VoxelisePass.RenderDebugViewToTexture(pContext);
	//m_DebugRenderTexture.RenderTexture(pContext, m_pFullScreenWindow->GetIndexCount(), mWorld, mBaseView, mOrtho, m_pCamera->GetPosition(), m_VoxelisePass.GetDebugTexture());
	
	m_dCPUFrameEndTime = Timer::Get()->GetCurrentTime();
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psLightingPass);
	//Go back to 3D rendering
	m_pD3D->TurnZBufferOn();
	m_pD3D->TurnOnAlphaBlending();

	m_VoxelisePass.RenderDebugCubes(pContext, mWorld, mView, mProjection);

	GPUProfiler::Get()->EndFrame(pContext);
	GPUProfiler::Get()->DisplayTimes(pContext, static_cast<float>(dCPUFrameTime));
	DebugLog::Get()->PrintLogToScreen(pContext);
	
	//Present the rendered scene to the screen
	m_pD3D->EndScene();

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////