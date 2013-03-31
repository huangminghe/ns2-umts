
Welcome!

this repo include a whole ns2.29-allinone and a simulation of handover between UMTS cell and WLAN. what a wonderful thing is that I developed the real time network performance monitor function for ns2, so we can handover based on the real time network performance monitor.

How to

1.Download ns2.29-allinone and its UMTS extension package
    Two choices for you 
	a. using the package I have compile and it works on my Fedora17.
	b. download these two packages individully and make it works by yourself, what need to mention 	is both of choice are need you to recompile it. individule package of ns2.29-allinone and UMTS extension package is in the umts-extension-package directory.

2.Installation of ns2.29-allinone with UMTS enabled

a.Installation preparation
	Before you install ns2.29, you should install some necessary packages. For Fedora user, you need:
		$ yum install gcc
		$ yum install tcl-devel
		$ yum install autoconf
		$ yum install automake
		$ yum install gcc-c++
		$ yum install libX11-devel
		$ yum install xorg-x11-proto-devel
		$ yum install libXt-devel
		$ yum install libXmu-devel
		$ yum install libtool
	After that, what you have to do is install the gcc4.1(for my fedora15, 16 17, gcc4.1.2 works fins), the version of gcc is very important for ns2.29’s installation, so I recommend the gcc4.1.2. Download it here (http://gcc.gnu.org/mirrors.html). After your gcc4.1.2 installed, you have two version of gcc on your computer now, so you have to modify you path of gcc compile link to your gcc4.1.2. which can be done by this:
		$which gcc 
	To get the symbol link of gcc location, the cd to the directory and do
		$sudo mv gcc gcc_bak
		$sudo mv g++ g++_bak
		$sudo mv cc cc_bak
		$sudo other-gcc- other-gcc-bak
	Then you have create the new symbol link to your new version of gcc4.1.2 by:
	$sudo ln –s /directory/to/gcc4.1.2/bin/gcc gcc
	And you should do that for all of them, like g++, cc, c++ or something more.

b.	Installation of ns2.29 with UMTS enabled.
	First thing you have to do is extracting the UMTS package and mv it to ns2.29-allinone to over cover the original ns2.29 directory within ns2.29-allinone directory. Then you can make install it with the simple command now:
		$./install
	There should be some troubles happen, and you can refer to
	This troubles shooting notes here ()

c.	Patch your ns2.29 with my real time performance monitor patch.
	Actually, this version of ns2.29-umts-enabled have patched, but I think maybe a lot person may have troubles installing my own version, so if you install the brand new version of ns2.29-allinone and the UMTS extension package, you should patch it like following to make it has real time performance monitor feature.
		$cd /to/ns2.29/trace/
		$patch –p1 < trace.patch
	Of cause you have to specify to location of patch file. After that you can remake your ns2.29 and have fun.

3.	Usage of real time performance monitor function
You just only type some command in your TCL scripts then easy to get the performance value of real time network.
set my_trace [new Agent/RealtimeTrace]
	#set mean_delay [$my_trace GetMeanDelay “src_nodeaddr” “dst_nodeaddr” “packet_type”
Like.
	set mean_delay [$my_trace GetMeanDelay “5.0.0” “3.0.2” “cbr” ]
	puts “$mean_delay”
You can refer to a handover between UMTS and WLAN according to mean_delay simulation example in the example directory. Have fun!

Trouble shooting

1 For gcc4.1.2 installaion
	open configure file with "$vim configure" find the "# For an installed makeinfo, we require it to be from texinfo 4.2 or# higher, else we use the "missing" dummy.if ${MAKEINFO} --version \| egrep 'texinfo[^0-9]*([1-3][0-9]|4\.[2-9]|[5-9])' >/dev/null 2>&1; then:elseMAKEINFO="$MISSING makeinfo"fi;;"
	and change the 'texinfo[^0-9]*([1-3][0-9]|4\.[2-9]|[5-9])' to 'texinfo[^0-9]*([1-3][0-9]|4\.[2-9]|4\.[1-9][0-9]*|[5-9])'

2 For ns2.29-allinone installaion

	when compiling the ns2 source code, there are some problems I have met and solutions are as follow.

	(1).
	./sctp/sctp.h:705: error: extra qualification 'SctpAgent::' on member 'DumpSendBuffer'
	make: *** [trace/trace.o] Error 1
	---just delete  'SctpAgent::'
	(2).
	./mobile/god.h:88: error：extra qualification ‘vector::’ on member ‘operator=’
	./mobile/god.h:93: error：extra qualification ‘vector::’ on member ‘operator+=’
	./mobile/god.h:98: error：extra qualification ‘vector::’ on member ‘operator==’
	./mobile/god.h:101: error：extra qualification ‘vector::’ on member ‘operator!=’
	---just delete 'vector::'。
	(3).
	queue/red.cc:874: error: invalid conversion from ‘const char*’ to ‘char*’
	queue/red.cc:875: error: invalid conversion from ‘const char*’ to ‘char*’
	queue/red.cc:876: error: invalid conversion from ‘const char*’ to ‘char*’
	queue/red.cc:877: error: invalid conversion from ‘const char*’ to ‘char*’
	---change "char *p" to "const char *p"
	(4).
	dsr/dsragent.cc: In member function ‘void DSRAgent::handleFlowForwarding(SRPacket&, int)’:
	dsr/dsragent.cc:828: error: ‘XmitFlowFailureCallback’ was not declared in this scope
	dsr/dsragent.cc: In member function ‘void DSRAgent::sendOutPacketWithRoute(SRPacket&, bool, Time)’:
	dsr/dsragent.cc:1385: error: ‘XmitFailureCallback’ was not declared in this scope
	dsr/dsragent.cc:1386: error: ‘XmitFlowFailureCallback’ was not declared in this scope
	dsr/dsragent.cc:1403: error: ‘XmitFailureCallback’ was not declared in this scope

	---add the declearation of the functions before line 828 
		void XmitFlowFailureCallback(Packet *pkt, void *data);
		void XmitFailureCallback(Packet *pkt, void *data);
	(5).
	diffusion/diffusion.cc:427: error: ‘XmitFailedCallback’ was not declared in this scope
	make: *** [diffusion/diffusion.o] Error 1
	---add the declearation of the functions before line 427
		void XmitFailedCallback(Packet *pkt, void *data);
	(6).
	diffusion/omni_mcast.cc:388: error: ‘OmniMcastXmitFailedCallback’ was not declared in this scope
	make: *** [diffusion/omni_mcast.o] Error 1
	---add the declearation of the functions before line 388
		void OmniMcastXmitFailedCallback(Packet *pkt, void *data);
	(7).
	queue/rio.cc:565: error: invalid conversion from ‘const char*’ to ‘char*’
	queue/rio.cc:566: error: invalid conversion from ‘const char*’ to ‘char*’
	queue/rio.cc:567: error: invalid conversion from ‘const char*’ to ‘char*’
	queue/rio.cc:568: error: invalid conversion from ‘const char*’ to ‘char*’
	queue/rio.cc:569: error: invalid conversion from ‘const char*’ to ‘char*’
	queue/rio.cc:570: error: invalid conversion from ‘const char*’ to ‘char*’
	queue/rio.cc:571: error: invalid conversion from ‘const char*’ to ‘char*’
	make: *** [queue/rio.o] Error 1
	---change "char *p" to "const char *p"
	(8).
	tcp/tcp-sack-rh.cc:68: error: extra qualification ‘SackRHTcpAgent::’ on member ‘newack’
	make: *** [tcp/tcp-sack-rh.o] Error 1
	---just delete ‘SackRHTcpAgent::’
	(9).
	pgm/pgm-agent.cc:307: error: extra qualification ‘PgmAgent::’ on member ‘trace_event’
	make: *** [pgm/pgm-agent.o] Error 1
	---just delete  ‘PgmAgent::’
	(10).
	pgm/pgm-sender.cc:189: error: extra qualification ‘PgmSender::’ on member ‘trace_event’
	make: *** [pgm/pgm-sender.o] Error 1
	---just delete ‘PgmSender::’
	(11).
	pgm/pgm-receiver.cc:186: error: extra qualification ‘PgmReceiver::’ on member ‘trace_event’
	make: *** [pgm/pgm-receiver.o] Error 1
	---just delete ‘PgmReceiver::’
	(12).
	setdest.h:26: error: extra qualification ‘vector::’ on member ‘operator=’
	setdest.h:31: error: extra qualification ‘vector::’ on member ‘operator+=’
	setdest.h:36: error: extra qualification ‘vector::’ on member ‘operator==’
	setdest.h:39: error: extra qualification ‘vector::’ on member ‘operator!=’
	make[1]: *** [setdest.o] Error 1
	---just delete ‘vector::’
