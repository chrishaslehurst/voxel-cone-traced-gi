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
	, m_iAlternateRender(0)
	, m_dCPUFrameStartTime(0.0)
	, m_dCPUFrameEndTime(0.0)
	, m_iElapsedFrames(0)
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

bool Renderer::Initialise(int iScreenWidth, int iScreenHeight, HWND hwnd, RenderMode eRenderMode, int iVoxelGridResolution, bool bTestMode)
{
	m_iScreenWidth = iScreenWidth;
	m_iScreenHeight = iScreenHeight;
	m_bTestMode = bTestMode;
	
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

	GPUProfiler::Get()->Initialise(m_pD3D->GetDevice());
	DebugLog::Get()->Initialise(m_pD3D->GetDevice());

	if (!bTestMode)
	{
		if (DisplayVoxelStorageMenu(eRenderMode))
		{
			//display the resolution menu..
			if (DisplayResolutionMenu(iVoxelGridResolution))
			{
				m_pD3D->BeginScene(0, 0, 0, 1);
				DebugLog::Get()->OutputString("Loading...");
				DebugLog::Get()->PrintLogToScreen(m_pD3D->GetDeviceContext());
				m_pD3D->EndScene();
			}
			else
			{
				return false;
			}

		}
		else
		{
			return false;
		}
	}

	m_eRenderMode = eRenderMode;

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
	if (m_eRenderMode == rmComparison)
	{
		for (int i = 0; i < ComparisonTextures::ctMax; i++)
		{
			m_arrComparisonTextures[i] = new Texture2D;
			if (FAILED(m_arrComparisonTextures[i]->Init(m_pD3D->GetDevice(), iScreenWidth, iScreenHeight, 1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS)))
			{
				VS_LOG_VERBOSE("Failed to create render target textures");
				return false;
			}
		}
		for (int j = 0; j < 3; j++)
		{
			m_arrComparisonResultTextures[j] = new Texture2D;
			if (FAILED(m_arrComparisonResultTextures[j]->Init(m_pD3D->GetDevice(), iScreenWidth, iScreenHeight, 1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS)))
			{
				VS_LOG_VERBOSE("Failed to create comparison result textures");
				return false;
			}
		}
		D3D11_TEXTURE2D_DESC textureDesc;
		m_arrComparisonResultTextures[0]->GetTexture()->GetDesc(&textureDesc);
		textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		textureDesc.Usage = D3D11_USAGE_STAGING;
		textureDesc.BindFlags = 0;
		m_pD3D->GetDevice()->CreateTexture2D(&textureDesc, nullptr, &m_pComparisonResultStaging);
		

		//Compile the clear voxels compute shader code
		ID3D10Blob* pImgCompBuffer;
		ID3D10Blob* pErrorMessage;
		HRESULT result = D3DCompileFromFile(L"../Assets/Shaders/ImageComparison.hlsl", NULL, nullptr, "main", "cs_5_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, &pImgCompBuffer, &pErrorMessage);
		if (FAILED(result))
		{
			if (pErrorMessage)
			{
				//If the shader failed to compile it should have written something to error message, so we output that here
				OutputShaderErrorMessage(pErrorMessage, hwnd, L"../Assets/Shaders/ImageComparison.hlsl");
			}
			else
			{
				//if it hasn't, then it couldn't find the shader file..
				MessageBox(hwnd, L"../Assets/Shaders/ImageComparison.hlsl", L"Missing Shader File", MB_OK);
			}
			return false;
		}
		result = m_pD3D->GetDevice()->CreateComputeShader(pImgCompBuffer->GetBufferPointer(), pImgCompBuffer->GetBufferSize(), nullptr, &m_pCompareImagesCompute);
		if (FAILED(result))
		{
			VS_LOG_VERBOSE("Failed to create the compute shader.");
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
	
		pLightManager->AddDirectionalLight();
		pLightManager->SetDirectionalLightColour(XMFLOAT4(1.0f, 1.0f, 1.0f, 0.0f));
		pLightManager->SetDirectionalLightDirection(XMFLOAT3(0.2f, -0.1f, 0.2f));
	}
	m_DebugRenderTexture.Initialise(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, iScreenWidth, iScreenHeight, SCREEN_DEPTH, SCREEN_NEAR);
	if (m_eRenderMode == rmComparison || m_eRenderMode == rmRegularTexture)
	{
		m_pRegularVoxelisedScene = new VoxelisedScene;
		m_pRegularVoxelisedScene->Initialise(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, m_arrModels[0]->GetWholeModelAABB(), iVoxelGridResolution, false);
	}
	if (m_eRenderMode == rmComparison || m_eRenderMode == rmTiledTexture)
	{
		m_pTiledVoxelisedScene = new VoxelisedScene;
		m_pTiledVoxelisedScene->Initialise(m_pD3D->GetDevice(), m_pD3D->GetDeviceContext(), hwnd, m_arrModels[0]->GetWholeModelAABB(), iVoxelGridResolution, true);
	}
	if (m_eRenderMode == rmNoGI)
	{
		m_eGITypeToRender = giNone;
	}
	

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
// 	if (InputManager::Get()->IsKeyPressed(DIK_R))
// 	{
// 		m_arrModels[0]->ReloadShaders(m_pD3D->GetDevice(), hwnd);
// 	}
	if (InputManager::Get()->IsKeyPressed(DIK_V) && !m_bVPressed)
	{
		m_bDebugRenderVoxels = !m_bDebugRenderVoxels;
		m_bVPressed = true;
	}
	else if (InputManager::Get()->IsKeyReleased(DIK_V))
	{
		m_bVPressed = false;
	}
// 	if (InputManager::Get()->IsKeyPressed(DIK_NUMPADPLUS) && !m_bPlusPressed)
// 	{
// 		if (m_pRegularVoxelisedScene)
// 		{
// 			m_pRegularVoxelisedScene->IncreaseDebugMipLevel();
// 		}
// 		if (m_pTiledVoxelisedScene)
// 		{
// 			m_pTiledVoxelisedScene->IncreaseDebugMipLevel();
// 		}
// 		m_bPlusPressed = true;
// 	}
// 	else if (InputManager::Get()->IsKeyReleased(DIK_NUMPADPLUS))
// 	{
// 		m_bPlusPressed = false;
// 	}
// 	if (InputManager::Get()->IsKeyPressed(DIK_NUMPADMINUS) && !m_bMinusPressed)
// 	{
// 		if (m_pRegularVoxelisedScene)
// 		{
// 			m_pRegularVoxelisedScene->DecreaseDebugMipLevel();
// 		}
// 		if (m_pTiledVoxelisedScene)
// 		{
// 			m_pTiledVoxelisedScene->DecreaseDebugMipLevel();
// 		}
// 		m_bMinusPressed = true;
// 	}
// 	else if (InputManager::Get()->IsKeyReleased(DIK_NUMPADMINUS))
// 	{
// 		m_bMinusPressed = false;
// 	}
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

// 	if (InputManager::Get()->IsKeyPressed(DIK_P))
// 	{
// 		m_pCamera->TraverseRoute();
// 	}

// 	if (InputManager::Get()->IsKeyPressed(DIK_M) && !m_bMPressed)
// 	{
// 		
// 		m_eRenderMode = static_cast<RenderMode>((static_cast<int>(m_eRenderMode) + 1) % static_cast<int>(RenderMode::rmMax));
// 		SetGITypeString();
// 		
// 		m_bMPressed = true;
// 	}
// 	else if (InputManager::Get()->IsKeyReleased(DIK_M))
// 	{
// 		m_bMPressed = false;
// 	}

// 	if (InputManager::Get()->IsKeyPressed(DIK_U))
// 	{
// 		m_pTiledVoxelisedScene->UnmapAllTiles(m_pD3D->GetDeviceContext());
// 	}

	m_pCamera->Update();
	for (int i = 0; i < m_arrModels.size(); i++)
	{
		m_arrModels[i]->Update();
	}
	LightManager::Get()->Update(m_pD3D->GetDeviceContext());

	//Render the scene
	if (!Render())
	{
		VS_LOG_VERBOSE("Render function failed or ended..")
		return false;
	}
	return true;
}

bool Renderer::DisplayVoxelStorageMenu(RenderMode& eRenderMode)
{
	while (true)
	{
		m_pD3D->BeginScene(0, 0, 0, 1);

		DebugLog::Get()->OutputString("Choose a Voxel Storage Method\n Press 1 for Regular 3D Texture\n Press 2 for Volume Tiled Resources\n Press 3 to Render in Comparison Mode\n(Comparison Mode will render both methods as alternating frames)\n\nPress Escape To Quit.");
		DebugLog::Get()->PrintLogToScreen(m_pD3D->GetDeviceContext());

		//Present the menu..
		m_pD3D->EndScene();

		InputManager* pInput = InputManager::Get();
		if (!pInput->Update())
		{
			return false;
		}
		if (pInput->IsKeyPressed(DIK_1))
		{
			while (true)
			{
				pInput->Update();
				if (pInput->IsKeyReleased(DIK_1))
				{
					eRenderMode = RenderMode::rmRegularTexture;
					return true;
				}
			}
		}
		else if (pInput->IsKeyPressed(DIK_2))
		{
			while (true)
			{
				pInput->Update();
				if (pInput->IsKeyReleased(DIK_2))
				{
					eRenderMode = RenderMode::rmTiledTexture;
					return true;
				}
			}
		}
		else if (pInput->IsKeyPressed(DIK_3))
		{
			while (true)
			{
				pInput->Update();
				if (pInput->IsKeyReleased(DIK_3))
				{
					eRenderMode = RenderMode::rmComparison;
					return true;
				}
			}
		}
		else if (pInput->IsKeyPressed(DIK_NUMPAD1))
		{
			while (true)
			{
				pInput->Update();
				if (pInput->IsKeyReleased(DIK_NUMPAD1))
				{
					eRenderMode = RenderMode::rmRegularTexture;
					return true;
				}
			}
		}
		else if (pInput->IsKeyPressed(DIK_NUMPAD2))
		{
			while (true)
			{
				pInput->Update();
				if (pInput->IsKeyReleased(DIK_NUMPAD2))
				{
					eRenderMode = RenderMode::rmTiledTexture;
					return true;
				}
			}
		}
		else if (pInput->IsKeyPressed(DIK_NUMPAD3))
		{
			while (true)
			{
				pInput->Update();
				if (pInput->IsKeyReleased(DIK_NUMPAD3))
				{
					eRenderMode = RenderMode::rmComparison;
					return true;
				}
			}
		}

		else if (pInput->IsEscapePressed())
		{
			return false;
		}
	}
	
}

bool Renderer::DisplayResolutionMenu(int& iVoxelGridResolution)
{
	InputManager* pInput = InputManager::Get();
	

	while (true)
	{
		m_pD3D->BeginScene(0, 0, 0, 1);

		DebugLog::Get()->OutputString("Choose a Resolution for the Voxelised Scene\n Press 1 for 64 x 64 x 64 (Will look overbright due to low res)\n Press 2 for 128 x 128 x 128\n Press 3 for 256 x 256 x 256 (Best Quality/Performance point)\n Press 4 for 512 x 512 x 512 (Will likely lag a little)\n\nPress Escape To Quit.");
		DebugLog::Get()->PrintLogToScreen(m_pD3D->GetDeviceContext());

		//Present the menu..
		m_pD3D->EndScene();

		
		if (!pInput->Update())
		{
			return false;
		}
		if (pInput->IsKeyPressed(DIK_1) || pInput->IsKeyPressed(DIK_NUMPAD1))
		{
			iVoxelGridResolution = 64;
			return true;
		}
		else if (pInput->IsKeyPressed(DIK_2) || pInput->IsKeyPressed(DIK_NUMPAD2))
		{
			iVoxelGridResolution = 128;
			return true;
		}
		else if (pInput->IsKeyPressed(DIK_3) || pInput->IsKeyPressed(DIK_NUMPAD3))
		{
			iVoxelGridResolution = 256;
			return true;
		}
		else if (pInput->IsKeyPressed(DIK_4) || pInput->IsKeyPressed(DIK_NUMPAD4))
		{
			iVoxelGridResolution = 512;
			return true;
		}
		else if (pInput->IsEscapePressed())
		{
			return false;
		}
	}
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
	case rmNoGI:
		m_sGIStorageMode += "No GI";
		break;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Renderer::Render()
{
	double dCPUFrameTime = (m_dCPUFrameEndTime - m_dCPUFrameStartTime) * 1000;
	m_dCPUFrameStartTime = Timer::Get()->GetCurrentTime();

	VoxelisedScene* pActiveVoxScene = nullptr;

	ID3D11DeviceContext3* pContext = m_pD3D->GetDeviceContext();
	std::string sRenderMode;
	int iMemUsage = 0;
	float imagePercentDiff = 0;
	int iResolution = 0;

	switch (m_eRenderMode)
	{
	case RenderMode::rmRegularTexture:
		RenderRegular();
		iMemUsage = m_pRegularVoxelisedScene->GetMemoryUsageInBytes();
		pActiveVoxScene = m_pRegularVoxelisedScene;
		sRenderMode = "RegularGrid";
		break;
	case RenderMode::rmTiledTexture:
		RenderTiled();
		iMemUsage = m_pTiledVoxelisedScene->GetMemoryUsageInBytes();
		pActiveVoxScene = m_pTiledVoxelisedScene;
		sRenderMode = "VolumeTiledResources";
		break;
	case RenderMode::rmComparison:
		iMemUsage = m_pTiledVoxelisedScene->GetMemoryUsageInBytes() + m_pRegularVoxelisedScene->GetMemoryUsageInBytes();
		RenderComparison(imagePercentDiff);
		pActiveVoxScene = m_pTiledVoxelisedScene;
		sRenderMode = "ComparisonMode";
		break;
	case RenderMode::rmNoGI:
		RenderRegular();
		sRenderMode = "NoGI";
		break;
	}

	if (RENDER_DEBUG)
	{
		
		stringstream ssCameraPosition;
		XMFLOAT3 vCamPos = m_pCamera->GetPosition();
		ssCameraPosition << "Camera Position: X: " << vCamPos.x << " Y: " << vCamPos.y << " Z: " << vCamPos.z;
		iMemUsage /= (1024 * 1024);
		stringstream ssMemoryUsage;
		ssMemoryUsage << "Memory Consumption in MB: " << iMemUsage;

		DebugLog::Get()->OutputString(ssMemoryUsage.str());
		//DebugLog::Get()->OutputString(ssCameraPosition.str());
		DebugLog::Get()->OutputString(m_sGIStorageMode);
	   

		m_dCPUFrameEndTime = Timer::Get()->GetCurrentTime();
		GPUProfiler::Get()->EndFrame(pContext);
		GPUProfiler::Get()->DisplayTimes(pContext, static_cast<float>(dCPUFrameTime), static_cast<float>(m_dTileUpdateTime), imagePercentDiff, m_pCamera->IsFollowingDebugRoute());
		
		if (m_pCamera->FinishedRouteThisFrame())
		{
			char sGPUName[64];
			int iGPUMemoryInMB;
			m_pD3D->GetVideoCardInfo(sGPUName, iGPUMemoryInMB);
			if(pActiveVoxScene)
			{
				iResolution = pActiveVoxScene->GetTextureDimensions();
			}
			GPUProfiler::Get()->OutputStoredTimesToFile(sGPUName, iGPUMemoryInMB, sRenderMode.c_str(), iResolution, iMemUsage);
			m_pCamera->EndRoute();
			return false;
		}

		DebugLog::Get()->OutputString(m_sGITypeRendered);
		DebugLog::Get()->PrintLogToScreen(pContext);
	}

	//Present the rendered scene to the screen
	m_pD3D->EndScene();

	//If we're in test mode, allow for stabilisation/loading before we profile..
	if (m_bTestMode && m_iElapsedFrames > 10)
	{
		m_pCamera->TraverseRoute();
		m_bTestMode = false;
	}
	m_iElapsedFrames++;
	
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Renderer::RenderRegular()
{
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

	m_dTileUpdateTime = 0;
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

	m_dTileUpdateTime = Timer::Get()->GetCurrentTime();
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psTileUpdate);
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psTileUpdate);
	m_dTileUpdateTime = (Timer::Get()->GetCurrentTime() - m_dTileUpdateTime) * 1000;

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
	

	return true;
}

bool Renderer::RenderTiled()
{
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

	m_dTileUpdateTime = 0;
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

	m_dTileUpdateTime = Timer::Get()->GetCurrentTime();
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psTileUpdate);
	if (bUpdateVoxelVolume)
	{
		m_pTiledVoxelisedScene->Update(pContext);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psTileUpdate);
	m_dTileUpdateTime = (Timer::Get()->GetCurrentTime() - m_dTileUpdateTime) * 1000;

	//Go back to 3D rendering
	m_pD3D->TurnZBufferOn();
	m_pD3D->TurnOnAlphaBlending();

	if (m_bDebugRenderVoxels)
	{
		m_pTiledVoxelisedScene->RenderDebugCubes(pContext, mWorld, mView, mProjection, m_pCamera);
	}
	
	return true;
}

bool Renderer::RenderComparison(float& imageDifferencePercent)
{
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

	m_dTileUpdateTime = 0;
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

	m_dTileUpdateTime = Timer::Get()->GetCurrentTime();
	GPUProfiler::Get()->StartTimeStamp(pContext, GPUProfiler::psTileUpdate);
	if (bUpdateVoxelVolume)
	{
		m_pTiledVoxelisedScene->Update(pContext);
	}
	GPUProfiler::Get()->EndTimeStamp(pContext, GPUProfiler::psTileUpdate);
	m_dTileUpdateTime = (Timer::Get()->GetCurrentTime() - m_dTileUpdateTime) * 1000;

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
	
	//Subtract Tiled from Regular and write the absolute value to the comparison texture..
	RunImageCompShader();
	//Render the scene with alternating frames..
	m_iAlternateRender++;
	if (m_iAlternateRender % 2 == 0)
	{
		m_DebugRenderTexture.RenderTexture(pContext, m_pFullScreenWindow->GetIndexCount(), mWorld, mBaseView, mOrtho, m_pCamera->GetPosition(), m_arrComparisonTextures[ComparisonTextures::ctRegularTexture]);
	}
	else
	{
		m_DebugRenderTexture.RenderTexture(pContext, m_pFullScreenWindow->GetIndexCount(), mWorld, mBaseView, mOrtho, m_pCamera->GetPosition(), m_arrComparisonTextures[ComparisonTextures::ctTiled]);
	}

	float fImageDifferencePercent = GetCompTexturePercentageDifference();
	imageDifferencePercent = fImageDifferencePercent;
	stringstream ssImageDiff;
	ssImageDiff << "Image Difference: " << fImageDifferencePercent << "%";

	DebugLog::Get()->OutputString(ssImageDiff.str());

	//Go back to 3D rendering
	m_pD3D->TurnZBufferOn();
	m_pD3D->TurnOnAlphaBlending();

	if (m_bDebugRenderVoxels)
	{
		m_pTiledVoxelisedScene->RenderDebugCubes(pContext, mWorld, mView, mProjection, m_pCamera);
	}

	return true;
}

void Renderer::RunImageCompShader()
{
	ID3D11UnorderedAccessView* ppUAViewNULL[1] = { nullptr };
	ID3D11ShaderResourceView* ppSRViewNULL[2] = { nullptr, nullptr };
	
	ID3D11DeviceContext3* pContext = m_pD3D->GetDeviceContext();
	pContext->CSSetShader(m_pCompareImagesCompute, nullptr, 0);
	ID3D11UnorderedAccessView* uav[1] = { m_arrComparisonResultTextures[0]->GetUAV() };
	ID3D11ShaderResourceView* srv[2] = { m_arrComparisonTextures[ComparisonTextures::ctRegularTexture]->GetShaderResourceView(), m_arrComparisonTextures[ComparisonTextures::ctTiled]->GetShaderResourceView() };
	pContext->CSSetUnorderedAccessViews(0, 1, uav, nullptr);
	pContext->CSSetShaderResources(0, 2, srv);
	
	pContext->Dispatch(64, 64, 1);

	//Reset
	pContext->CSSetShader(nullptr, nullptr, 0);
	pContext->CSSetUnorderedAccessViews(0, 1, ppUAViewNULL, nullptr);
	pContext->CSSetShaderResources(0, 2, ppSRViewNULL);
	
}

float Renderer::GetCompTexturePercentageDifference()
{
	//TODO: Triple buffer this to avoid stalls..
	ID3D11DeviceContext3* pContext = m_pD3D->GetDeviceContext();
 	pContext->CopySubresourceRegion(m_pComparisonResultStaging, 0, 0, 0, 0, m_arrComparisonResultTextures[0]->GetTexture(), 0, nullptr);
 
 	D3D11_MAPPED_SUBRESOURCE pTexture;
 	HRESULT res = pContext->Map(m_pComparisonResultStaging, 0, D3D11_MAP_READ, 0, &pTexture);
 	if (FAILED(res))
 	{
		VS_LOG_VERBOSE("Couldn't map resource..");
 		return 0.f;
 	}
	float* pTexData = static_cast<float*>(pTexture.pData);
	int index = 0;
	float totalPixelDiff = 0;
	for (int y = 0; y < m_iScreenHeight; y++)
	{
		index = y * pTexture.RowPitch / 16;
		for (int x = 0; x < m_iScreenWidth ; x++)
		{
			float texDataR = pTexData[index];
			float texDataG = pTexData[index+1];
			float texDataB = pTexData[index+2];
			float texDataA = pTexData[index+3];

			totalPixelDiff += ((texDataR + texDataG + texDataB + texDataA) / 4.f);
			index += 4;
		}
	}

	totalPixelDiff /= (float)(m_iScreenWidth * m_iScreenHeight);
	float totalPixelDiffAsPercentage = totalPixelDiff * 100;
	return totalPixelDiffAsPercentage;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Renderer::OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename)
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