# Test for MutiFaceNodes.
# @author rouil
# @date 05/19/2005.
# Scenario: Create a multi-interface node using different technologies
#           There is a TCP connection between the router0 and MultiFaceNode.
#           We first use the UMTS interface, then we switch the traffic 
#           to the 802.11 interface when it becomes available. Then the Ethernet
#           link is connected and we switch to that interface.
#           To test disconnection, we disconnect Ethernet to switch to 802.11, then 
#           the node leaving the coverage area of 802.11 creates a link going down 
#           event to redirect to UMTS.
#
# Topology scenario:
#
#                                   bstation802.11(2.0.0)->)
#                                   /
#                                  /
# router0(0.0.0)---router1(1.0.0)---                             +------------------------------------+
#                                  \                             + iface1:802.11(2.0.1)|              |
#                                   \                            +---------------------+ MutiFaceNode |
#                                   bstation802.16(3.0.0)->)     + iface2:802.16(3.0.1)|  (4.0.0)     |
#                                                                +------------------------------------+
#                                                   
#                                                                
#              
#

#check input parameters
if {$argc != 3} {
	puts ""
	puts "Wrong Number of Arguments! No arguments in this topology"
	puts "ns wifi_wimax.tcl seed gd_threshold shutdown"
	puts ""
	exit (1)
}

global ns

#set debug attributes
Agent/ND set debug_ 1
Agent/MIH set debug_ 1
Agent/MIHUser/IFMNGMT/MIPV6 set debug_ 1
Mac/802_16 set debug_ 1 
Mac/802_11 set debug_ 1 

Mac/802_16 set dcd_interval_         5 ;#max 10s
Mac/802_16 set ucd_interval_         5 ;#max 10s
set default_modulation               OFDM_16QAM_3_4 ;#OFDM_BPSK_1_2
set contention_size                  5 ;#for initial ranging and bw  
Mac/802_16 set t21_timeout_          0.02 ;#max 10s, to replace the timer for looking at preamble 
Mac/802_16 set client_timeout_       50 

#random used in movement of MN
set seed [lindex $argv 0]
set move [new RandomVariable/Uniform]
$move set min_ 2
$move set max_ 12
for {set j 0} {$j < $seed} {incr j} {
    set departure [$move value]
}


#defines function for flushing and closing files
proc finish {} {
    global ns f
    $ns flush-trace
    close $f
    puts " Simulation ended."
    exit 0
}

#$defaultRNG seed [lindex $argv 0]
Mac/802_11 set pr_limit_ [lindex $argv 1] ;#1.0 for link down only
Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover2 set shutdown_on_ack_ [lindex $argv 2]

# set global variables
set output_dir .
set traffic_start 5
set traffic_stop  70
set simulation_stop 70

#create the simulator
set ns [new Simulator]
$ns use-newtrace

#open file for trace
set f [open out.res w]
$ns trace-all $f

# set up for hierarchical routing (needed for routing over a basestation)
$ns node-config -addressType hierarchical
AddrParams set domain_num_  5                   ;# domain number
AddrParams set cluster_num_ {1 1 1 1 1}           ;# cluster number for each domain 
AddrParams set nodes_num_   {1 1 2 2 1}           ;# number of nodes for each cluster             

# Node address for router0 and router1 are 4 and 5, respectively.
set router0 [$ns node 0.0.0]
puts "router0: tcl=$router0; id=[$router0 id]; addr=[$router0 node-addr]"
set router1 [$ns node 1.0.0]
puts "router1: tcl=$router1; id=[$router1 id]; addr=[$router1 node-addr]"

# connect links 
$ns duplex-link $router1 $router0 100MBit 30ms DropTail 1000

# creation of the MutiFaceNodes. It MUST be done before the 802.11
$ns node-config  -multiIf ON                            ;#to create MultiFaceNode 
set multiFaceNode [$ns node 4.0.0]                      ;# node id is 6
$ns node-config  -multiIf OFF                           ;#reset attribute
puts "multiFaceNode: tcl=$multiFaceNode; id=[$multiFaceNode id]; addr=[$multiFaceNode node-addr]"

#
# Now we add 802.11 nodes
#

# parameter for wireless nodes
set opt(chan)           Channel/WirelessChannel    ;# channel type for 802.11
set opt(prop)           Propagation/TwoRayGround   ;# radio-propagation model 802.11
set opt(netif)          Phy/WirelessPhy            ;# network interface type 802.11
set opt(mac)            Mac/802_11                 ;# MAC type 802.11
set opt(ifq)            Queue/DropTail/PriQueue    ;# interface queue type 802.11
set opt(ll)             LL                         ;# link layer type 802.11
set opt(ant)            Antenna/OmniAntenna        ;# antenna model 802.11
set opt(ifqlen)         50              	   ;# max packet in ifq 802.11
set opt(adhocRouting)   DSDV                       ;# routing protocol 802.11
set opt(umtsRouting)    ""                         ;# routing for UMTS (to reset node config)

set opt(x)		2000			   ;# X dimension of the topography
set opt(y)		2000			   ;# Y dimension of the topography

# configure rate for 802.11
Mac/802_11 set basicRate_ 1Mb
Mac/802_11 set dataRate_ 11Mb
Mac/802_11 set bandwidth_ 11Mb

#create the topography
set topo [new Topography]
$topo load_flatgrid $opt(x) $opt(y)
#puts "Topology created"
set chan [new $opt(chan)]

# create God
create-god 11				                ;# give the number of nodes 

#configure for 20m radius 2.4Ghz
Phy/WirelessPhy set Pt_ 0.025
Phy/WirelessPhy set freq_ 2412e+6
Phy/WirelessPhy set RXThresh_ 6.12277e-09
Phy/WirelessPhy set CSThresh_ [expr 0.9*[Phy/WirelessPhy set RXThresh_]]

# configure Access Points
$ns node-config  -adhocRouting $opt(adhocRouting) \
                 -llType $opt(ll) \
                 -macType $opt(mac) \
                 -channel $chan \
                 -ifqType $opt(ifq) \
                 -ifqLen $opt(ifqlen) \
                 -antType $opt(ant) \
                 -propType $opt(prop)    \
                 -phyType $opt(netif) \
                 -topoInstance $topo \
                 -wiredRouting ON \
                 -agentTrace ON \
                 -routerTrace OFF \
                 -macTrace ON  \
                 -movementTrace OFF

# configure Base station 802.11
set bstation802 [$ns node 2.0.0] ;
$bstation802 set X_ 500.0
$bstation802 set Y_ 1000.0
$bstation802 set Z_ 0.0
puts "bstation802: tcl=$bstation802; id=[$bstation802 id]; addr=[$bstation802 node-addr]"
# we need to set the BSS for the base station
set bstationMac [$bstation802 getMac 0]
set AP_ADDR_0 [$bstationMac id]
puts "bss_id for bstation 1=$AP_ADDR_0"
$bstationMac bss_id $AP_ADDR_0
$bstationMac enable-beacon

# creation of the wireless interface 802.11
$ns node-config -wiredRouting OFF \
                -macTrace ON 				
set iface1 [$ns node 2.0.1] 	                                   ;# node id is 8.	
$iface1 random-motion 0			                           ;# disable random motion
$iface1 base-station [AddrParams addr2id [$bstation802 node-addr]] ;#attach mn to basestation
$iface1 set X_ 470.0
$iface1 set Y_ 1000.0
$iface1 set Z_ 0.0
# define node movement. We start from outside the coverage, cross it and leave.
$ns at $departure "$iface1 setdest 540.0 1000.0 1.0"
puts "iface1: tcl=$iface1; id=[$iface1 id]; addr=[$iface1 node-addr]"		    

# add link to backbone
$ns duplex-link $bstation802 $router1 100MBit 15ms DropTail 1000

# add Wimax nodes
set opt(netif)          Phy/WirelessPhy/OFDM       ;# network interface type 802.16
set opt(mac)            Mac/802_16                 ;# MAC type 802.16

# radius =  
Phy/WirelessPhy set Pt_ 0.025
#Phy/WirelessPhy set RXThresh_ 7.91016e-15 ;#500m:2.025e-12
Phy/WirelessPhy set RXThresh_ 1.26562e-13 ;#1000m radius
Phy/WirelessPhy set CSThresh_ [expr 0.8*[Phy/WirelessPhy set RXThresh_]]

# configure Access Points
$ns node-config  -adhocRouting $opt(adhocRouting) \
                 -llType $opt(ll) \
                 -macType $opt(mac) \
                 -channel $chan \
                 -ifqType $opt(ifq) \
                 -ifqLen $opt(ifqlen) \
                 -antType $opt(ant) \
                 -propType $opt(prop)    \
                 -phyType $opt(netif) \
                 -topoInstance $topo \
                 -wiredRouting ON \
                 -agentTrace ON \
                 -routerTrace ON \
                 -macTrace ON  \
                 -movementTrace OFF

# configure Base station 802.16
set bstation802_16 [$ns node 3.0.0] ;
$bstation802_16 set X_ 1000
$bstation802_16 set Y_ 1000
$bstation802_16 set Z_ 0.0
puts "bstation802_16: tcl=$bstation802_16; id=[$bstation802_16 id]; addr=[$bstation802_16 node-addr]"
set clas [new SDUClassifier/Dest]
[$bstation802_16 set mac_(0)] add-classifier $clas
#set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
set bs_sched [new WimaxScheduler/BS]
$bs_sched set-default-modulation $default_modulation
[$bstation802_16 set mac_(0)] set-scheduler $bs_sched
[$bstation802_16 set mac_(0)] set-channel 1

# creation of the wireless interface 802.11
$ns node-config -wiredRouting OFF \
                -macTrace ON 				
set iface2 [$ns node 3.0.1] 	                                   ;# node id is 8.	
$iface2 random-motion 0			                           ;# disable random motion
$iface2 base-station [AddrParams addr2id [$bstation802_16 node-addr]] ;#attach mn to basestation
$iface2 set X_ 470.0
$iface2 set Y_ 1000.0
$iface2 set Z_ 0.0
set clas [new SDUClassifier/Dest]
[$iface2 set mac_(0)] add-classifier $clas
#set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
set ss_sched [new WimaxScheduler/SS]
[$iface2 set mac_(0)] set-scheduler $ss_sched
[$iface2 set mac_(0)] set-channel 0
# define node movement. We start from outside the coverage, cross it and leave.
$ns at $departure "$iface2 setdest 540.0 1000.0 1.0"
puts "iface2: tcl=$iface2; id=[$iface2 id]; addr=[$iface2 node-addr]"		    

# add link to backbone
$ns duplex-link $bstation802_16 $router1 100MBit 15ms DropTail 1000

# add interfaces to MultiFaceNode
$multiFaceNode add-interface-node $iface1
$multiFaceNode add-interface-node $iface2


# install ND modules

# now WLAN
set nd_bs [$bstation802 install-nd]
$nd_bs set-router TRUE
$nd_bs router-lifetime 1800

set nd_mn [$iface1 install-nd]

# now WIMAX
set nd_bs2 [$bstation802_16 install-nd]
$nd_bs2 set-router TRUE
$nd_bs2 router-lifetime 20 ;#just enough to expire while we are connected to wlan.

set nd_mn2 [$iface2 install-nd]

# install interface manager into multi-interface node and CN
Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover2 set debug_ 1
set handover [new Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover2]
$multiFaceNode install-ifmanager $handover
$nd_mn set-ifmanager $handover
$handover nd_mac $nd_mn [$iface1 set mac_(0)] ;#to know how to send RS
$nd_mn2 set-ifmanager $handover
$handover nd_mac $nd_mn2 [$iface2 set mac_(0)] ;#to know how to send RS


set ifmgmt_cn [$router0 install-default-ifmanager]

# install MIH in multi-interface node
set mih [$multiFaceNode install-mih]

$handover connect-mih $mih ;#create connection between MIH and iface management

# install MIH on AP/BS
set mih_bs [$bstation802 install-mih]
set tmp_bs [$bstation802 set mac_(0)]
$tmp_bs mih $mih_bs
$mih_bs add-mac $tmp_bs

set mih_bs [$bstation802_16 install-mih]
set tmp_bs [$bstation802_16 set mac_(0)]
$tmp_bs mih $mih_bs
$mih_bs add-mac $tmp_bs


# Now we can register the MIH module with all the MACs
set tmp2 [$iface1 set mac_(0)] ;#in 802.11 one interface is created
$tmp2 mih $mih
$mih add-mac $tmp2             ;#inform the MIH about the local MAC
set tmp2 [$iface2 set mac_(0)] ;#in 802.16 one interface is created
$tmp2 mih $mih
$mih add-mac $tmp2             ;#inform the MIH about the local MAC

# set the starting time for Router Advertisements
$ns at 2 "$nd_bs start-ra"
$ns at 2 "$nd_bs2 start-ra"

#traffic
##configure traffic
set i 0
set udpvideo_($i) [new Agent/UDP]
$udpvideo_($i) set packetSize_ 1240

#create video traffic
set cbrvideo_($i) [new Application/Traffic/CBR]
$cbrvideo_($i) set packetSize_ 4960
$cbrvideo_($i) set interval_ 0.1
$cbrvideo_($i) attach-agent $udpvideo_($i)
set nullvideo_($i) [new Agent/Null]

#sinkNode is transmitter    
$ns attach-agent $router0 $udpvideo_($i)
$ns attach-agent $multiFaceNode $nullvideo_($i)

$handover add-flow $nullvideo_($i) $udpvideo_($i) $iface2 1 

$ns at $traffic_start "$cbrvideo_($i) start"
$ns at $traffic_stop "$cbrvideo_($i) stop"
$ns at $simulation_stop "finish"

puts " Simulation is running ... please wait ..."
$ns run
