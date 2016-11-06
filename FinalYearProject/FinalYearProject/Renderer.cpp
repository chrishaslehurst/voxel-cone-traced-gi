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
	, m_pFullScreenWindow(nullptr)
	, m_bDebugRenderVoxels(false)
	, m_bMinusPressed(false)
	, m_bPlusPressed(false)
	, m_bVPressed(false)
	, m_bGPressed(false)
	, m_bMPressed(false)
	, m_eGITypeToRender(GIRenderFlag::giFull)
	, m_eRenderMode(RenderMode::rmTiledTexture)
	, m_iAlternateRender(0)
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

	for (int i = 0; i < ComparisonTextures::ctMax; i++)
	{
		m_arrComparisonTextures[i] = new Texture2D;
		if (FAILED(m_arrComparisonTextures[i]->Init(m_pD3D->GetDevice(), iScreenWidth, iScreenHeight, 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE)))
		{
			VS_LOG_VERBOSE("Failed to create render target textures");
			return false;
		}
	}

	//Create the mesh..
	m_arrModels.push_back(new Mesh);
	m_arrModels.push_back(new Mesh);
	if (!m_arrModels[0] || !m_arrModels[1])
	{
		VS_LOG_VERBOSE("Could not create Mesh");
		return false;
	}


	if (!m_arrModels[0]->InitialiseFromObj(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, "../Assets/Models/sponza_tri1.obj"))
	{
		VS_LOG_VERBOSE("Unable to initialise mesh");
		return false;
	}
	if (!m_arrModels[1]->InitialiseFromObj(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, "../Assets/Models/sphere_unsmoothed.obj"))
	{
		VS_LOG_VERBOSE("Unable to initialise mesh");
		return false;
	}
	m_arrModels[1]->SetMeshScale(50.f);
	m_arrModels[1]->SetMeshPosition(XMFLOAT3(-1250.f, 200.f, 0.f));
	m_arrModels[1]->AddPatrolPoint(XMFLOAT3(-1250.f, 200.f, 0.f));
	m_arrModels[1]->AddPatrolPoint(XMFLOAT3(1150.f, 200.f, 0.f));
	m_arrModels[1]->StartPatrol();
	

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

	m_pRegularVoxelisedScene = new VoxelisedScene;
	m_pRegularVoxelisedScene->Initialise(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, m_arrModels[0]->GetWholeModelAABB(), false);

	m_pTiledVoxelisedScene = new VoxelisedScene;
	m_pTiledVoxelisedScene->Initialise(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, m_arrModels[0]->GetWholeModelAABB(), true);

	GPUProfiler::Get()->Initialise(m_pD3D->GetDevice());
	DebugLog::Get()->Initialise(m_pD3D->GetDevice());

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::Shutdown()
{
	m_DeferredRender.Shutdown();

	//deallocate the model
	for (int i = 0; i < m_arrModels.size(); i++)
	{
		delete m_arrModels[i];
		m_arrModels[i] = nullptr;
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
	if (m_pRegularVoxelisedScene)
	{
		delete m_pRegularVoxelisedScene;
		m_pRegularVoxelisedScene = nullptr;
	}
	if (m_pTiledVoxelisedScene)
	{
		delete m_pTiledVoxelisedScene;
		m_pTiledVoxelisedScene = nullptr;
	}

	m_DebugRenderTexture.Shutdown();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Renderer::Update(HWND hwnd)
{
	if (InputManager::Get()->IsKeyPressed(DIK_R))
	{
		m_arrModels[0]->ReloadShaders(m_pD3D->GetDevice(), hwnd);
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
		m_pRegularVoxelisedScene->IncreaseDebugMipLevel();
		m_bPlusPressed = true;
	}
	else if (InputManager::Get()->IsKeyReleased(DIK_NUMPADPLUS))
	{
		m_bPlusPressed = false;
	}
	if (InputManager::Get()->IsKeyPressed(DIK_NUMPADMINUS) && !m_bMinusPressed)
	{
		m_pRegularVoxelisedScene->DecreaseDebugMipLevel();
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

	if (InputManager::Get()->IsKeyPressed(DIK_M) && !m_bMPressed)
	{
		m_eRenderMode = static_cast<RenderMode>((static_cast<int>(m_eRenderMode) + 1) % static_cast<int>(RenderMode::rmMax));
		SetGITypeString();
		m_bMPressed = true;
	}
	else if (InputManager::Get()->IsKeyReleased(DIK_M))
	{
		m_bMPressed = false;
	}

	if (InputManager::Get()->IsKeyPressed(DIK_U))
	{
		m_pTiledVoxelisedScene->UnmapAllTiles(m_pD3D->GetDeviceContext());
	}

	m_pCamera->Update();
	for (int i = 0; i < m_arrModels.size(); i++)
	{
		m_arrModels[i]->Update();
	}
	LightManager::Get()->Update(m_pD3D->GetDeviceContext());

	//Render the scene
	if (!Render())
	{
		VS_LOG_VERBOSE("Render function failed")
		return false;
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::SetGITypeString()
{
	m_sGITypeRendered = "Render Type: ";
	m_sGIStorageMode = "Storage Mode: ";
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

	switch (m_eRenderMode)
	{
	case rmRegularTexture:
		m_sGIStorageMode += "Regular Texture";
		break;
	case rmTiledTexture:
		m_sGIStorageMode += "Tiled Texture";
		break;
	case rmComparison:
		m_sGIStorageMode += "Comparison Mode";
		break;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Renderer::Render()
{
	switch (m_eRenderMode)
	{
	case RenderMode::rmRegularTexture:
		RenderRegular();
		break;
	case RenderMode::rmTiledTexture:
		RenderTiled();
		break;
	case RenderMode::rmComparison:
		RenderComparison();
		break;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Renderer::RenderRegular()
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
		m_pRegularVoxelisedScene->RenderClearVoxelsPass(pContext);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psVoxeliseClear);

	//Render the scene to the volume..
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psVoxelisePass);
	if (bUpdateVoxelVolume)
	{
		for (int i = 0; i < m_arrModels.size(); i++)
		{
			m_arrModels[i]->GetWorldMatrix(mWorld);
			m_pRegularVoxelisedScene->RenderMesh(pContext, mWorld, mBaseView, mProjection, m_pCamera->GetPosition(), m_arrModels[i]);
		}
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psVoxelisePass);

	dTileUpdateTime = Timer::Get()->GetCurrentTime();
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psTileUpdate);
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psTileUpdate);
	dTileUpdateTime = (Timer::Get()->GetCurrentTime() - dTileUpdateTime) * 1000;

	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psInjectRadiance);
	if (bUpdateVoxelVolume)
	{
		m_pRegularVoxelisedScene->RenderInjectRadiancePass(pContext);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psInjectRadiance);

	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psGenerateMips);
	if (bUpdateVoxelVolume)
	{
		m_pRegularVoxelisedScene->GenerateMips(pContext);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psGenerateMips);

	m_pD3D->TurnZBufferOn();
	//Render the model to the deferred buffers
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psRenderToBuffer);
	m_DeferredRender.SetRenderTargets(pContext);
	m_DeferredRender.ClearRenderTargets(pContext, 0.f, 0.f, 0.f, 1.f);
	for (int i = 0; i < m_arrModels.size(); i++)
	{
		m_arrModels[i]->GetWorldMatrix(mWorld);
		m_arrModels[i]->RenderToBuffers(pContext, mWorld, mView, mProjection, m_pCamera);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psRenderToBuffer);



	//Render the model to the shadow maps
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psShadowRender);
	m_pD3D->RenderBackFacesOn();
	LightManager::Get()->ClearShadowMaps(pContext);
	for (int i = 0; i < m_arrModels.size(); i++)
	{
		m_arrModels[i]->GetWorldMatrix(mWorld);
		m_arrModels[i]->RenderShadows(pContext, mWorld, mView, mProjection, LightManager::Get()->GetDirectionalLightDirection(), LightManager::Get()->GetDirectionalLightColour(), XMFLOAT4(0.1f, 0.1f, 0.1f, 1.f), m_pCamera->GetPosition());
	}
	m_pD3D->RenderBackFacesOff();
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psShadowRender);

	//Clear buffers to begin the scene
	m_pD3D->BeginScene(0.5f, 0.5f, 0.5f, 1.f);
	m_pD3D->GetWorldMatrix(mWorld);


	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psLightingPass);
	m_pD3D->SetRenderOutputToScreen();
	m_pD3D->TurnZBufferOff();
	m_pFullScreenWindow->Render(m_pD3D->GetDeviceContext());
	//m_DebugRenderTexture.RenderTexture(pContext, m_pFullScreenWindow->GetIndexCount(), mWorld, mBaseView, mOrtho, m_pCamera->GetPosition(), m_DeferredRender.GetTexture(btNormals));
	m_DeferredRender.RenderLightingPass(pContext, m_pFullScreenWindow->GetIndexCount(), mWorld, mBaseView, mOrtho, m_pCamera->GetPosition(), m_pRegularVoxelisedScene, m_eGITypeToRender);
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psLightingPass);

	

	//Go back to 3D rendering
	m_pD3D->TurnZBufferOn();
	m_pD3D->TurnOnAlphaBlending();
	
	if (m_bDebugRenderVoxels)
	{
		m_pRegularVoxelisedScene->RenderDebugCubes(pContext, mWorld, mView, mProjection, m_pCamera);
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

bool Renderer::RenderTiled()
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
		m_pTiledVoxelisedScene->RenderClearVoxelsPass(pContext);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psVoxeliseClear);

	//Render the scene to the volume..
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psVoxelisePass);
	if (bUpdateVoxelVolume)
	{
		for (int i = 0; i < m_arrModels.size(); i++)
		{
			m_arrModels[i]->GetWorldMatrix(mWorld);
			m_pTiledVoxelisedScene->RenderMesh(pContext, mWorld, mBaseView, mProjection, m_pCamera->GetPosition(), m_arrModels[i]);
		}
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psVoxelisePass);

	dTileUpdateTime = Timer::Get()->GetCurrentTime();
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psTileUpdate);
	if (bUpdateVoxelVolume)
	{
		m_pTiledVoxelisedScene->Update(pContext);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psTileUpdate);
	dTileUpdateTime = (Timer::Get()->GetCurrentTime() - dTileUpdateTime) * 1000;

	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psInjectRadiance);
	if (bUpdateVoxelVolume)
	{
		m_pTiledVoxelisedScene->RenderInjectRadiancePass(pContext);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psInjectRadiance);

	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psGenerateMips);
	if (bUpdateVoxelVolume)
	{
		m_pTiledVoxelisedScene->GenerateMips(pContext);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psGenerateMips);

	m_pD3D->TurnZBufferOn();
	//Render the model to the deferred buffers
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psRenderToBuffer);
	m_DeferredRender.SetRenderTargets(pContext);
	m_DeferredRender.ClearRenderTargets(pContext, 0.f, 0.f, 0.f, 1.f);
	for (int i = 0; i < m_arrModels.size(); i++)
	{
		m_arrModels[i]->GetWorldMatrix(mWorld);
		m_arrModels[i]->RenderToBuffers(pContext, mWorld, mView, mProjection, m_pCamera);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psRenderToBuffer);



	//Render the model to the shadow maps
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psShadowRender);
	m_pD3D->RenderBackFacesOn();
	LightManager::Get()->ClearShadowMaps(pContext);
	for (int i = 0; i < m_arrModels.size(); i++)
	{
		m_arrModels[i]->GetWorldMatrix(mWorld);
		m_arrModels[i]->RenderShadows(pContext, mWorld, mView, mProjection, LightManager::Get()->GetDirectionalLightDirection(), LightManager::Get()->GetDirectionalLightColour(), XMFLOAT4(0.1f, 0.1f, 0.1f, 1.f), m_pCamera->GetPosition());
	}
	m_pD3D->RenderBackFacesOff();
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psShadowRender);

	//Clear buffers to begin the scene
	m_pD3D->BeginScene(0.5f, 0.5f, 0.5f, 1.f);
	m_pD3D->GetWorldMatrix(mWorld);

	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psLightingPass);
	m_pD3D->SetRenderOutputToScreen();
	//m_pD3D->SetRenderOutputToTexture(m_arrComparisonTextures[ComparisonTextures::ctTiled]->GetRenderTargetView());
	m_pD3D->TurnZBufferOff();
	m_pFullScreenWindow->Render(m_pD3D->GetDeviceContext());
	//m_DebugRenderTexture.RenderTexture(pContext, m_pFullScreenWindow->GetIndexCount(), mWorld, mBaseView, mOrtho, m_pCamera->GetPosition(), m_DeferredRender.GetTexture(btNormals));
	m_DeferredRender.RenderLightingPass(pContext, m_pFullScreenWindow->GetIndexCount(), mWorld, mBaseView, mOrtho, m_pCamera->GetPosition(), m_pTiledVoxelisedScene, m_eGITypeToRender);
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psLightingPass);

	//Go back to 3D rendering
	m_pD3D->TurnZBufferOn();
	m_pD3D->TurnOnAlphaBlending();

	if (m_bDebugRenderVoxels)
	{
		m_pTiledVoxelisedScene->RenderDebugCubes(pContext, mWorld, mView, mProjection, m_pCamera);
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

bool Renderer::RenderComparison()
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
		m_pTiledVoxelisedScene->RenderClearVoxelsPass(pContext);
		m_pRegularVoxelisedScene->RenderClearVoxelsPass(pContext);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psVoxeliseClear);

	//Render the scene to the volume..
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psVoxelisePass);
	if (bUpdateVoxelVolume)
	{
		for (int i = 0; i < m_arrModels.size(); i++)
		{
			m_arrModels[i]->GetWorldMatrix(mWorld);
			m_pTiledVoxelisedScene->RenderMesh(pContext, mWorld, mBaseView, mProjection, m_pCamera->GetPosition(), m_arrModels[i]);
			m_pRegularVoxelisedScene->RenderMesh(pContext, mWorld, mBaseView, mProjection, m_pCamera->GetPosition(), m_arrModels[i]);
		}
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psVoxelisePass);

	dTileUpdateTime = Timer::Get()->GetCurrentTime();
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psTileUpdate);
	if (bUpdateVoxelVolume)
	{
		m_pTiledVoxelisedScene->Update(pContext);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psTileUpdate);
	dTileUpdateTime = (Timer::Get()->GetCurrentTime() - dTileUpdateTime) * 1000;

	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psInjectRadiance);
	if (bUpdateVoxelVolume)
	{
		m_pTiledVoxelisedScene->RenderInjectRadiancePass(pContext);
		m_pRegularVoxelisedScene->RenderInjectRadiancePass(pContext);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psInjectRadiance);

	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psGenerateMips);
	if (bUpdateVoxelVolume)
	{
		m_pTiledVoxelisedScene->GenerateMips(pContext);
		m_pRegularVoxelisedScene->GenerateMips(pContext);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psGenerateMips);

	m_pD3D->TurnZBufferOn();
	//Render the model to the deferred buffers
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psRenderToBuffer);
	m_DeferredRender.SetRenderTargets(pContext);
	m_DeferredRender.ClearRenderTargets(pContext, 0.f, 0.f, 0.f, 1.f);
	for (int i = 0; i < m_arrModels.size(); i++)
	{
		m_arrModels[i]->GetWorldMatrix(mWorld);
		m_arrModels[i]->RenderToBuffers(pContext, mWorld, mView, mProjection, m_pCamera);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psRenderToBuffer);



	//Render the model to the shadow maps
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psShadowRender);
	m_pD3D->RenderBackFacesOn();
	LightManager::Get()->ClearShadowMaps(pContext);
	for (int i = 0; i < m_arrModels.size(); i++)
	{
		m_arrModels[i]->GetWorldMatrix(mWorld);
		m_arrModels[i]->RenderShadows(pContext, mWorld, mView, mProjection, LightManager::Get()->GetDirectionalLightDirection(), LightManager::Get()->GetDirectionalLightColour(), XMFLOAT4(0.1f, 0.1f, 0.1f, 1.f), m_pCamera->GetPosition());
	}
	m_pD3D->RenderBackFacesOff();
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psShadowRender);

	//Clear buffers to begin the scene
	m_pD3D->BeginScene(0.5f, 0.5f, 0.5f, 1.f);
	m_pD3D->GetWorldMatrix(mWorld);

	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psLightingPass);
	
	//Render tiled to Tiled texture..
	m_pD3D->SetRenderOutputToTexture(m_arrComparisonTextures[ComparisonTextures::ctTiled]->GetRenderTargetView());
	m_pD3D->TurnZBufferOff();
	m_pFullScreenWindow->Render(m_pD3D->GetDeviceContext());
	m_DeferredRender.RenderLightingPass(pContext, m_pFullScreenWindow->GetIndexCount(), mWorld, mBaseView, mOrtho, m_pCamera->GetPosition(), m_pTiledVoxelisedScene, m_eGITypeToRender);

	//Render regular to regular texture
	m_pD3D->SetRenderOutputToTexture(m_arrComparisonTextures[ComparisonTextures::ctRegularTexture]->GetRenderTargetView());
	m_pFullScreenWindow->Render(m_pD3D->GetDeviceContext());
	m_DeferredRender.RenderLightingPass(pContext, m_pFullScreenWindow->GetIndexCount(), mWorld, mBaseView, mOrtho, m_pCamera->GetPosition(), m_pRegularVoxelisedScene, m_eGITypeToRender);

	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psLightingPass);
	m_pD3D->SetRenderOutputToScreen();
	m_iAlternateRender++;
	if (m_iAlternateRender % 2 == 0)
	{
		m_DebugRenderTexture.RenderTexture(pContext, m_pFullScreenWindow->GetIndexCount(), mWorld, mBaseView, mOrtho, m_pCamera->GetPosition(), m_arrComparisonTextures[ComparisonTextures::ctRegularTexture]);
	}
	else
	{
		m_DebugRenderTexture.RenderTexture(pContext, m_pFullScreenWindow->GetIndexCount(), mWorld, mBaseView, mOrtho, m_pCamera->GetPosition(), m_arrComparisonTextures[ComparisonTextures::ctTiled]);
	}

	//Go back to 3D rendering
	m_pD3D->TurnZBufferOn();
	m_pD3D->TurnOnAlphaBlending();

	if (m_bDebugRenderVoxels)
	{
		m_pTiledVoxelisedScene->RenderDebugCubes(pContext, mWorld, mView, mProjection, m_pCamera);
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