#include "realtime-trace.h"
#include "realtime-trace.cc"

int main()
{
	char* fromNode = "Me";
	char* toNode = "You";
	char flag1 = 's';
	char flag2 = 'r';
	char* packetType = "cbr";
	int packetSize = 123;
	double sTime = 0.22333;
	double rTime = 1.345667;

	RealtimeTrace rt;

	rt.TraceType(fromNode, toNode, flag1,1233, sTime, rTime, packetType, packetSize);
	rt.TraceType(fromNode, toNode, flag2,1233, sTime, rTime, packetType, packetSize);
	


	double mean_delay = rt.GetMeanDelay(fromNode, toNode);
	double current_delay = rt.GetCurrentDelay(fromNode, toNode);
	std::cout<< mean_delay <<std::endl;
	std::cout<< current_delay << std::endl;

	rt.TraceType(fromNode, toNode, flag1,1234, sTime, rTime+9, packetType, packetSize);
	double mean_delay2 = rt.GetMeanDelay(fromNode, toNode);
	std::cout<< mean_delay2 <<std::endl;

	rt.TraceType(fromNode, toNode, flag2,1234, sTime, rTime+11, packetType, packetSize);

	double current_delay3 = rt.GetCurrentDelay(fromNode, toNode);
    std::cout<< current_delay3<<std::endl;

	return 0;

}

