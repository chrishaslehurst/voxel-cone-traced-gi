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
	, m_bDebugRenderVoxels(false)
	, m_bMinusPressed(false)
	, m_bPlusPressed(false)
	, m_bVPressed(false)
	, m_bGPressed(false)
	, m_eGITypeToRender(GIRenderFlag::giFull)
{
	SetGITypeString();
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
	m_pCamera->SetPosition(210.f, 200.f, 250.f);

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


	if (!m_pModel->InitialiseFromObj(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, "../Assets/Models/sponza_tri1.obj"))
	{
		VS_LOG_VERBOSE("Unable to initialise mesh");
		return false;
	}

	LightManager* pLightManager = LightManager::Get();
	if (pLightManager)
	{
		pLightManager->Initialise(m_pD3D->GetDevice());

		pLightManager->SetAmbientLightColour(XMFLOAT4(0.05f, 0.05f, 0.05f, 1.f));

		pLightManager->AddPointLight(XMFLOAT3(250.f, 500.f, 50.f), XMFLOAT4(1.f, 1.f, 0.8f, 1.f), 3000.f);
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
	m_VoxelisedScene.Initialise(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, m_pModel->GetWholeModelAABB());

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
	if (InputManager::Get()->IsKeyPressed(DIK_R))
	{
		m_pModel->ReloadShaders(m_pD3D->GetDevice(), hwnd);
	}
	if (InputManager::Get()->IsKeyPressed(DIK_V) && !m_bVPressed)
	{
		m_bDebugRenderVoxels = !m_bDebugRenderVoxels;
		m_bVPressed = true;
	}
	else if (InputManager::Get()->IsKeyReleased(DIK_V))
	{
		m_bVPressed = false;
	}
	if (InputManager::Get()->IsKeyPressed(DIK_NUMPADPLUS) && !m_bPlusPressed)
	{
		m_VoxelisedScene.IncreaseDebugMipLevel();
		m_bPlusPressed = true;
	}
	else if (InputManager::Get()->IsKeyReleased(DIK_NUMPADPLUS))
	{
		m_bPlusPressed = false;
	}
	if (InputManager::Get()->IsKeyPressed(DIK_NUMPADMINUS) && !m_bMinusPressed)
	{
		m_VoxelisedScene.DecreaseDebugMipLevel();
		m_bMinusPressed = true;
	}
	else if (InputManager::Get()->IsKeyReleased(DIK_NUMPADMINUS))
	{
		m_bMinusPressed = false;
	}
	if (InputManager::Get()->IsKeyPressed(DIK_G) && !m_bGPressed)
	{
		m_eGITypeToRender = static_cast<GIRenderFlag>((static_cast<int>(m_eGITypeToRender) + 1) % static_cast<int>(GIRenderFlag::giMax));
		SetGITypeString();
		m_bGPressed = true;
	}
	else if (InputManager::Get()->IsKeyReleased(DIK_G))
	{
		m_bGPressed = false;
	}

	if (InputManager::Get()->IsKeyPressed(DIK_P))
	{
		m_pCamera->TraverseRoute();
	}

	if (InputManager::Get()->IsKeyPressed(DIK_U))
	{
		m_VoxelisedScene.UnmapAllTiles(m_pD3D->GetDeviceContext());
	}

	m_pCamera->Update();

	LightManager::Get()->Update(m_pD3D->GetDeviceContext());

	//Render the scene
	if (!Render())
	{
		VS_LOG_VERBOSE("Render function failed")
		return false;
	}
	return true;
}

void Renderer::SetGITypeString()
{
	m_sGITypeRendered = "Render Type: ";
	switch (m_eGITypeToRender)
	{
	case giNone:
		m_sGITypeRendered += "Direct Light Only";
		break;
	case giFull:
		m_sGITypeRendered += "Full Global Illumination";
		break;
	case giDiff:
		m_sGITypeRendered += "Diffuse Indirect Light Only";
		break;
	case giAO:
		m_sGITypeRendered += "Ambient Occlusion Factor Only";
		break;
	case giSpec:
		m_sGITypeRendered += "Specular Indirect Light Only";
		break;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Renderer::Render()
{
	m_dCPUFrameEndTime = Timer::Get()->GetCurrentTime();
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
	m_pCamera->GetBaseViewMatrix(mBaseView);
	m_pD3D->TurnOffAlphaBlending();
	
	double dTileUpdateTime = 0;
	bool bUpdateVoxelVolume(m_eGITypeToRender != GIRenderFlag::giNone);
	m_pD3D->TurnZBufferOff();
	
	//Clear the voxel volume..
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psVoxeliseClear);
	if (bUpdateVoxelVolume)
	{
		m_VoxelisedScene.RenderClearVoxelsPass(pContext);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psVoxeliseClear);

	//Render the scene to the volume..
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psVoxelisePass);
	if (bUpdateVoxelVolume)
	{
		m_VoxelisedScene.RenderMesh(pContext, mWorld, mBaseView, mProjection, m_pCamera->GetPosition(), m_pModel);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psVoxelisePass);

	dTileUpdateTime = Timer::Get()->GetCurrentTime();
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psTileUpdate);
	if (bUpdateVoxelVolume)
	{
		m_VoxelisedScene.Update(pContext);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psTileUpdate);
	dTileUpdateTime = (Timer::Get()->GetCurrentTime() - dTileUpdateTime) * 1000;

	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psInjectRadiance);
	if (bUpdateVoxelVolume)
	{
		m_VoxelisedScene.RenderInjectRadiancePass(pContext);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psInjectRadiance);
		
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psGenerateMips);
	if (bUpdateVoxelVolume)
	{
		m_VoxelisedScene.GenerateMips(pContext);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psGenerateMips);
	
	m_pD3D->TurnZBufferOn();
	//Render the model to the deferred buffers
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psRenderToBuffer);
	m_DeferredRender.SetRenderTargets(pContext);
	m_DeferredRender.ClearRenderTargets(pContext, 0.f, 0.f, 0.f, 1.f);
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

	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psLightingPass);
	m_pD3D->SetRenderOutputToScreen();
	m_pD3D->TurnZBufferOff();
	m_pFullScreenWindow->Render(m_pD3D->GetDeviceContext());
	//m_DebugRenderTexture.RenderTexture(pContext, m_pFullScreenWindow->GetIndexCount(), mWorld, mBaseView, mOrtho, m_pCamera->GetPosition(), m_DeferredRender.GetTexture(btNormals));
	m_DeferredRender.RenderLightingPass(pContext, m_pFullScreenWindow->GetIndexCount(), mWorld, mBaseView, mOrtho, m_pCamera->GetPosition(), &m_VoxelisedScene, m_eGITypeToRender);
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psLightingPass);
	
	//Go back to 3D rendering
	m_pD3D->TurnZBufferOn();
	m_pD3D->TurnOnAlphaBlending();

	if (m_bDebugRenderVoxels)
	{
		m_VoxelisedScene.RenderDebugCubes(pContext, mWorld, mView, mProjection, m_pCamera);
	}

	stringstream ssCameraPosition;
	XMFLOAT3 vCamPos = m_pCamera->GetPosition();
	ssCameraPosition << "Camera Position: X: " << vCamPos.x << " Y: " << vCamPos.y << " Z: " << vCamPos.z;

	DebugLog::Get()->OutputString(ssCameraPosition.str());
	
	GPUProfiler::Get()->EndFrame(pContext);
	GPUProfiler::Get()->DisplayTimes(pContext, static_cast<float>(dCPUFrameTime), static_cast<float>(dTileUpdateTime), m_pCamera->IsFollowingDebugRoute());
	
	if (m_pCamera->FinishedRouteThisFrame())
	{
		GPUProfiler::Get()->OutputStoredTimesToFile();
	}

	DebugLog::Get()->OutputString(m_sGITypeRendered);
	DebugLog::Get()->PrintLogToScreen(pContext);
	
	//Present the rendered scene to the screen
	m_pD3D->EndScene();
	
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////