#ifndef LIGHT_MANAGER_H
#define LIGHT_MANAGER_H

#include "PointLight.h"
#include "DirectionalLight.h"
#include <vector>

#define NUM_LIGHTS 4

class LightManager
{
public:

	static LightManager* Get()
	{
		if (!s_pTheInstance)
		{
			s_pTheInstance = new LightManager;
		}
		return s_pTheInstance;
	}

	void SetDirectionalLightDirection(const XMFLOAT4& vDir);
	void SetDirectionalLightColour(const XMFLOAT4& vCol);
	void AddDirectionalLight();

	void SetPointLightRange(int iIndex, float fRange);
	void SetPointLightColour(int iIndex, const XMFLOAT4& vCol);
	void SetPointLightPosition(int iIndex, const XMFLOAT3& vPos);

	void SetAmbientLightColour(const XMFLOAT4& vAmbientCol);
	const XMFLOAT4& GetAmbientColour() { return m_vAmbientColour; }

	int AddPointLight(const XMFLOAT3& vPos, const XMFLOAT4& vCol, float fRange);

	int GetNumLightsAllocated() { return m_iNumPointLightsAllocated; }
	PointLight* GetPointLight(int iIndex);

private:
	LightManager();
	~LightManager();

	static LightManager* s_pTheInstance;

	std::vector<PointLight> m_arrPointLights;
	DirectionalLight* m_pDirectionalLight;
	XMFLOAT4 m_vAmbientColour;

	int m_iNumPointLightsAllocated;
};

#endif // !LIGHT_MANAGER_H
