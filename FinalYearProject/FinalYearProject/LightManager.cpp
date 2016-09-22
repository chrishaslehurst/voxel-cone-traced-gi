#include "LightManager.h"
#include "Debugging.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

LightManager* LightManager::s_pTheInstance = nullptr;

bool LightManager::Initialise(ID3D11Device3* pDevice)
{
	D3D11_BUFFER_DESC lightingBufferDesc;
	lightingBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	lightingBufferDesc.ByteWidth = sizeof(LightBuffer);
	lightingBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	lightingBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	lightingBufferDesc.MiscFlags = 0;
	lightingBufferDesc.StructureByteStride = 0;

	//Create the light buffer so we can access it
	HRESULT res = pDevice->CreateBuffer(&lightingBufferDesc, nullptr, &m_pLightingBuffer);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to create lighting buffer");
		return false;
	}
}

bool LightManager::Update(ID3D11DeviceContext3* pContext)
{

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT res = pContext->Map(m_pLightingBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to lock the lighting buffer");
		return false;
	}
	LightBuffer* pLightData;
	pLightData = (LightBuffer*)mappedResource.pData;

	pLightData->AmbientColour = GetAmbientColour();
	pLightData->DirectionalLightDirection = GetDirectionalLightDirection();
	pLightData->DirectionalLightColour = GetDirectionalLightColour();
	for (int i = 0; i < m_arrPointLights.size(); i++)
	{
		PointLight* pLight = &m_arrPointLights[i];
		if (pLight)
		{
			pLightData->pointLights[i].vDiffuseColour = pLight->GetDiffuseColour();
			pLightData->pointLights[i].fRange = pLight->GetReciprocalRange();
			pLightData->pointLights[i].vPosition = XMFLOAT3(pLight->GetPosition().x, pLight->GetPosition().y, pLight->GetPosition().z);
		}
	}
	
	pContext->Unmap(m_pLightingBuffer, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LightManager::SetDirectionalLightDirection(const XMFLOAT3& vDir)
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
		VS_LOG_VERBOSE("Light index out of range!");
		return nullptr;
	}
}

void LightManager::Shutdown()
{
	delete s_pTheInstance;
	s_pTheInstance = nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

LightManager::LightManager()
	: m_iNumPointLightsAllocated(0)
	, m_pDirectionalLight(nullptr)
	, m_pLightingBuffer(nullptr)
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
	if (m_pLightingBuffer)
	{
		m_pLightingBuffer->Release();
		m_pLightingBuffer = nullptr;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

