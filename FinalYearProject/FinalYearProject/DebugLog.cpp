#include "DebugLog.h"
#include "Debugging.h"

DebugLog* DebugLog::s_pTheInstance = nullptr;

DebugLog::DebugLog()
	: m_iNumStrings(0)
{

}

DebugLog::~DebugLog()
{

}

bool DebugLog::Initialise(ID3D11Device* pDevice)
{
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
	return true;
}

void DebugLog::OutputString(std::string sOutput)
{
	if (m_iNumStrings < MAX_STRINGS)
	{
		m_arrLog[m_iNumStrings] = sOutput;
		m_iNumStrings++;
	}
	else
	{
		VS_LOG_VERBOSE("Exceeded max strings for log..");
	}
}

void DebugLog::PrintLogToScreen(ID3D11DeviceContext* pContext)
{
	float xPos = 100.f;
	float yPos = 500.f;
	float textSize = 18.f;
	UINT32 TextColour = 0xffffffff;

	for (int i = 0; i < m_iNumStrings; i++)
	{
		std::wstring wideString(m_arrLog[i].begin(), m_arrLog[i].end());
		m_pFontWrapper->DrawString(pContext, wideString.c_str(), textSize, xPos, yPos - (i*textSize + 2), TextColour, 0);
	}
	m_iNumStrings = 0;

	pContext->GSSetShader(nullptr, nullptr, 0);
}

