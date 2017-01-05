#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H


#include "../FW1FontWrapper/FW1FontWrapper.h"

#include <vector>

#define MAX_STRINGS 20

class DebugLog
{

public:

	static DebugLog* Get()
	{
		if (!s_pTheInstance)
		{
			s_pTheInstance = new DebugLog;
		}
		return s_pTheInstance;
	}

	bool Initialise(ID3D11Device* pDevice);

	void OutputString(std::string sOutput);
	void PrintLogToScreen(ID3D11DeviceContext* pContext);

	void ClearLog() { m_iNumStrings = 0; }

private:

	int m_iNumStrings;

	static DebugLog* s_pTheInstance;

	DebugLog();
	~DebugLog();

	std::string m_arrLog[MAX_STRINGS];

	IFW1Factory* m_pFW1Factory;
	IFW1FontWrapper* m_pFontWrapper;



};

#endif
