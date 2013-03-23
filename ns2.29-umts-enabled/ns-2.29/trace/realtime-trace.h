#ifndef REALTIME_TRACE_H
#define REALTIME_TRACE_H
#include <map>
#include <string.h>
#include <iostream>
#include <fstream>
#include "/home/eric/ns/ns-allinone-2.29/ns-2.29/common/agent.h"
#include "trace.h"

struct iterm {
    char* send_node_;
    char* recv_node_;
	double send_time_;
	double recv_time_;
	char* packet_type_;
	int packet_size_;
};

class RealtimeTrace : public Agent {
private:
	// the source node
	//char* src_nodeaddr;
	// the destination node
	//char* dst_nodeaddr;
public:
	//map<uid, <send_time, recv_time, packet_size>> to store the trace data
	static std::map<int, iterm> traceMap;
    
    //static double current_delay_;
    //static double mean_delay_;
	
	RealtimeTrace() : Agent(PT_UDP){};
	~RealtimeTrace();
	void TraceSend(const char* fromNode, const char flag, const int packedUid,
					const double sendTime, const int packetSize);
	void TraceRecv(const char* toNode, const char flag, const int packedUid, 
					const double recvTime, const int packetSize);
	void Trace(const char* fromNode, const char* toNode, 
                const char flag, const int packedUid, const double sendTime, 
					const double recvTime, const int packetSize);
	void TraceSendType(const char* fromNode, const char flag, const int packedUid, const double sendTime,
					const char* packetType, const int packetSize);
	void TraceRecvType(const char* toNode, const char flag, const int packedUid, const double recvTime, 
						const char* packetType, const int packetSize);
	void TraceType(const char* fromNode, const char* toNode, 
                    const char flag, const int packedUid, 
					const double sendTime, 	const double recvTime,
					const char* packetType, const int packetSize);

	double GetMeanDelay(const char* src_nodeaddr, const char* dst_nodeaddr, const char* pt);
	double GetCurrentDelay(const char* src_nodeaddr, const char* dst_nodeaddr, const char* pt);

	void WriteDelayOnFile(const char* src_nodeaddr, const char* dst_nodeaddr);

    void Test();
    double SetValueTest(double val);
protected:
    int command(int argc, const char*const* argv);
};
#endif
