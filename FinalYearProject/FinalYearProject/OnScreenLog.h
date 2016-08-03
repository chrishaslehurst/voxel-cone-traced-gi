#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H




class OnScreenLog
{

public:

	static OnScreenLog* Get()
	{
		if (!s_pTheInstance)
		{
			s_pTheInstance = new OnScreenLog;
		}
		return s_pTheInstance;
	}

private:

	static OnScreenLog* s_pTheInstance;

	OnScreenLog();
	~OnScreenLog();



};

#endif
