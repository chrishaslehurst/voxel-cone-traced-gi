#ifndef LIGHT_MANAGER_H
#define LIGHT_MANAGER_H

#include "PointLight.h"
#include "DirectionalLight.h"
#include <vector>
#include <d3d11_3.h>
#include <DirectXMath.h>
#include <D3DCompiler.h>

using namespace DirectX;

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

	__declspec(align(16)) struct PointLightPixelStruct
	{
		XMFLOAT4 vDiffuseColour;
		float	 fRange;
		XMFLOAT3 vPosition;

		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_mm_free(p);
		}
	};

	__declspec(align(16)) struct LightBuffer
	{
		void* operator new(size_t i)
		{
			return _mm_malloc(i, 16);
		}

			void operator delete(void* p)
		{
			_mm_free(p);
		}

		XMFLOAT4 AmbientColour;
		XMFLOAT4 DirectionalLightColour;
		XMFLOAT3 DirectionalLightDirection;
		PointLightPixelStruct pointLights[NUM_LIGHTS];
	};

	bool Initialise(ID3D11Device3* pDevice);
	bool Update(ID3D11DeviceContext3* pContext);
	void SetDirectionalLightDirection(const XMFLOAT3& vDir);
	void SetDirectionalLightColour(const XMFLOAT4& vCol);
	const XMFLOAT3& GetDirectionalLightDirection() { return m_pDirectionalLight->GetDirection(); }
	const XMFLOAT4& GetDirectionalLightColour() { return m_pDirectionalLight->GetDiffuseColour(); }

	void AddDirectionalLight();

	void SetPointLightRange(int iIndex, float fRange);
	void SetPointLightColour(int iIndex, const XMFLOAT4& vCol);
	void SetPointLightPosition(int iIndex, const XMFLOAT3& vPos);

	void SetAmbientLightColour(const XMFLOAT4& vAmbientCol);
	const XMFLOAT4& GetAmbientColour() { return m_vAmbientColour; }

	int AddPointLight(const XMFLOAT3& vPos, const XMFLOAT4& vCol, float fRange);

	int GetNumLightsAllocated() { return m_iNumPointLightsAllocated; }
	PointLight* GetPointLight(int iIndex);

	void Shutdown();

	ID3D11Buffer* GetLightBuffer() { return m_pLightingBuffer; };

	void ClearShadowMaps(ID3D11DeviceContext3* pContext);
	
private:
	LightManager();
	~LightManager();

	static LightManager* s_pTheInstance;

	std::vector<PointLight> m_arrPointLights;
	DirectionalLight* m_pDirectionalLight;
	XMFLOAT4 m_vAmbientColour;
	ID3D11Buffer* m_pLightingBuffer;

	int m_iNumPointLightsAllocated;
};

#endif // !LIGHT_MANAGER_H
