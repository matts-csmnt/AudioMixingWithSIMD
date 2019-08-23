#include "Profiler.h"

//TIMER
LARGE_INTEGER Timer::sm_frequency = { 0, 0 };
std::vector<std::tuple<std::string, double, int>> Timer::sm_data;
int Timer::sm_funcID = 0;

Timer::Timer(const char* name)
	: m_name(name)
{
	//skip getting freq data if we have it
	if(sm_frequency.QuadPart == 0)
		QueryPerformanceFrequency(&sm_frequency);	
	
	QueryPerformanceCounter(&m_startTime);
	
	//init the data entry and get an id for what we run
	m_id = sm_data.size();
	sm_data.push_back(std::make_tuple(m_name, 0.0, sm_funcID));
	++sm_funcID;
}

Timer::~Timer()
{
	LARGE_INTEGER liET;
	double dET;
	//calculate elapsed time microseconds
	QueryPerformanceCounter(&m_stopTime);
	liET.QuadPart = m_stopTime.QuadPart - m_startTime.QuadPart;

	dET = (double)liET.QuadPart / sm_frequency.QuadPart;

	//add to results
	std::get<1>(sm_data[m_id]) = dET;

	--sm_funcID;
}

void Timer::output_data()
{
	std::vector<std::tuple<int, double, int>> averages;
	int found = -1;

	printf("\nLOGGING DATA TO FILE, PLEASE WAIT...\n");

	for (const auto& d : sm_data)
	{
		//average for one iteration of scope
		//find id in averages vec
		std::for_each(averages.begin(), averages.end(),
			[&found, &d](std::tuple<int, double, int> t) {
				if(std::get<0>(t) == std::get<2>(d))
					found = std::get<0>(t); });

		if (found < 0)
		{
			//insert to vector
			averages.push_back(std::make_tuple(std::get<2>(d), std::get<1>(d), 1));
		}
		else
		{
			//increment average (sum and divide)
			++std::get<2>(averages.at(found));
			std::get<1>(averages.at(found)) += std::get<1>(d);
			std::get<1>(averages.at(found)) /= 2.0;
		}

		//printf("%s [id: %i]: %d\n", std::get<0>(d).c_str(), std::get<2>(d), std::get<1>(d));
#if CONDENSED_TIMINGS == 0
		std::time_t t = std::time(nullptr);
		std::tm tm; localtime_s(&tm, &t);
		std::ofstream datalog("datalog.csv", std::fstream::app);
		datalog << std::put_time(&tm, "%d-%m-%Y %H:%M:%S")
			<< ", " << std::get<2>(d)
			<< ", " << std::get<0>(d).c_str()
			<< ", " << std::fixed << std::setprecision(6) << std::get<1>(d)
			//<< ", " << dataSize
			//<< ", " << iterations
#if defined _DEBUG
			<< ", Debug"
#else
			<< ", Release"
#endif
			<< std::endl;
#endif
	}

	for (const auto& a : averages)
	{
		std::time_t t = std::time(nullptr);
		std::tm tm; localtime_s(&tm, &t);
		std::ofstream datalog("datalog.csv", std::fstream::app);
		datalog << std::endl
			<< std::put_time(&tm, "%d-%m-%Y %H:%M:%S")
			<< ",,,, " << std::get<0>(a)
			<< ", " << std::fixed << std::setprecision(6) << std::get<1>(a)
			<< ", RAN " << std::get<2>(a) << " TIMES"
#if defined _DEBUG
			<< ", Debug"
#else
			<< ", Release"
#endif
			<< std::endl;
	}

	printf("\nDATA LOGGED TO FILE, CLOSING...\n");
}