#ifndef RENDERER_H
#define RENDERER_H
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include "D3DWrapper.h"
#include "Camera.h"
#include "Mesh.h"
#include "Material.h"
#include "DirectionalLight.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Global consts..

const bool FULL_SCREEN = false;
const bool VSYNC_ENABLED = true;
const float SCREEN_DEPTH = 1000.f;
const float SCREEN_NEAR = 0.1f;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class Renderer
{
public:
	Renderer();
	~Renderer();

	bool Initialise(int iScreenWidth, int iScreenHeight, HWND hwnd);
	void Shutdown();
	bool Update();

private:
	D3DWrapper* m_pD3D;
	Camera*		m_pCamera;

	//TODO: These things should probably go elsewhere long term!
	Mesh*		m_pModel;
	Material*	m_pShader;
	DirectionalLight* m_pDirectionalLight;

	bool Render();
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // !RENDERER_H
