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

const bool FULL_SCREEN = false;
const bool RENDER_DEBUG = true;
const bool VSYNC_ENABLED = true;
const float SCREEN_DEPTH = 3000.f;
const float SCREEN_NEAR = 0.1f;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
	rmMax
};

const RenderMode k_eRenderMode = rmTiledTexture;

class Renderer
{
public:
	Renderer();
	~Renderer();

	bool Initialise(int iScreenWidth, int iScreenHeight, HWND hwnd);
	void Shutdown();
	bool Update(HWND hwnd);

private:
	D3DWrapper* m_pD3D;
	Camera*		m_pCamera;

	std::vector<Mesh*> m_arrModels;

	DeferredRender m_DeferredRender;
	RenderTextureToScreen m_DebugRenderTexture;
	VoxelisedScene*   m_pRegularVoxelisedScene;
	VoxelisedScene*	  m_pTiledVoxelisedScene;
	OrthoWindow* m_pFullScreenWindow;

	Texture2D* m_arrComparisonTextures[ComparisonTextures::ctMax];
	bool m_bDebugRenderVoxels;

	bool m_bPlusPressed;
	bool m_bMinusPressed;
	bool m_bVPressed;
	bool m_bGPressed;
	bool m_bMPressed;

	RenderMode m_eRenderMode;

	GIRenderFlag m_eGITypeToRender;
	std::string m_sGITypeRendered;
	std::string m_sGIStorageMode;
	void SetGITypeString();

	bool Render();
	bool RenderRegular();
	bool RenderTiled();
	bool RenderComparison();

	int m_iAlternateRender;

	double m_dCPUFrameStartTime;
	double m_dCPUFrameEndTime;
	double m_dTileUpdateTime;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // !RENDERER_H
