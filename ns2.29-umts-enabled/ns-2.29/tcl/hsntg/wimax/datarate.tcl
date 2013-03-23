# Test script to evaluate datarate in 802.16 networks.
# @author rouil
# Scenario: Communication between MN and Sink Node with MN attached to BS.

#
# Topology scenario:
#
#
#	        |-----|          
#	        | MN0 |                 ; 1.0.1 
#	        |-----|        
#
#
#		  (^)
#		   |
#	    |--------------|
#           | Base Station | 		; 1.0.0
#           |--------------|
#	    	   |
#	    	   |
#	     |-----------|
#            | Sink node | 		; 0.0.0
#            |-----------|
#
# Notes: 
# Traffic should not start before 25s for the following reasons:
# - Network Entry can be time consuming
#    - The time to discover the AP (i.e. receive DL_MAP) is fairly quick even
#      with channel scanning. In the order of few hundred ms.
#    - Default DCD/UCD interval is 5s. 
#    - Ranging/Registration is quick (<100ms)
# - Routing protocol used here is DSDV, with default updates interval of 15s.

#check input parameters
if {$argc != 2} {
	puts ""
	puts "Wrong Number of Arguments! 2 arguments for this script"
	puts "Usage: ns datarate.tcl modulation cyclic_prefix "
        puts "modulation: OFDM_BPSK_1_2, OFDM_QPSK_1_2, OFDM_QPSK_3_4"
        puts "            OFDM_16QAM_1_2, OFDM_16QAM_3_4, OFDM_64QAM_2_3, OFDM_64QAM_3_4"
        puts "cyclic_prefix: 0.25, 0.125, 0.0625, 0.03125"
	exit 
}

# set global variables
set output_dir .
set traffic_start 25
set traffic_stop  30
set simulation_stop 32

# Configure Wimax
Mac/802_16 set debug_ 0
Mac/802_16 set frame_duration_ 0.020

#define coverage area for base station: 500m coverage 
Phy/WirelessPhy/OFDM set g_ [lindex $argv 1]
Phy/WirelessPhy set Pt_ 0.025
Phy/WirelessPhy set RXThresh_ 2.025e-12 ;# 500m radius
Phy/WirelessPhy set CSThresh_ [expr 0.9*[Phy/WirelessPhy set RXThresh_]]

# Parameter for wireless nodes
set opt(chan)           Channel/WirelessChannel    ;# channel type
set opt(prop)           Propagation/TwoRayGround   ;# radio-propagation model
set opt(netif)          Phy/WirelessPhy/OFDM       ;# network interface type
set opt(mac)            Mac/802_16                 ;# MAC type
set opt(ifq)            Queue/DropTail/PriQueue    ;# interface queue type
set opt(ll)             LL                         ;# link layer type
set opt(ant)            Antenna/OmniAntenna        ;# antenna model
set opt(ifqlen)         50              	   ;# max packet in ifq
set opt(adhocRouting)   DSDV                       ;# routing protocol

set opt(x)		1100			   ;# X dimension of the topography
set opt(y)		1100			   ;# Y dimension of the topography

#defines function for flushing and closing files
proc finish {} {
        global ns tf output_dir nb_mn
        $ns flush-trace
        close $tf
	exit 0
}

#create the simulator
set ns [new Simulator]
$ns use-newtrace

#create the topography
set topo [new Topography]
$topo load_flatgrid $opt(x) $opt(y)
#puts "Topology created"

#open file for trace
set tf [open $output_dir/out.res w]
$ns trace-all $tf
#puts "Output file configured"

# set up for hierarchical routing (needed for routing over a basestation)
$ns node-config -addressType hierarchical
AddrParams set domain_num_ 2          			;# domain number
lappend cluster_num 1 1            			;# cluster number for each domain 
AddrParams set cluster_num_ $cluster_num
lappend eilastlevel 1 2 		;# number of nodes for each cluster (1 for sink and one for mobile node + base station
AddrParams set nodes_num_ $eilastlevel
puts "Configuration of hierarchical addressing done"

# Create God
create-god 2	

#creates the sink node in first address space.
set sinkNode [$ns node 0.0.0]
puts "sink node created"

#creates the Access Point (Base station)
$ns node-config -adhocRouting $opt(adhocRouting) \
                 -llType $opt(ll) \
                 -macType $opt(mac) \
                 -ifqType $opt(ifq) \
                 -ifqLen $opt(ifqlen) \
                 -antType $opt(ant) \
                 -propType $opt(prop)    \
                 -phyType $opt(netif) \
                 -channel [new $opt(chan)] \
                 -topoInstance $topo \
                 -wiredRouting ON \
                 -agentTrace ON \
                 -routerTrace ON \
                 -macTrace ON  \
                 -movementTrace OFF
#puts "Configuration of base station"

set bstation [$ns node 1.0.0]  
$bstation random-motion 0
#provide some co-ord (fixed) to base station node
$bstation set X_ 550.0
$bstation set Y_ 550.0
$bstation set Z_ 0.0
set clas [new SDUClassifier/Dest]
[$bstation set mac_(0)] add-classifier $clas
#set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
set bs_sched [new WimaxScheduler/BS]
$bs_sched set-default-modulation [lindex $argv 0]     ;#OFDM_BPSK_1_2
[$bstation set mac_(0)] set-scheduler $bs_sched
[$bstation set mac_(0)] set-channel 0
puts "Base-Station node created"

# creation of the mobile nodes
$ns node-config -wiredRouting OFF \
                -macTrace ON  				;# Mobile nodes cannot do routing.

set wl_node [$ns node 1.0.1] 	;# create the node with given @.	
$wl_node random-motion 0			;# disable random motion
$wl_node base-station [AddrParams addr2id [$bstation node-addr]] ;#attach mn to basestation
#compute position of the node
$wl_node set X_ 400.0
$wl_node set Y_ 550.0
$wl_node set Z_ 0.0
puts "wireless node created ..."

set clas [new SDUClassifier/Dest]
[$wl_node set mac_(0)] add-classifier $clas
#set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
set ss_sched [new WimaxScheduler/SS]
[$wl_node set mac_(0)] set-scheduler $ss_sched
[$wl_node set mac_(0)] set-channel 0

#create source traffic
#Create a UDP agent and attach it to node n0
set udp [new Agent/UDP]
$udp set packetSize_ 1500
$ns attach-agent $wl_node $udp

# Create a CBR traffic source and attach it to udp0
set cbr [new Application/Traffic/CBR]
$cbr set packetSize_ 1500
$cbr set interval_ 0.0005
$cbr attach-agent $udp

#create an sink into the sink node

# Create the Null agent to sink traffic
set null [new Agent/Null] 
$ns attach-agent $sinkNode $null

# Attach the 2 agents
$ns connect $udp $null

# create the link between sink node and base station
$ns duplex-link $sinkNode $bstation 100Mb 1ms DropTail

#Schedule start/stop of traffic
$ns at $traffic_start "$cbr start"
$ns at $traffic_stop "$cbr stop"

$ns at $simulation_stop "finish"
puts "Starts simulation"
$ns run
puts "Simulation done."
