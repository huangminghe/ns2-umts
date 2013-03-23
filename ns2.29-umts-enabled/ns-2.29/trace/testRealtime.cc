#include "realtime-trace.h"
#include "realtime-trace.cc"

int main()
{
	char* fromNode = "Me";
	char* toNode = "You";
	char flag1 = 's';
	char flag2 = 'r';
	int packetSize = 123;
	double sTime = 0.22333;
	double rTime = 1.345667;

	RealtimeTrace rt;


	rt.TraceType(fromNode, toNode, flag1,1233, sTime, rTime, packetSize);
	rt.TraceType(fromNode, toNode, flag2,1233, sTime, rTime, packetSize);

	double mean_delay = rt.GetMeanDelay(fromDelay, toNode);
	double current_delay = rt.GetCurrentDelay(fromDelay, toNode);
	std::cout<<mean_delay;
	std::cout<<current_delay;

	return 0;

}

