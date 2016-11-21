#include "GPUProfiler.h"
#include "Debugging.h"
#include <iomanip>
#include <iostream>
#include <fstream>

GPUProfiler* GPUProfiler::s_pTheInstance = nullptr;

GPUProfiler::GPUProfiler()
	: m_iNumFramesProfiled(0)
	, m_fStoredCPUAverageTime(0)
	, m_fStoredCPUMaxTime(0)
	, m_fStoredCPUMinTime(FLT_MAX)
	, m_fAverageImageDifference(0)
	, m_fMaxImageDifference(0)
	, m_fMinImageDifference(FLT_MAX)
{
	for (int i = 0; i < ProfiledSections::psMax; i++)
	{
		m_arrStoredGPUAverageTimes[i] = 0;
		m_arrStoredGPUMaxTimes[i] = 0;
		m_arrStoredGPUMinTimes[i] = FLT_MAX;
	}
}

GPUProfiler::~GPUProfiler()
{
	
}

bool GPUProfiler::Initialise(ID3D11Device* pDevice)
{
	m_iCurrentBufferIndex = 0;

	HRESULT res = FW1CreateFactory(FW1_VERSION, &m_pFW1Factory);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to create font factory for FW1");
		return false;
	}
	res = m_pFW1Factory->CreateFontWrapper(pDevice, L"Consolas", &m_pFontWrapper);
	if (FAILED(res))
	{
		VS_LOG_VERBOSE("Failed to create font wrapper for FW1");
		return false;
	}

	D3D11_QUERY_DESC queryDescription;
	ZeroMemory(&queryDescription, sizeof(queryDescription));
	queryDescription.Query = D3D11_QUERY_TIMESTAMP;

	D3D11_QUERY_DESC disjointQueryDescription;
	ZeroMemory(&disjointQueryDescription, sizeof(disjointQueryDescription));
	disjointQueryDescription.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;

	for (int i = 0; i < ProfiledSections::psMax; i++)
	{
		pDevice->CreateQuery(&queryDescription, &m_arrProfiledSectionStartTimesBuffer[0][i]);
		pDevice->CreateQuery(&queryDescription, &m_arrProfiledSectionStartTimesBuffer[1][i]);

		pDevice->CreateQuery(&queryDescription, &m_arrProfiledSectionEndTimesBuffer[0][i]);
		pDevice->CreateQuery(&queryDescription, &m_arrProfiledSectionEndTimesBuffer[1][i]);
	}

	m_arrProfiledSectionNames[ProfiledSections::psRenderToBuffer] = "Render to Buffers:  ";
	m_arrProfiledSectionNames[ProfiledSections::psShadowRender] =	"Shadow Maps Render: ";
	m_arrProfiledSectionNames[ProfiledSections::psLightingPass] =	"Lighting Pass:      ";
	m_arrProfiledSectionNames[ProfiledSections::psVoxeliseClear] =	"Voxelise Clear:     ";
	m_arrProfiledSectionNames[ProfiledSections::psVoxelisePass] =	"Voxelise Pass:      ";
	m_arrProfiledSectionNames[ProfiledSections::psInjectRadiance] =	"Inject Radiance:    ";
	m_arrProfiledSectionNames[ProfiledSections::psTileUpdate] =		"GPU Tile Update:    ";
	m_arrProfiledSectionNames[ProfiledSections::psGenerateMips] =	"Generate Mips:      ";
	m_arrProfiledSectionNames[ProfiledSections::psWholeFrame] =		"GPU Frame Time:     ";

	for (int i = 0; i < 2; i++)
	{
		pDevice->CreateQuery(&disjointQueryDescription, &m_pDisjointQuery[i]);
	}

	return true;
}

void GPUProfiler::BeginFrame(ID3D11DeviceContext* pContext)
{
	pContext->Begin(m_pDisjointQuery[m_iCurrentBufferIndex]);
	pContext->End(m_arrProfiledSectionStartTimesBuffer[m_iCurrentBufferIndex][ProfiledSections::psWholeFrame]);
}

void GPUProfiler::EndFrame(ID3D11DeviceContext* pContext)
{
	pContext->End(m_arrProfiledSectionEndTimesBuffer[m_iCurrentBufferIndex][ProfiledSections::psWholeFrame]);
	pContext->End(m_pDisjointQuery[m_iCurrentBufferIndex]);

	if (m_iCurrentBufferIndex == 1)
	{
		m_iCurrentBufferIndex = 0;
	}
 	else
 	{
 		m_iCurrentBufferIndex = 1;
 	}
}

void GPUProfiler::StartTimeStamp(ID3D11DeviceContext* pContext, ProfiledSections eSectionID)
{
	pContext->End(m_arrProfiledSectionStartTimesBuffer[m_iCurrentBufferIndex][eSectionID]);
}

void GPUProfiler::EndTimeStamp(ID3D11DeviceContext* pContext, ProfiledSections eSectionID)
{
	pContext->End(m_arrProfiledSectionEndTimesBuffer[m_iCurrentBufferIndex][eSectionID]);
}

void GPUProfiler::DisplayTimes(ID3D11DeviceContext* pContext, float CPUFrameTime, float CPUTileUpdateTime, float fImageDifferencePercentage, bool bProfilingRun)
{
	while (pContext->GetData(m_pDisjointQuery[m_iCurrentBufferIndex], nullptr, 0, 0) == S_FALSE)
	{
		Sleep(1); //wait for the data to be ready..
	}

	D3D10_QUERY_DATA_TIMESTAMP_DISJOINT tsDisjoint;
	pContext->GetData(m_pDisjointQuery[m_iCurrentBufferIndex], &tsDisjoint, sizeof(tsDisjoint), 0);
	if (tsDisjoint.Disjoint)
	{
		return; //Data was disjoint so discard it..
	}

	
	float xPos = 100.f;
	float yPos = 50.f;
	float textSize = 20.f;
	UINT32 TextColour = 0xffffffff;
	
	stringstream CPUss; 
	CPUss << std::fixed << std::setprecision(2) << "CPU Frame Time:     " << CPUFrameTime << "ms";
	string sCPUString = CPUss.str();
	std::wstring wideCPUFrameString(sCPUString.begin(), sCPUString.end());
	if (bProfilingRun)
	{
		m_fStoredCPUAverageTime += CPUFrameTime;
		m_fAverageImageDifference += fImageDifferencePercentage;
		if (CPUFrameTime > m_fStoredCPUMaxTime)
		{
			m_fStoredCPUMaxTime = CPUFrameTime;
		}
		if (CPUFrameTime < m_fStoredCPUMinTime)
		{
			m_fStoredCPUMinTime = CPUFrameTime;
		}

		if (fImageDifferencePercentage > m_fMaxImageDifference)
		{
			m_fMaxImageDifference = fImageDifferencePercentage;
		}
		if (fImageDifferencePercentage < m_fMinImageDifference)
		{
			m_fMinImageDifference = fImageDifferencePercentage;
		}

		m_iNumFramesProfiled++;
	}
	m_pFontWrapper->DrawString(pContext, wideCPUFrameString.c_str(), textSize, xPos, yPos, TextColour, 0);
	yPos += textSize;

	stringstream TileUpdateSs;
	TileUpdateSs << std::fixed << std::setprecision(2) << "CPU Tile Update Time:" << CPUTileUpdateTime << "ms";
	string sTileUpdateString = TileUpdateSs.str();
	std::wstring wideTileUpdateFrameString(sTileUpdateString.begin(), sTileUpdateString.end());
	m_pFontWrapper->DrawString(pContext, wideTileUpdateFrameString.c_str(), textSize, xPos, yPos, TextColour, 0);

	yPos += textSize;

	for (int i = 0; i < ProfiledSections::psMax; i++)
	{
		//Get the timestamps..
		UINT64 tsBegin, tsEnd;

		pContext->GetData(m_arrProfiledSectionStartTimesBuffer[m_iCurrentBufferIndex][i], &tsBegin, sizeof(UINT64), 0);
		pContext->GetData(m_arrProfiledSectionEndTimesBuffer[m_iCurrentBufferIndex][i], &tsEnd, sizeof(UINT64), 0);
		float fFrameTime = float(tsEnd - tsBegin) / float(tsDisjoint.Frequency) * 1000.f;

		stringstream GPUss;
		GPUss << std::fixed << std::setprecision(2) << m_arrProfiledSectionNames[i] << fFrameTime << "ms";
		string outputString = GPUss.str();
		std::wstring outputWideString(outputString.begin(), outputString.end());
		if (bProfilingRun)
		{
			m_arrStoredGPUAverageTimes[i] += fFrameTime;
			if (fFrameTime > m_arrStoredGPUMaxTimes[i])
			{
				m_arrStoredGPUMaxTimes[i] = fFrameTime;
			}
			if (fFrameTime < m_arrStoredGPUMinTimes[i])
			{
				m_arrStoredGPUMinTimes[i] = fFrameTime;
			}
		}
		m_pFontWrapper->DrawString(pContext, outputWideString.c_str(), textSize, xPos, yPos + (i * textSize), TextColour, 0);
	}
	
	pContext->GSSetShader(nullptr, nullptr, 0);

}

void GPUProfiler::OutputStoredTimesToFile(const char* gpuName, int gpuMemInMB, const char* voxelStorageType, int iResolution, int MemUsage)
{
	//Get the averages as up to now just accumulated values..
	m_fStoredCPUAverageTime /= static_cast<float>(m_iNumFramesProfiled);
	m_fAverageImageDifference /= static_cast<float>(m_iNumFramesProfiled);

	std::stringstream ss;
	ss << "../Results/" << gpuName << "_" << gpuMemInMB << "MB_" << voxelStorageType << "_" << iResolution << ".csv";

	std::ofstream outfile;
	outfile.open(ss.str().c_str());
	outfile << std::fixed << "Profiled Section, Average, Minimum, Maximum\n";
	outfile << "CPU Frame Time," << m_fStoredCPUAverageTime << "," << m_fStoredCPUMinTime << "," << m_fStoredCPUMaxTime << "\n";
	for (int i = 0; i < ProfiledSections::psMax; i++)
	{
		m_arrStoredGPUAverageTimes[i] /= m_iNumFramesProfiled;
		outfile << m_arrProfiledSectionNames[i] << "," << m_arrStoredGPUAverageTimes[i] << "," << m_arrStoredGPUMinTimes[i] << "," << m_arrStoredGPUMaxTimes[i] << "\n";
	}
	outfile << "Image Comparison Difference," << m_fAverageImageDifference << "," << m_fMinImageDifference << "," << m_fMaxImageDifference << "\n";
	outfile << "\nMemory Usage(MB):," << MemUsage;
	
	outfile.close();
	//Reset Times Stored
	m_fStoredCPUAverageTime = 0;
	m_fStoredCPUMaxTime = 0;
	m_fStoredCPUMinTime = FLT_MAX;

	m_fAverageImageDifference = 0;
	m_fMaxImageDifference = 0;
	m_fMinImageDifference = FLT_MAX;

	for (int i = 0; i < ProfiledSections::psMax; i++)
	{
		m_arrStoredGPUMinTimes[i] = FLT_MAX;
		m_arrStoredGPUMaxTimes[i] = 0;
		m_arrStoredGPUAverageTimes[i] = 0;
	}
	m_iNumFramesProfiled = 0;
}

void GPUProfiler::Shutdown()
{
	if (m_pFontWrapper)
	{
		m_pFontWrapper->Release();
		m_pFontWrapper = nullptr;
	}

	if (m_pFW1Factory)
	{
		m_pFW1Factory->Release();
		m_pFW1Factory = nullptr;
	}

	for (int i = 0; i < ProfiledSections::psMax; i++)
	{
		if (m_arrProfiledSectionStartTimesBuffer[0][i])
		{
			m_arrProfiledSectionStartTimesBuffer[0][i]->Release();
			m_arrProfiledSectionStartTimesBuffer[0][i] = nullptr;
		}
		if(m_arrProfiledSectionStartTimesBuffer[1][i])
		{
			m_arrProfiledSectionStartTimesBuffer[1][i]->Release();
			m_arrProfiledSectionStartTimesBuffer[1][i] = nullptr;
		}
	}
	for (int i = 0; i < 2; i++)
	{
		if (m_pDisjointQuery[i])
		{
			m_pDisjointQuery[i]->Release();
			m_pDisjointQuery[i] = nullptr;
		}
		if (m_pBeginFrame[i])
		{
			m_pBeginFrame[i]->Release();
			m_pBeginFrame[i] = nullptr;
		}
	}
}

