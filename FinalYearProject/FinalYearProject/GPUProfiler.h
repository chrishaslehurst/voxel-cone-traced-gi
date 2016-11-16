#ifndef GPU_PROFILER_H
#define GPU_PROFILER_H

#include <d3d11_3.h>
#include <string>
#include "../FW1FontWrapper/FW1FontWrapper.h"
#include <vector>

using namespace std;

class GPUProfiler
{

public:
	enum ProfiledSections
	{
		psWholeFrame,
		psRenderToBuffer,
		psShadowRender,
		psLightingPass,
		psVoxeliseClear,
		psVoxelisePass,
		psInjectRadiance,
		psTileUpdate,
		psGenerateMips,
		psMax
	};

	static GPUProfiler* Get()
	{
		if (!s_pTheInstance)
		{
			s_pTheInstance = new GPUProfiler;
		}
		return s_pTheInstance;
	}

	bool Initialise(ID3D11Device* pDevice);
	void BeginFrame(ID3D11DeviceContext* pContext);
	void EndFrame(ID3D11DeviceContext* pContext);

	void StartTimeStamp(ID3D11DeviceContext* pContext, ProfiledSections eSectionID);
	void EndTimeStamp(ID3D11DeviceContext* pContext, ProfiledSections eSectionID);

	void DisplayTimes(ID3D11DeviceContext* pContext, float CPUFrameTime, float CPUTileUpdateTime, bool bProfilingRun);
	void OutputStoredTimesToFile(const char* gpuName, int gpuMemInMB, const char* voxelStorageType, int iResolution, int MemUsage);

	void Shutdown();
private:

	int m_iCurrentBufferIndex;

	static GPUProfiler* s_pTheInstance;

	string m_arrProfiledSectionNames[ProfiledSections::psMax];
	ID3D11Query* m_arrProfiledSectionStartTimesBuffer[2][ProfiledSections::psMax];
	ID3D11Query* m_arrProfiledSectionEndTimesBuffer[2][ProfiledSections::psMax];

	int m_iNumFramesProfiled;
	float m_arrStoredGPUAverageTimes[ProfiledSections::psMax];
	float m_arrStoredGPUMaxTimes[ProfiledSections::psMax];
	float m_arrStoredGPUMinTimes[ProfiledSections::psMax];
	float m_fStoredCPUAverageTime;
	float m_fStoredCPUMaxTime;
	float m_fStoredCPUMinTime;

	ID3D11Query* m_pBeginFrame[2];
	ID3D11Query* m_pDisjointQuery[2];

	IFW1Factory* m_pFW1Factory;
	IFW1FontWrapper* m_pFontWrapper;

	GPUProfiler();
	~GPUProfiler();
};

#endif