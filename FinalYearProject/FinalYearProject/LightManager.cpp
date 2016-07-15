#include "LightManager.h"
#include "Debugging.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

LightManager* LightManager::s_pTheInstance = nullptr;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LightManager::SetDirectionalLightDirection(const XMFLOAT4& vDir)
{
	if (m_pDirectionalLight)
	{
		m_pDirectionalLight->SetDirection(vDir.x, vDir.y, vDir.z);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LightManager::SetDirectionalLightColour(const XMFLOAT4& vCol)
{
	if (m_pDirectionalLight)
	{
		m_pDirectionalLight->SetDiffuseColour(vCol.x, vCol.y, vCol.z, vCol.w);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LightManager::AddDirectionalLight()
{
	if (!m_pDirectionalLight)
	{
		m_pDirectionalLight = new DirectionalLight;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LightManager::SetPointLightRange(int iIndex, float fRange)
{
	if (iIndex < m_iNumPointLightsAllocated)
	{
		m_arrPointLights[iIndex].SetRange(fRange);
	}
	else
	{
		VS_LOG_VERBOSE("Light index out of range!");
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LightManager::SetPointLightColour(int iIndex, const XMFLOAT4& vCol)
{
	if (iIndex < m_iNumPointLightsAllocated)
	{
		m_arrPointLights[iIndex].SetDiffuseColour(vCol.x, vCol.y, vCol.z, vCol.w);
	}
	else
	{
		VS_LOG_VERBOSE("Light index out of range!");
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LightManager::SetPointLightPosition(int iIndex, const XMFLOAT3& vPos)
{
	if (iIndex < m_iNumPointLightsAllocated)
	{
		m_arrPointLights[iIndex].SetPosition(vPos.x, vPos.y, vPos.z);
	}
	else
	{
		VS_LOG_VERBOSE("Light index out of range!");
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LightManager::SetAmbientLightColour(const XMFLOAT4& vAmbientCol)
{
	m_vAmbientColour = vAmbientCol;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int LightManager::AddPointLight(const XMFLOAT3& vPos, const XMFLOAT4& vCol, float fRange)
{
	if (m_iNumPointLightsAllocated < NUM_LIGHTS)
	{
		m_arrPointLights[m_iNumPointLightsAllocated].SetPosition(vPos.x, vPos.y, vPos.z);
		m_arrPointLights[m_iNumPointLightsAllocated].SetRange(fRange);
		m_arrPointLights[m_iNumPointLightsAllocated].SetDiffuseColour(vCol.x, vCol.y, vCol.z, vCol.w);

		m_iNumPointLightsAllocated++;
	}
	else
	{
		VS_LOG_VERBOSE("Max lights allocated..");
	}

	return m_iNumPointLightsAllocated - 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

PointLight* LightManager::GetPointLight(int iIndex)
{
	if (iIndex < m_arrPointLights.size())
	{
		return &m_arrPointLights[iIndex];
	}
	else
	{
		VS_LOG_VERBOSE("Light index out of range!")
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

LightManager::LightManager()
	: m_iNumPointLightsAllocated(0)
	, m_pDirectionalLight(nullptr)
{
	for (int i = 0; i < NUM_LIGHTS; i++)
	{
		m_arrPointLights.push_back(PointLight());
		m_arrPointLights[i].SetPosition(0.f, 0.f, 0.f);
		m_arrPointLights[i].SetRange(0.f);
		m_arrPointLights[i].SetDiffuseColour(0.f, 0.f, 0.f, 0.f);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

LightManager::~LightManager()
{
	if (m_pDirectionalLight)
	{
		delete m_pDirectionalLight;
		m_pDirectionalLight = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

