# Test datarate for 802.11 nodes.
# @author rouil
# @date 04/01/2006

#
# Topology scenario:
#
#
#	        |-----|               |-----|
#	        | MN0 |               | MN1 |
#	        |-----|               |-----|
#
#
#
#		  (^)                   (^)
#		   |                     |
#	    |--------------|      |--------------|  
#           |     AP 0     | 	  |     AP 1     |     
#           |--------------|      |--------------|
#	    	   |             /
#	    	   |            /
#	     |-----------|     /
#            | Sink node |____/ 		; node 0
#            |-----------|
#
# To see the number of packets dropped during simulation, execute "grep -c ^d out.res"


#check input parameters
if {$argc != 3} {
	puts ""
	puts "Wrong Number of Arguments! Please provide in the following order:"
	puts ""
	puts "1-channel AP0"
        puts "2-channel AP1"
        puts "3-interferences (0=no, 1=yes)"
	puts ""
	puts "Example: ns node802.11.tcl 1 3 1"
	exit
}

# set global variables
set channelAP0 [lindex $argv 0]
set channelAP1 [lindex $argv 1]
set interference [lindex $argv 2]
set nb_mn 2 				;# number of mobile node
set packet_size	1000			;# packet size in bytes at CBR applications 
set output_dir .
set gap_size 0.01
set traffic_start 2
set traffic_stop  5
set simulation_stop 5

# Parameter for wireless nodes
set opt(chan)           Channel/WirelessChannel    ;# channel type
set opt(prop)           Propagation/TwoRayGround   ;# radio-propagation model
set opt(netif)          Phy/WirelessPhy            ;# network interface type
set opt(mac)            Mac/802_11                 ;# MAC type
set opt(ifq)            Queue/DropTail/PriQueue    ;# interface queue type
set opt(ll)             LL                         ;# link layer type
set opt(ant)            Antenna/OmniAntenna        ;# antenna model
set opt(ifqlen)         50              	   ;# max packet in ifq
set opt(adhocRouting)   DSDV                       ;# routing protocol

set opt(x)		670			   ;# X dimension of the topography
set opt(y)		670			   ;# Y dimension of the topography

Mac/802_11 set basicRate_ 11Mb
Mac/802_11 set dataRate_ 11Mb
Mac/802_11 set bandwidth_ 11Mb
Mac/802_11 set RTSThreshold_  30000
Mac/802_11 set debug_ 1


#defines function for flushing and closing files
proc finish {} {
        global ns tf output_dir nb_mn
        $ns flush-trace
	close $tf
       	exit 
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
#puts "start hierarchical addressing"
$ns node-config -addressType hierarchical
AddrParams set domain_num_ 3          			;# domain number
lappend cluster_num 1 1 1           			;# cluster number for each domain 
AddrParams set cluster_num_ $cluster_num
lappend eilastlevel 1 2 2 ;# number of nodes for each cluster 
AddrParams set nodes_num_ $eilastlevel
puts "Configuration of hierarchical addressing done"

# Create God
create-god 4				;# nb_mn + 2 (base station and sink node)
#puts "God node created"

#creates the sink node in first addressing space.
set sinkNode [$ns node 0.0.0]
#provide some co-ord (fixed) to sink node
$sinkNode set X_ 50.0
$sinkNode set Y_ 10.0
$sinkNode set Z_ 0.0
#puts "sink node created"

#define coverage area for base station: 20m coverage
Phy/WirelessPhy set Pt_ 0.025
Phy/WirelessPhy set RXThresh_ 2.025e-12
Phy/WirelessPhy set CSThresh_ [expr 0.9*[Phy/WirelessPhy set RXThresh_]]

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
                 -agentTrace OFF \
                 -routerTrace OFF \
                 -macTrace ON  \
                 -movementTrace OFF
#puts "Configuration of base station"

set bstation [$ns node 1.0.0]  
$bstation random-motion 0
#provide some co-ord (fixed) to base station node
$bstation set X_ 50.0
$bstation set Y_ 50.0
$bstation set Z_ 0.0
set bstationMac [$bstation getMac 0]
set AP_ADDR_0 [$bstationMac id]
$bstationMac bss_id $AP_ADDR_0
$bstationMac set-channel $channelAP0
$bstationMac enable-beacon
[$bstation set netif_(0)] setTechno 802.11
if { $interference == 1 } {
    [$bstation set netif_(0)] enableInterference ;#this only needs to be done for 1 PHY
}
puts "Base-Station node AP0 created"

set bstation2 [$ns node 1.0.0]  
$bstation2 random-motion 0
#provide some co-ord (fixed) to base station node
$bstation2 set X_ 70.0
$bstation2 set Y_ 50.0
$bstation2 set Z_ 0.0
set bstationMac [$bstation2 getMac 0]
set AP_ADDR_1 [$bstationMac id]
$bstationMac bss_id $AP_ADDR_1
$bstationMac set-channel $channelAP1
$bstationMac enable-beacon
[$bstation2 set netif_(0)] setTechno 802.11
puts "Base-Station node AP1 created"

# creation of the mobile nodes
$ns node-config -wiredRouting OFF \
                -macTrace OFF  				;# Mobile nodes cannot do routing.
for {set i 0} {$i < $nb_mn} {incr i} {
	set wl_node_($i) [$ns node 1.0.[expr $i + 2]] 	;# create the node with given @.	
	$wl_node_($i) random-motion 0			;# disable random motion
#	puts "wireless node $i created ..."			;# debug info
    if { $i == 0 } {
	$wl_node_($i) base-station [AddrParams addr2id [$bstation node-addr]] ;#attach mn to basestation
	$wl_node_($i) set X_ 55.0
	$wl_node_($i) set Y_ 50.0
	$wl_node_($i) set Z_ 0.0
        [$wl_node_($i) set mac_(0)] set-channel $channelAP0
        [$wl_node_($i) set netif_(0)] setTechno 802.11
    } else {
	$wl_node_($i) base-station [AddrParams addr2id [$bstation2 node-addr]] ;#attach mn to basestation
	$wl_node_($i) set X_ [expr 60.0+$i]
	$wl_node_($i) set Y_ 50.0
	$wl_node_($i) set Z_ 0.0
        [$wl_node_($i) set mac_(0)] set-channel $channelAP1
        [$wl_node_($i) set netif_(0)] setTechno 802.11
    }

	#create source traffic
	#Create a UDP agent and attach it to node n0
	set udp_($i) [new Agent/UDP]
	$udp_($i) set packetSize_ 1500
	$ns attach-agent $wl_node_($i) $udp_($i)

	# Create a CBR traffic source and attach it to udp0
	set cbr_($i) [new Application/Traffic/CBR]
	$cbr_($i) set packetSize_ $packet_size
	$cbr_($i) set interval_ $gap_size
	$cbr_($i) attach-agent $udp_($i)

	#create an sink into the sink node

	# Create the Null agent to sink traffic
	set null_($i) [new Agent/Null] 
	$ns attach-agent $sinkNode $null_($i)
	
	# Attach the 2 agents
	$ns connect $udp_($i) $null_($i)
}

# create the link between sink node and base station
$ns duplex-link $sinkNode $bstation 100Mb 1ms DropTail
$ns duplex-link $sinkNode $bstation2 100Mb 1ms DropTail

# Traffic scenario: here the all start talking at the same time
for {set i 0} {$i < $nb_mn} {incr i} {
	$ns at $traffic_start "$cbr_($i) start"
	$ns at $traffic_stop "$cbr_($i) stop"
}


$ns at $simulation_stop "finish"
#$ns at $simulation_stop "$ns halt"
# Run the simulation
puts "Running simulation for $nb_mn mobile nodes..."
$ns run
puts "Simulation done."
