#include "GPUProfiler.h"
#include "Debugging.h"

GPUProfiler* GPUProfiler::s_pTheInstance = nullptr;

GPUProfiler::GPUProfiler()
{
	
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
	res = m_pFW1Factory->CreateFontWrapper(pDevice, L"Arial", &m_pFontWrapper);
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
		pDevice->CreateQuery(&queryDescription, &m_arrProfiledSectionTimesBuffer[0][i]);
		pDevice->CreateQuery(&queryDescription, &m_arrProfiledSectionTimesBuffer[1][i]);
	}

	m_arrProfiledSectionNames[ProfiledSections::psRenderToBuffer] = "Render to Buffers";
	m_arrProfiledSectionNames[ProfiledSections::psShadowRender] = "Shadow Maps Render";
	m_arrProfiledSectionNames[ProfiledSections::psLightingPass] = "Lighting Pass";
	m_arrProfiledSectionNames[ProfiledSections::psWholeFrame] = "GPU Frame Time";

	for (int i = 0; i < 2; i++)
	{
		pDevice->CreateQuery(&queryDescription, &m_pBeginFrame[i]);
		pDevice->CreateQuery(&disjointQueryDescription, &m_pDisjointQuery[i]);
	}

	return true;
}

void GPUProfiler::BeginFrame(ID3D11DeviceContext* pContext)
{
	pContext->Begin(m_pDisjointQuery[m_iCurrentBufferIndex]);
	pContext->End(m_pBeginFrame[m_iCurrentBufferIndex]);
}

void GPUProfiler::EndFrame(ID3D11DeviceContext* pContext)
{
	pContext->End(m_arrProfiledSectionTimesBuffer[m_iCurrentBufferIndex][ProfiledSections::psWholeFrame]);
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

void GPUProfiler::StampTime(ID3D11DeviceContext* pContext, ProfiledSections eSectionID)
{
	pContext->End(m_arrProfiledSectionTimesBuffer[m_iCurrentBufferIndex][eSectionID]);
}

void GPUProfiler::DisplayTimes(ID3D11DeviceContext* pContext)
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

	//Get the timestamps..
	UINT64 tsBeginFrame, tsEndFrame;

	pContext->GetData(m_pBeginFrame[m_iCurrentBufferIndex], &tsBeginFrame, sizeof(UINT64), 0);
	pContext->GetData(m_arrProfiledSectionTimesBuffer[m_iCurrentBufferIndex][ProfiledSections::psWholeFrame], &tsEndFrame, sizeof(UINT64), 0);

	float fFrameTime = float(tsEndFrame - tsBeginFrame) / float(tsDisjoint.Frequency) * 1000.f;

	stringstream ss;
	ss << "GPU Frame Time: " << fFrameTime;

	string framestring = ss.str();
	std::wstring wideFrameString(framestring.begin(), framestring.end());

	m_pFontWrapper->DrawString(pContext,
		wideFrameString.c_str(), 20.0f,	100.0f,	50.0f,// Y position
		0xffffffff,// Text color, 0xAaBbGgRr
		0// Flags (for example FW1_RESTORESTATE to keep context states unchanged)
		);
	pContext->GSSetShader(nullptr, nullptr, 0);

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
		if (m_arrProfiledSectionTimesBuffer[0][i])
		{
			m_arrProfiledSectionTimesBuffer[0][i]->Release();
			m_arrProfiledSectionTimesBuffer[0][i] = nullptr;
		}
		if(m_arrProfiledSectionTimesBuffer[1][i])
		{
			m_arrProfiledSectionTimesBuffer[1][i]->Release();
			m_arrProfiledSectionTimesBuffer[1][i] = nullptr;
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

