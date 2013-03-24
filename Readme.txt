Welcome to the ns2-umts wiki!

this repo include a whole ns2.29-allinone and a simulation of handover between UMTS cell and WLAN. what a wonderful thing is that I developed the real time network performance monitor function for ns2, so we can handover based on the real time network performance monitor.

How to

1.Download ns2.29-allinone and its UMTS extension package
Download the ns2.29-allinone here (http://www.isi.edu/nsnam/dist/)
Download the UMTS extension package here ()

2.Installation of ns2.29-allinone with UMTS enabled
a.Installation preparation
	Before you install ns2.29, you should install some necessary packages. For Fedora user, you need:
		$ yum install gcc.
		$ yum install tcl-devel.
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