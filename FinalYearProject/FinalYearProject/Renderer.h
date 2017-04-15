#ifndef RENDERER_H
#define RENDERER_H
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include "D3DWrapper.h"
#include "Camera.h"
#include "Mesh.h"
#include "DirectionalLight.h"
#include "DeferredRender.h"
#include "VoxelisedScene.h"
#include "OrthoWindow.h"
#include "RenderTextureToScreen.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Global consts..

const bool FULL_SCREEN = true;
const bool RENDER_DEBUG = false;
const bool VSYNC_ENABLED = true;
const float SCREEN_DEPTH = 3000.f;
const float SCREEN_NEAR = 0.1f;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//What method are we using to store the GI info - regular is the baseline, tiled is the experimental.
enum ComparisonTextures
{
	ctRegularTexture,
	ctTiled,
	ctMax
};

//which mode are we rendering in? Comparison mode renders both the tiled and regular texture in order to do a comparison subtraction..
enum RenderMode
{
	rmRegularTexture,
	rmTiledTexture,
	rmComparison,
	rmNoGI,
	rmMax
};



class Renderer
{
public:
	Renderer();
	~Renderer();

	bool Initialise(int iScreenWidth, int iScreenHeight, HWND hwnd, RenderMode eRenderMode, int iVoxelGridResolution, bool bTestMode);
	void Shutdown();
	bool Update(HWND hwnd);
	bool DisplayVoxelStorageMenu(RenderMode& eRenderMode);
	bool DisplayResolutionMenu(int& iVoxelGridResolution);

private:
	int m_iScreenWidth;
	int m_iScreenHeight;

	RenderMode m_eRenderMode;

	D3DWrapper* m_pD3D;
	Camera*		m_pCamera;

	std::vector<Mesh*> m_arrModels;

	DeferredRender m_DeferredRender;
	RenderTextureToScreen m_DebugRenderTexture;
	VoxelisedScene*   m_pRegularVoxelisedScene;
	VoxelisedScene*	  m_pTiledVoxelisedScene;
	OrthoWindow* m_pFullScreenWindow;

	ID3D11ComputeShader*	m_pCompareImagesCompute;

	Texture2D* m_arrComparisonTextures[ComparisonTextures::ctMax];
	//There will be readback from GPU so triple buffer it..
	Texture2D* m_arrComparisonResultTextures[3];
	ID3D11Texture2D* m_pComparisonResultStaging;
	bool m_bDebugRenderVoxels;

	bool m_bPlusPressed;
	bool m_bMinusPressed;
	bool m_bVPressed;
	bool m_bGPressed;
	bool m_bMPressed;

	bool m_bTestMode;
	int m_iElapsedFrames;
	
	GIRenderFlag m_eGITypeToRender;
	std::string m_sGITypeRendered;
	std::string m_sGIStorageMode;
	void SetGITypeString();

	bool Render();
	bool RenderRegular();
	bool RenderTiled();
	bool RenderComparison(float& imageDifferencePercent);

	void RunImageCompShader();
	float GetCompTexturePercentageDifference();
	void OutputShaderErrorMessage(ID3D10Blob* errorMessage, HWND hwnd, WCHAR* shaderFilename);

	int m_iAlternateRender;

	double m_dCPUFrameStartTime;
	double m_dCPUFrameEndTime;
	double m_dTileUpdateTime;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // !RENDERER_H
