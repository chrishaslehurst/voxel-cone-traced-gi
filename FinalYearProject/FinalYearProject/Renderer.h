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
const bool VSYNC_ENABLED = true;
const float SCREEN_DEPTH = 3000.f;
const float SCREEN_NEAR = 0.1f;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

	//TODO: Model should probably go elsewhere long term!
	Mesh*		m_pModel;
	Mesh*		m_pCube;

	DeferredRender m_DeferredRender;
	RenderTextureToScreen m_DebugRenderTexture;
	VoxelisedScene   m_VoxelisePass;
	OrthoWindow* m_pFullScreenWindow;


	bool Render();

	double m_dCPUFrameStartTime;
	double m_dCPUFrameEndTime;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // !RENDERER_H
