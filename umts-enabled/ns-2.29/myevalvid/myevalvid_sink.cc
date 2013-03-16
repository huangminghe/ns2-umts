/*
 * 050406 - rouil - comment unused variables
 */

#include <stdio.h>
#include <stdlib.h>
#include "myevalvid_sink.h"
#include "ip.h"
#include "udp.h"
#include "rtp.h"

static class myEvalvid_SinkClass : public TclClass {
public:
        myEvalvid_SinkClass() : TclClass("Agent/myEvalvid_Sink") {}
        TclObject* create(int, const char*const*) {
                return (new myEvalvid_Sink);
        }
} class_myEvalvid_Sink;

void myEvalvid_Sink::recv(Packet* pkt, Handler*)
{
  //int i, j;
        //hdr_ip* iph=hdr_ip::access(pkt);
  	hdr_cmn* hdr=hdr_cmn::access(pkt);
	//hdr_rtp* rtp = hdr_rtp::access(pkt);
	
	pkt_received+=1;
		
	fprintf(tFile,"%-16f id %-16ld udp %-16d\n", Scheduler::instance().clock(), hdr->frame_pkt_id_, hdr->size()-28);
		
	if (app_)
               app_->recv(hdr_cmn::access(pkt)->size());

        Packet::free(pkt);
}

int myEvalvid_Sink::command(int argc, const char*const* argv)
{
  //Tcl& tcl = Tcl::instance();
	
	if (strcmp(argv[1], "set_filename") == 0) {
		strcpy(tbuf, argv[2]);
		tFile = fopen(tbuf, "w");
		return (TCL_OK);
	}  
	
	if (strcmp(argv[1], "closefile") == 0) {		
		fclose(tFile);
		return (TCL_OK);
	}
	
	if(strcmp(argv[1],"printstatus")==0) {
		print_status();
		return (TCL_OK);
	}
	
	return (Agent::command(argc, argv));
}

void myEvalvid_Sink::print_status()
{
	printf("myEvalvid_Sink)Total packets received:%ld\n", pkt_received);
}
