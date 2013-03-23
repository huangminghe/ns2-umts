# Test for 802.16 nodes.
# @author rouil
# @date 01/20/2006
# Test file for wimax
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

#check input parameters
if {$argc < 4} {
    puts ""
    puts "Wrong Number of Arguments! No arguments in this topology"
    puts "ns handover2.tcl seed scan_iteration going_down other_nodes"
    puts ""
    exit 1
}

#read arguments
set seed                             [lindex $argv 0]
Mac/802_16 set scan_iteration_       [lindex $argv 1]
set use_going_down                   [lindex $argv 2]
set other_node                       [lindex $argv 3] 

if {$use_going_down == 1} {
    Mac/802_16 set lgd_factor_           1.1
} else {
    Mac/802_16 set lgd_factor_           1.0
}
Mac/802_16 set scan_duration_        50
Mac/802_16 set interleaving_interval_ 40
Agent/WimaxCtrl set adv_interval_ 1.0
Agent/WimaxCtrl set default_association_level_ 0
Agent/WimaxCtrl set synch_frame_delay_ 0.5
Agent/WimaxCtrl set debug_ 1


set simulation_time                  200 ;#30
set nb_mn                            1
Mac/802_16 set dcd_interval_         5 ;#max 10s
Mac/802_16 set ucd_interval_         5 ;#max 10s
set default_modulation               OFDM_16QAM_3_4 ;#OFDM_BPSK_1_2
set contention_size                  5 ;#for initial ranging and bw  
Mac/802_16 set t21_timeout_          0.02 ;#max 10s, to replace the timer for looking at preamble 
Mac/802_16 set client_timeout_       50 

#random used in movement of MN
set move [new RandomVariable/Uniform]
$move set min_ 0
$move set max_ 10


# set global variables
set packet_size	250			;# packet size in bytes at CBR applications 
set gap_size 0.05 ;#compute gap size between packets
set output_dir .
set traffic_start 10
set traffic_stop  150

set simulation_stop $simulation_time

set use_voice 0
set use_video 1

#define debug values
Agent/ND set debug_ 1
Agent/MIH set debug_ 1
Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1 set debug_ 1
Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1 set case_ 3
Agent/MIHUser/IFMNGMT/MIPV6 set debug_ 1
Mac/802_16 set debug_ 1

#define coverage area for base station: 
Phy/WirelessPhy set Pt_ 0.025
#Phy/WirelessPhy set RXThresh_ 6.12277e-09 ;#20m coverage 
#Phy/WirelessPhy set RXThresh_ 2.90781e-09 ;#500m coverage
Phy/WirelessPhy set RXThresh_ 1.26562e-13 ;#1000m radius
Phy/WirelessPhy set CSThresh_ [expr 0.8 *[Phy/WirelessPhy set RXThresh_]]

# Parameter for wireless nodes
set opt(chan)           Channel/WirelessChannel    ;# channel type
set opt(prop)           Propagation/TwoRayGround   ;# radio-propagation model
set opt(netif)          Phy/WirelessPhy/OFDM       ;# network interface type
set opt(mac)            Mac/802_16                 ;# MAC type
set opt(ifq)            Queue/DropTail/PriQueue    ;# interface queue type
set opt(ll)             LL                         ;# link layer type
set opt(ant)            Antenna/OmniAntenna        ;# antenna model
set opt(ifqlen)         50              	   ;# max packet in ifq
set opt(adhocRouting)   NOAH                       ;# routing protocol

set opt(x)		4000			   ;# X dimension of the topography
set opt(y)		2000			   ;# Y dimension of the topography

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
#puts "start hierarchical addressing"
$ns node-config -addressType hierarchical
AddrParams set domain_num_ 3          			;# domain number
lappend cluster_num 1 1 1          			;# cluster number for each domain 
AddrParams set cluster_num_ $cluster_num
lappend eilastlevel 2 [expr ($nb_mn+2)] [expr ($nb_mn+2)];# number of nodes for each cluster (1 for sink and one for mobile nodes + base station
AddrParams set nodes_num_ $eilastlevel
puts "Configuration of hierarchical addressing done"

# Create God
create-god [expr ($nb_mn + 2)]				;# nb_mn + 2 (base station and sink node)
#puts "God node created"

#creates the sink node in first addressing space.
set sinkNode [$ns node 0.0.0]
#provide some co-ord (fixed) to base station node
$sinkNode set X_ 50.0
$sinkNode set Y_ 10.0
$sinkNode set Z_ 0.0
#puts "sink node created"

set router [$ns node 0.0.1]

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

Mac/802_16 set debug_ 1
set bstation [$ns node 1.0.0]  
$bstation random-motion 0
#puts "Base-Station node created"
#provide some co-ord (fixed) to base station node
$bstation set X_ 1100.0
$bstation set Y_ 1000.0
$bstation set Z_ 0.0
set clas [new SDUClassifier/Dest]
[$bstation set mac_(0)] add-classifier $clas
#set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
set bs_sched [new WimaxScheduler/BS]
$bs_sched set-default-modulation $default_modulation
$bs_sched set-contention-size 5
[$bstation set mac_(0)] set-scheduler $bs_sched
[$bstation set mac_(0)] set-channel 0
set wimaxctrl [new Agent/WimaxCtrl]
$wimaxctrl set-mac [$bstation set mac_(0)]
$ns attach-agent $bstation $wimaxctrl


Mac/802_16 set debug_ 1
set bstation2 [$ns node 2.0.0]  
$bstation2 random-motion 0
puts "Base-Station node created"
#provide some co-ord (fixed) to base station node
$bstation2 set X_ 3000.0 
$bstation2 set Y_ 1000.0
$bstation2 set Z_ 0.0
set clas2 [new SDUClassifier/Dest]
[$bstation2 set mac_(0)] add-classifier $clas2
#set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
set bs_sched2 [new WimaxScheduler/BS]
$bs_sched2 set-default-modulation $default_modulation
#$bs_sched2 set-contention-size [expr $contention_size]
[$bstation2 set mac_(0)] set-scheduler $bs_sched2
[$bstation2 set mac_(0)] set-channel 2
set wimaxctrl2 [new Agent/WimaxCtrl]
$wimaxctrl2 set-mac [$bstation2 set mac_(0)]
$ns attach-agent $bstation2 $wimaxctrl2

#Add neighbor information to the BSs
$wimaxctrl add-neighbor [$bstation2 set mac_(0)] $bstation2
$wimaxctrl2 add-neighbor [$bstation set mac_(0)] $bstation


# creation of the mobile nodes
$ns node-config -wiredRouting OFF \
                -macTrace ON  				;# Mobile nodes cannot do routing.

#create 1 node in each cell to init cells
if {$other_node == 1} {
Mac/802_16 set debug_ 1
set m_node_(0) [$ns node 1.0.1] 	;# create the node with given @.	
$m_node_(0) random-motion 0			;# disable random motion
$m_node_(0) base-station [AddrParams addr2id [$bstation node-addr]] ;#attach mn to basestation
$m_node_(0) set X_ [expr 1000.0]
$m_node_(0) set Y_ 1000.0
$m_node_(0) set Z_ 0.0
set clas [new SDUClassifier/Dest]
[$m_node_(0) set mac_(0)] add-classifier $clas
#set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
set ss_sched [new WimaxScheduler/SS]
[$m_node_(0) set mac_(0)] set-scheduler $ss_sched
[$m_node_(0) set mac_(0)] set-channel 0
set nd_mn [$m_node_(0) install-nd]

Mac/802_16 set debug_ 1
set m_node_(1) [$ns node 2.0.1] 	;# create the node with given @.	
$m_node_(1) random-motion 0			;# disable random motion
$m_node_(1) base-station [AddrParams addr2id [$bstation2 node-addr]] ;#attach mn to basestation
$m_node_(1) set X_ [expr 3100.0]
$m_node_(1) set Y_ 1000.0
$m_node_(1) set Z_ 0.0
set clas [new SDUClassifier/Dest]
[$m_node_(1) set mac_(0)] add-classifier $clas
#set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
set ss_sched [new WimaxScheduler/SS]
[$m_node_(1) set mac_(0)] set-scheduler $ss_sched
[$m_node_(1) set mac_(0)] set-channel 2
set nd_mn [$m_node_(1) install-nd]
}

Mac/802_16 set debug_ 1
for {set i 0} {$i < $nb_mn} {incr i} {
    set wl_node_($i) [$ns node 1.0.[expr $i + 2]] 	;# create the node with given @.	
    $wl_node_($i) random-motion 0			;# disable random motion
    $wl_node_($i) base-station [AddrParams addr2id [$bstation node-addr]] ;#attach mn to basestation
    #compute position of the nod
    if {$i == 0} {
	set tmp [$move value]
        for {set j 0} {$j < $seed} {incr j} {
            set tmp [$move value]
        }
        #puts "start at $tmp"

	$wl_node_($i) set X_ [expr 1900.0]
	$ns at $tmp "$wl_node_($i) setdest 2200.0 1000.0 2.0" ;#100 to get out of cell 2
    } else {
	$wl_node_($i) set X_ [expr 55.0]
    }
    $wl_node_($i) set Y_ 1000.0
    $wl_node_($i) set Z_ 0.0
    puts "wireless node $i created ..."			;# debug info
  
    set clas [new SDUClassifier/Dest]
    [$wl_node_($i) set mac_(0)] add-classifier $clas
    #set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
    set ss_sched [new WimaxScheduler/SS]
    [$wl_node_($i) set mac_(0)] set-scheduler $ss_sched
    [$wl_node_($i) set mac_(0)] set-channel 0
    if {$i == 0} {
	#$ns at $scan_time "$ss_sched send-scan"
    }

    set nd_mn [$wl_node_($i) install-nd]
    # install interface manager into multi-interface node and CN
    set handover [new Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1]
    $wl_node_($i) install-ifmanager $handover
    $nd_mn set-ifmanager $handover
    $nd_mn set-ifmanager $handover
    $handover nd_mac $nd_mn [$wl_node_($i) set mac_(0)] ;#to know how to send RS

    # install MIH in mobile node
    set mih [$wl_node_($i) install-mih]
    
    $handover connect-mih $mih ;#create connection between MIH and iface management

    set tmp2 [$wl_node_($i) set mac_(0)] 
    $tmp2 mih $mih
    $mih add-mac $tmp2  

    ##configure traffic
    set udpvideo_($i) [new Agent/UDP]
    $udpvideo_($i) set packetSize_ 1240

    set udpvoice1_($i) [new Agent/UDP]
    $udpvoice1_($i) set packetSize_ 1500
    set udpvoice2_($i) [new Agent/UDP]
    $udpvoice2_($i) set packetSize_ 1500
    
    #create video traffic
    set cbrvideo_($i) [new Application/Traffic/CBR]
    $cbrvideo_($i) set packetSize_ 4960
    $cbrvideo_($i) set interval_ 0.1
    $cbrvideo_($i) attach-agent $udpvideo_($i)
    set nullvideo_($i) [new Agent/Null]

    #sinkNode is transmitter    
    $ns attach-agent $sinkNode $udpvideo_($i)
    $ns attach-agent $wl_node_($i) $nullvideo_($i)

if {$use_video == 1} {
    $handover add-flow $nullvideo_($i) $udpvideo_($i) $wl_node_($i) 1 20.
}
    #create voice traffic
    set cbrvoice1_($i) [new Application/Traffic/CBR]
    $cbrvoice1_($i) set packetSize_ 160
    $cbrvoice1_($i) set interval_ 0.02
    $cbrvoice1_($i) attach-agent $udpvoice1_($i)
    set nullvoice1_($i) [new Agent/Null]
    $ns attach-agent $sinkNode $udpvoice1_($i)
    $ns attach-agent $wl_node_($i) $nullvoice1_($i)
#    $ifmgmt add-flow $nullvoice1_($i) $udpvoice1_($i) $wl_node_($i) 1 ;#2000.
    
    #second voice flow
    set cbrvoice2_($i) [new Application/Traffic/CBR]
    $cbrvoice2_($i) set packetSize_ 160
    $cbrvoice2_($i) set interval_ 0.02
    $cbrvoice2_($i) attach-agent $udpvoice2_($i)
    set nullvoice2_($i) [new Agent/Null]
    $ns attach-agent $sinkNode $nullvoice2_($i)
    $ns attach-agent $wl_node_($i) $udpvoice2_($i)
if {$use_voice == 1} {
    $handover add-flow $udpvoice2_($i) $nullvoice2_($i) $wl_node_($i) 1 ;#2000.
}

}

# install MIH in BS
set mih [$bstation install-mih]
set tmp_bs [$bstation set mac_(0)]
$tmp_bs mih $mih
$mih add-mac $tmp_bs

set mih [$bstation2 install-mih]
set tmp_bs [$bstation2 set mac_(0)]
$tmp_bs mih $mih
$mih add-mac $tmp_bs

# create the link between sink node and base station
$ns duplex-link $sinkNode $router 100Mb 30ms DropTail
$ns duplex-link $router $bstation 100Mb 15ms DropTail
$ns duplex-link $router $bstation2 100Mb 15ms DropTail

# ND in wireless lan
set nd_bs [$bstation install-nd]
$nd_bs set-router TRUE
$nd_bs router-lifetime 1800
$ns at 1 "$nd_bs start-ra"
set nd_bs [$bstation2 install-nd]
$nd_bs set-router TRUE
$nd_bs router-lifetime 1800
$ns at 1 "$nd_bs start-ra"

# Interface manager in sink node
set ifmgmt_cn [$sinkNode install-default-ifmanager]

# Traffic scenario: here the all start talking at the same time
for {set i 0} {$i < $nb_mn} {incr i} {
if {$use_video == 1} {
    $ns at $traffic_start "$cbrvideo_($i) start"
    $ns at $traffic_stop "$cbrvideo_($i) stop"
}
if {$use_voice == 1} {
    $ns at $traffic_start "$cbrvoice1_($i) start"
    $ns at $traffic_stop "$cbrvoice1_($i) stop"
    $ns at $traffic_start "$cbrvoice2_($i) start"
    $ns at $traffic_stop "$cbrvoice2_($i) stop"
}
}

$ns at $simulation_stop "finish"

puts "Running simulation for $nb_mn mobile nodes..."
$ns run
puts "Simulation done."
