#include "Timer.h"
#include "Debugging.h"


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Timer* Timer::s_pInstance = nullptr;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Timer::Timer()
{
	__int64 counts;
	QueryPerformanceFrequency((LARGE_INTEGER*)&counts);
	m_Frequency = 1.0 / (double)counts;


	__int64 current;
	QueryPerformanceCounter((LARGE_INTEGER*)&current);
	m_Start = current;
	m_Previous = current;
	timePassed = 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Timer::~Timer()
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double Timer::GetCurrentTime()
{
	__int64 current;
	QueryPerformanceCounter((LARGE_INTEGER*)&current);

	return (current * m_Frequency);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void Timer::UpdateTimers()
{
	__int64 current;
	QueryPerformanceCounter((LARGE_INTEGER*)&current);
	m_Delta = (current - m_Previous) * m_Frequency;


	static int frameCount = 0;
	static float elapsed = 0.0f;

	frameCount++;
	if ((((current - m_Start)* m_Frequency) - elapsed) >= 1.0f)
	{
		float fps = (float)frameCount;
		float ms = 1000.0f / fps;

		//std::wostringstream ss;
		//ss.precision(6);
		//ss << m_Name.c_str() << L"FPS: " << fps << "  ms: " << ms;
		//SetWindowText(m_hwnd, ss.str().c_str());
		VS_LOG(fps);
		VS_LOG(ms);

		frameCount = 0;
		elapsed += 1.0f;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

