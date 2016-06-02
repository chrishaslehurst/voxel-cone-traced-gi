//////////////////////////////////////////////////////
//	Filename: Debugging.h
//	Created by: Chris Haslehurst
//	Date: 16/05/2015
//
//////////////////////////////////////////////////////


/*
Some useful debugging tools/macros.
**/

#include <Windows.h>
#include <iostream>
#include <sstream>
#include <cassert>


//Log your message along with the filename and the line number on which this log was called
#define VS_LOG_VERBOSE( vs )													\
{																				\
	std::wostringstream vos_;													\
	vos_ << vs << " on line: " << __LINE__ << " in file: " << __FILE__ << "\n";	\
	OutputDebugStringW( vos_.str().c_str() );									\
}	


//Log just your message to the the visual studio log
#define VS_LOG( s )																\
{																				\
	std::wostringstream os_;													\
	os_ << s << "\n";															\
	OutputDebugStringW(os_.str().c_str());										\
}

//an assert that will allow for execution to continue afterwards, can be used as a conditional breakpoint
#define check(expr, msg )														\
{																				\
	if(!(expr))																	\
	{																			\
	std::wostringstream vos_;													\
	vos_ << msg << " on line: " << __LINE__ << " in file: " << __FILE__ << "\n";\
	OutputDebugStringW( vos_.str().c_str() );									\
	__debugbreak();																\
	}																			\
}

//Basically an assert but can send your own message through to the log
#define ensure(expr, msg )														\
{																				\
	if(!(expr))																	\
	{																			\
	std::wostringstream vos_;													\
	vos_ << msg << " on line: " << __LINE__ << " in file: " << __FILE__ << "\n";\
	OutputDebugStringW( vos_.str().c_str() );									\
	__debugbreak();																\
	abort();																	\
	}																			\
}