#ifndef RENDERER_H
#define RENDERER_H
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include "D3DWrapper.h"
#include "Camera.h"
#include "Mesh.h"
#include "DirectionalLight.h"
#include "DeferredRender.h"
#include "OrthoWindow.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Global consts..

const bool FULL_SCREEN = true;
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

	DeferredRender m_DeferredRender;
	OrthoWindow* m_pFullScreenWindow;

	bool Render();
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // !RENDERER_H
