#pragma once
#ifndef PROFILER_H
#define PROFILER_H

#define CONDENSED_TIMINGS 1

#include <Windows.h>
#include <vector>
#include <tuple>
#include <iostream>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

//TIMER - logs time within a scope
class Timer
{
public:
	Timer(const char* name);
	~Timer();
	static void output_data();

private:
	//deleted operators & constructors
	Timer(const Timer&) = delete;
	Timer& operator=(const Timer&) = delete;

	unsigned m_id;
	const char* m_name;
	static int sm_funcID;

	LARGE_INTEGER m_startTime;
	LARGE_INTEGER m_stopTime;
	static LARGE_INTEGER sm_frequency;

	static std::vector<std::tuple<std::string, double, int>> sm_data;
};

//start stop macros
#define TIMER_FUNCSTART Timer __xperfstart##__COUNTER__(__FUNCTION__)	//time a function
#define TIMER_SCOPED(str) Timer __xperfstart##__COUNTER__(str)			//start timing
#define TIMER_START(str) { Timer __xperfstart##__COUNTER__(str)
#define TIMER_END }

//output data macros
#define TIMER_OUTALL Timer::output_data
#define TIMER_OUTALL_ATEXIT atexit(Timer::output_data);

#endif // !PROFILER_H


