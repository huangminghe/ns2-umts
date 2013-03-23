#include "myudp.h"
#include "rtp.h"
#include "random.h"
#include "address.h"
#include "ip.h"


static class myUdpAgentClass : public TclClass {
public:
	myUdpAgentClass() : TclClass("Agent/myUDP") {}
	TclObject* create(int, const char*const*) {
		return (new myUdpAgent());
	}
} class_myudp_agent;

myUdpAgent::myUdpAgent() : id_(0), openfile(0)
{
	bind("packetSize_", &size_);
	UdpAgent::UdpAgent();
}

void myUdpAgent::sendmsg(int nbytes, AppData* data, const char* flags)
{
	Packet *p;
	int n;
	char buf[100]; //added by smallko

	if (size_)
		n = nbytes / size_;
	else
		printf("Error: myUDP size = 0\n");

	if (nbytes == -1) {
		printf("Error:  sendmsg() for UDP should not be -1\n");
		return;
	}	

	// If they are sending data, then it must fit within a single packet.
	if (data && nbytes > size_) {
		printf("Error: data greater than maximum myUDP packet size\n");
		return;
	}

	double local_time = Scheduler::instance().clock();
	while (n-- > 0) {
		p = allocpkt();
		hdr_cmn::access(p)->size() = size_;
		hdr_rtp* rh = hdr_rtp::access(p);
		rh->flags() = 0;
		rh->seqno() = ++seqno_;
		hdr_cmn::access(p)->timestamp() = 
		    (u_int32_t)(SAMPLERATE*local_time);
		hdr_cmn::access(p)->sendtime_ = local_time;	// (smallko)
		if(openfile!=0){
			hdr_cmn::access(p)->frame_pkt_id_ = id_++;
			sprintf(buf, "%-16f id %-16ld udp %-16d\n", local_time, hdr_cmn::access(p)->frame_pkt_id_, hdr_cmn::access(p)->size()-28);
			fwrite(buf, strlen(buf), 1, BWFile); 
			//printf("%-16f id %-16d udp %-16d\n", local_time, hdr_cmn::access(p)->frame_pkt_id_, hdr_cmn::access(p)->size()-28);
		}
		// add "beginning of talkspurt" labels (tcl/ex/test-rcvr.tcl)
		if (flags && (0 ==strcmp(flags, "NEW_BURST")))
			rh->flags() |= RTP_M;
		p->setdata(data);
		target_->recv(p);
	}
	n = nbytes % size_;
	if (n > 0) {
		p = allocpkt();
		hdr_cmn::access(p)->size() = n;
		hdr_rtp* rh = hdr_rtp::access(p);
		rh->flags() = 0;
		rh->seqno() = ++seqno_;
		hdr_cmn::access(p)->timestamp() = 
		    (u_int32_t)(SAMPLERATE*local_time);
		hdr_cmn::access(p)->sendtime_ = local_time;	// (smallko)
		if(openfile!=0){
			hdr_cmn::access(p)->frame_pkt_id_ = id_++;
			sprintf(buf, "%-16f id %-16ld udp %-16d\n", local_time, hdr_cmn::access(p)->frame_pkt_id_, hdr_cmn::access(p)->size()-28);
			fwrite(buf, strlen(buf), 1, BWFile); 
			//printf("%-16f id %-16d udp %-16d\n", local_time, hdr_cmn::access(p)->frame_pkt_id_, hdr_cmn::access(p)->size()-28);
		}
		// add "beginning of talkspurt" labels (tcl/ex/test-rcvr.tcl)
		if (flags && (0 == strcmp(flags, "NEW_BURST")))
			rh->flags() |= RTP_M;
		p->setdata(data);
		target_->recv(p);
	}
	idle();
}

int myUdpAgent::command(int argc, const char*const* argv)
{
	if(argc ==2) {		//added by smallko
		if (strcmp(argv[1], "closefile") == 0) {
			if(openfile==1)
				fclose(BWFile);
			return (TCL_OK);
		}
	
	} 
	
	if (argc ==3) {  	//added by smallko
		if (strcmp(argv[1], "set_filename") == 0) {
			strcpy(BWfile, argv[2]);
			BWFile = fopen(BWfile, "w");
			openfile=1;
			return (TCL_OK);
		}
	}
	
	return (UdpAgent::command(argc, argv));
}
