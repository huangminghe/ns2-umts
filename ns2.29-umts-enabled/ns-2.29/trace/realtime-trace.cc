#include "realtime-trace.h"

std::map<int, iterm> RealtimeTrace::traceMap;

static class RealtimeTraceClass : public TclClass
{
public:
    RealtimeTraceClass():TclClass("Agent/RealtimeTrace"){}
    TclObject* create(int, const char*const*)
    {
        return(new RealtimeTrace());
    }
}realtimetrace_class;

int RealtimeTrace::command(int argc, const char*const* argv)
{
    if(strcmp(argv[1], "Test") == 0) {
        Test();
        return(TCL_OK);
    }
    if(argc == 5) {
        if(strcmp(argv[1], "GetMeanDelay") == 0) {
            //Here should check argv[2], argv[3] and argv[4] if they are valid
            //They should be 1.0.0 or 1.0.3 and something like that ip address;
            SetValueTest(GetMeanDelay(argv[2], argv[3], argv[4]));
            return(TCL_OK);
        } else if(strcmp(argv[1], "GetCurrentDelay") == 0) {
            //Here should check argv[2], argv[3] and argv[4] if they are valid
            //They should be 1.0.0 or 1.0.3 and something like that ip address;
            SetValueTest(GetCurrentDelay(argv[2], argv[3], argv[4]));
            return(TCL_OK);
        }
    }

    return(Agent::command(argc, argv));
}


RealtimeTrace::~RealtimeTrace()
{
//	WriteDelayOnFile("5.0.0", "3.0.2");
	//delete [] src_nodeaddr;
	//delete [] dst_nodeaddr;
}

void RealtimeTrace::Test()
{
    Tcl& tcl = Tcl::instance();
    tcl.eval("puts \"I am good\"");
}

double RealtimeTrace::SetValueTest(double val)
{
    Tcl& tcl = Tcl::instance();
    tcl.evalf("set val %f", val);
}

void RealtimeTrace::TraceSendType(const char* fromNode, const char flag, 
						const int packetUid, const double sendTime, 
						const char* packetType, const int packetSize)
{
	if((flag == 's' || flag == '+') 
        &&(strcmp(packetType, "cbr") == 0
            || strcmp(packetType, "tcp") ==0
            || strcmp(packetType, "telnet") == 0
            || strcmp(packetType, "audio") == 0
            || strcmp(packetType, "video") == 0
            || strcmp(packetType, "ack") == 0
            || strcmp(packetType, "sctp") ==0
            || strcmp(packetType, "scp_app1") == 0
            || strcmp(packetType, "HDLC") == 0)) {
        std::map<int, iterm> :: iterator iter = traceMap.find(packetUid);
        //if not found, it means had not trace this packet sending
        if(iter == traceMap.end()) {
	    	traceMap[packetUid].send_time_ = sendTime;
		    //obviously, when packet just being sending, recv_time should had not been set;
		    traceMap[packetUid].recv_time_ = 0;
		    traceMap[packetUid].packet_size_ = packetSize;

		    traceMap[packetUid].packet_type_ = new char[strlen(packetType)+1];
		    strncpy(traceMap[packetUid].packet_type_, packetType, strlen(packetType)+1);

            traceMap[packetUid].send_node_ = new char[strlen(fromNode)+1];
            strncpy(traceMap[packetUid].send_node_, fromNode, strlen(fromNode)+1);

        //initialize the recv_node_
            traceMap[packetUid].recv_node_ = new char[1];
            traceMap[packetUid].recv_node_[0] = '\0';
	    }
    }
}

void RealtimeTrace::TraceRecvType(const char* toNode, const char flag, 
			const int packetUid, const double recvTime, 
			const char* packetType, const int packetSize)
{
	if(flag == 'r' && strcmp(packetType, "cbr") == 0) {
		//keep the send_time still, and set the recv_time
        traceMap[packetUid].recv_time_ = recvTime;
        traceMap[packetUid].packet_size_ = packetSize;

        traceMap[packetUid].recv_node_ = new char[strlen(toNode)+1];
        strncpy(traceMap[packetUid].recv_node_, toNode, strlen(toNode)+1);
	}
}

void RealtimeTrace::TraceType(const char* fromNode, const char* toNode,
						const char flag, const int packetUid,
					 	const double sendTime, const double recvTime,
					 	const char* packetType, const int packetSize)
{
	if(flag == '+' || flag == 's') {
		RealtimeTrace::TraceSendType(fromNode, flag, packetUid, sendTime, packetType, packetSize);
	}
	if(flag == 'r') {
		RealtimeTrace::TraceRecvType(toNode, flag, packetUid, recvTime, packetType, packetSize);
	}
}


void  RealtimeTrace::WriteDelayOnFile(const char* src_nodeaddr, const char* dst_nodeaddr)
{	
	std::ofstream in;
	in.open("/home/eric/delay.txt", std::ios::app);
		
	std::map<int, iterm>::iterator iter;
	for(iter = traceMap.begin(); iter != traceMap.end(); ++iter)
	{
        if(strcmp(iter->second.send_node_, src_nodeaddr) == 0 
           && strcmp(iter->second.recv_node_, dst_nodeaddr) == 0) {
		    in<<iter->first;
		    in<<" ";
		    in<<iter->second.packet_type_;
	    	in<<" ";
            in<<iter->second.send_node_;
            in<<" ";
            in<<iter->second.recv_node_;
            in<<" ";
		    in<<(iter->second.recv_time_ - iter->second.send_time_);
		    in<<"\n";
        }
	}

	in.close();
}

double RealtimeTrace::GetMeanDelay(const char* src_nodeaddr, const char* dst_nodeaddr, const char* pt)
{
	std::ofstream in;
	in.open("/home/eric/mean_delay.txt", std::ios::app);
		
	std::map<int, iterm>::iterator iter;
	double mean_delay = 0.0;
    long sec_packet_num = 0;

	for(iter = traceMap.begin(); iter != traceMap.end(); ++iter) {
        if(strncmp(iter->second.send_node_, src_nodeaddr, strlen(src_nodeaddr)) == 0 
                && strncmp(iter->second.recv_node_, dst_nodeaddr, strlen(dst_nodeaddr)) == 0
                && strncmp(iter->second.packet_type_, pt, strlen(pt)) == 0) {
		    mean_delay += iter->second.recv_time_ - iter->second.send_time_;
            sec_packet_num++;
        }
	}

	in<<mean_delay;
    in<<"\n";
    in.close();

	return sec_packet_num ?  (mean_delay/sec_packet_num) : 0;
}

double RealtimeTrace::GetCurrentDelay(const char* src_nodeaddr, const char* dst_nodeaddr, const char* pt)
{
	std::ofstream in;
	in.open("/home/eric/current_delay.txt", std::ios::app);
		
	std::map<int, iterm>::iterator iter;
	double  current_delay = 0.0;
	for(iter = traceMap.end(), --iter; 
            (strncmp(iter->second.send_node_, src_nodeaddr,  strlen(src_nodeaddr)) != 0 
             || strncmp(iter->second.recv_node_, dst_nodeaddr, strlen(dst_nodeaddr)) != 0 
             || strncmp(iter->second.packet_type_, pt, strlen(pt)) != 0); 
            --iter) {
        ;
    }
    
	current_delay = iter->second.recv_time_ - iter->second.send_time_;
	in<<current_delay;
	in.close();

	return current_delay;
}
