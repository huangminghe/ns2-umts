#ifndef myevalvid_sink_h
#define myevalvid_sink_h

#include <stdio.h>
#include "agent.h"

class myEvalvid_Sink : public Agent
{
public:
 		myEvalvid_Sink() : Agent(PT_UDP) 
 		{
 			pkt_received=0;		
 		} 		
        	void recv(Packet*, Handler*);
        	int command(int argc, const char*const* argv);
        	void print_status();
protected:
		char tbuf[100];
		FILE *tFile;
		unsigned long int  pkt_received;
};

#endif
