#ifndef ns_myudp_h
#define ns_myudp_h

#include "udp.h"

class myUdpAgent : public UdpAgent {
public:
	myUdpAgent();
	virtual void sendmsg(int nbytes, AppData* data, const char *flags = 0);
	virtual int command(int argc, const char*const* argv);
protected:
	//added by smallko
	int id_;	
	char BWfile[100];
	FILE *BWFile;
	int openfile;
};

#endif
