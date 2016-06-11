#ifndef TIMER_H
#define TIMER_H

#include <windows.h>

class Timer
{

public:

	static Timer* Get()
	{
		if (!s_pInstance)
		{
			s_pInstance = new Timer;
		}
		return s_pInstance;
	}

	void UpdateTimers();
	double GetFrameTime() { return m_Delta; }
	double GetCurrentTime();

private:
	
	static Timer* s_pInstance;
	
	Timer();
	~Timer();

	__int64 m_Start;
	__int64 m_Previous;
	double m_Frequency;
	double m_Delta;

	float timePassed;

};

#endif // !TIMER_H
