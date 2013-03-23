#
#
#
#
#                      CN  0.0.0
#                      |
#                      R1  1.0.0
#                     /|\
#                    / | \
#                   /  |  \ 
#                  R2  R3  R4
#
#               2.0.0 3.0.0 4.0.0       
#
#             MN1-------------->
#              2.0.1   
#

#read arguments
set seed                             5555
Mac/802_16 set scan_iteration_       2
set use_going_down                   1

if {$use_going_down == 1} {
    Mac/802_16 set lgd_factor_        1.1
} else {
    Mac/802_16 set lgd_factor_        1.0
}
Mac/802_16 set scan_duration_         50
Mac/802_16 set interleaving_interval_ 40

Mac/802_16 set dcd_interval_         5 ;#max 10s
Mac/802_16 set ucd_interval_         5 ;#max 10s
set default_modulation               OFDM_16QAM_3_4 ;#OFDM_BPSK_1_2
set contention_size                  5 ;#for initial ranging and bw  
Mac/802_16 set t21_timeout_          0.02 ;#max 10s, to replace the timer for looking at preamble 
Mac/802_16 set client_timeout_       50 

#define frequency of RA at base station
Agent/ND set minRtrAdvInterval_ 1
Agent/ND set maxRtrAdvInterval_ 10
Agent/ND set router_lifetime_   1800
Agent/ND set minDelayBetweenRA_ 0.03
Agent/ND set maxRADelay_        0

#define debug values
Agent/ND set debug_ 1
Agent/ND set send-RS 1
Agent/MIH set debug_ 1
Mac/802_16 set debug_ 0
Agent/MIHUser/IFMNGMT/MIPV6 set debug_ 1
Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1 set debug_ 1

# Handover
#Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1 set case_ 2
Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1 set case_ 3

#define coverage area for base station: 
#default frequency 3.5e+9 hz 
Phy/WirelessPhy set Pt_ 15
Phy/WirelessPhy set RXThresh_  1.215e-09           ;#500m coverage
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
set opt(nbMN)           1                          ;# number of Mobile Nodes
set opt(nbBS)           3                          ;# number of base stations
set opt(x)		3000			   ;# X dimension of the topography
set opt(y)		3000			   ;# Y dimension of the topography
set opt(mnSender)       1


#Rate at which the nodes start moving
set moveStart 10.0
set moveStop 2000.0
#Speed of the mobile nodes (m/sec)
set moveSpeed 1

#origin of the MN
set X_dst 2000.0
set Y_dst 1000.0

#defines function for flushing and closing files
proc finish {} {
        global ns tf
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

#open file for trace
set tf [open out.res w]
$ns trace-all $tf


# set up for hierarchical routing (needed for routing over a basestation)
$ns node-config -addressType hierarchical
AddrParams set domain_num_  9                      ;# domain number
AddrParams set cluster_num_ {1 1 1 1 1 1 1 1 1}    ;# cluster number for each domain 
lappend tmp 1                                      ;# router CN
lappend tmp 1                                      ;# router 1
lappend tmp 3                                      ;# 802.16 MNs+BS
lappend tmp 3                                      ;# 802.16 MNs+BS
lappend tmp 3                                      ;# 802.16 MNs+BS
lappend tmp 1                                      ;# MultifaceNode
lappend tmp 1                                      ;# MultifaceNode
lappend tmp 1                                      ;# MultifaceNode
lappend tmp 1                                      ;# MultifaceNode
AddrParams set nodes_num_ $tmp

# Node address for router0 and router1 are 4 and 5, respectively.
set CN [$ns node 0.0.0]
$CN install-default-ifmanager
set router1 [$ns node 1.0.0]
 
# connect links 
$ns duplex-link $router1 $CN 100MBit 30ms DropTail 1000


# Create God
create-god [expr ($opt(nbMN) + $opt(nbBS))]         ;

# Create multi-interface node

$ns node-config  -multiIf ON  
for {set i 0} {$i < $opt(nbBS)} {incr i} {
    set multiFaceNode($i) [$ns node [expr 5+$i].0.0]
}

set multiFaceNode_wl [$ns node [expr 5+$opt(nbBS)].0.0]

$ns node-config  -multiIf OFF  


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


# configure each Base station 802.16
for {set i 0} {$i < $opt(nbBS) } {incr i} {
    Mac/802_16 set debug_ 1
    set bstation($i) [$ns node [expr 2+$i].0.0]  
    $bstation($i) random-motion 0
    $bstation($i) set X_ [expr 500 + $i*750]
    $bstation($i) set Y_ 1000.0
    $bstation($i) set Z_ 0.0

    set clas($i) [new SDUClassifier/Dest]
    [$bstation($i) set mac_(0)] add-classifier $clas($i)
    #set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
    set bs_sched($i) [new WimaxScheduler/BS]
    $bs_sched($i) set-default-modulation $default_modulation
    $bs_sched($i) set-contention-size 5
    [$bstation($i) set mac_(0)] set-scheduler $bs_sched($i)
    [$bstation($i) set mac_(0)] set-channel [expr $i*2]

    #add MOB_SCN handler
    set wimaxctrl($i) [new Agent/WimaxCtrl]
    $wimaxctrl($i) set-mac [$bstation($i) set mac_(0)]
    $ns attach-agent $bstation($i) $wimaxctrl($i)

    puts "Bstation: tcl=$bstation($i); id=[$bstation($i) id]; addr=[$bstation($i) node-addr] X=[expr 500.0 + $i*750.0] Y=1000.0"
}

$wimaxctrl(0) add-neighbor [$bstation(1) set mac_(0)] $bstation(1)
$wimaxctrl(1) add-neighbor [$bstation(2) set mac_(0)] $bstation(2)
$wimaxctrl(1) add-neighbor [$bstation(0) set mac_(0)] $bstation(0)
$wimaxctrl(2) add-neighbor [$bstation(1) set mac_(0)] $bstation(1)

# creation of the mobile nodes
$ns node-config -wiredRouting OFF \
                -macTrace ON  				                     ;# Mobile nodes cannot do routing.

for {set i 0} {$i < $opt(nbBS)} {incr i} {
    #create 1 node in each cell to init cells
    Mac/802_16 set debug_ 1
    set m_node_($i) [$ns node [expr 2+$i].0.1] 
    $m_node_($i) random-motion 0			                     ;# disable random motion
    $m_node_($i) base-station [AddrParams addr2id [$bstation($i) node-addr]] ;#attach mn to basestation
    $m_node_($i) set X_ [expr 500.0 + $i*750.0]
    $m_node_($i) set Y_ 1000.0
    $m_node_($i) set Z_ 0.0
    set clas_mn($i) [new SDUClassifier/Dest]
    [$m_node_($i) set mac_(0)] add-classifier $clas_mn($i)
    #set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
    set ss_sched($i) [new WimaxScheduler/SS]
    [$m_node_($i) set mac_(0)] set-scheduler $ss_sched($i)
    [$m_node_($i) set mac_(0)] set-channel [expr $i*2]
    
    #add the interfaces to supernode
    $multiFaceNode($i) add-interface-node $m_node_($i)
    
    puts "InitNode: tcl=$m_node_($i); id=[$m_node_($i) id]; addr=[$m_node_($i) node-addr] X=[expr 500.0 + $i*750.0] Y=1000.0"    
}

#configure the MOBILE NODE
Mac/802_16 set debug_ 1
set wl_node  [$ns node 2.0.2] 	                                      ;# create the node with given @.	
$wl_node random-motion 0			                      ;# disable random motion
$wl_node base-station [AddrParams addr2id [$bstation(0) node-addr]]   ;# attach mn to basestation
$wl_node set X_ 500.0
$wl_node set Y_ 1000.0 
$wl_node set Z_ 0.0
set clas_wl [new SDUClassifier/Dest]
[$wl_node set mac_(0)] add-classifier $clas_wl
#set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
set ss_sched_wl [new WimaxScheduler/SS]
[$wl_node set mac_(0)] set-scheduler $ss_sched_wl
[$wl_node set mac_(0)] set-channel 0
$multiFaceNode_wl  add-interface-node $wl_node
puts "Mobile Node: tcl=$wl_node; id=[$wl_node id]; addr=[$wl_node node-addr] X=500.0 Y=1000.0"


# add link to backbone
for {set i 0} {$i < $opt(nbBS) } {incr i} {
    # add link to backbone
    $ns duplex-link $bstation($i) $router1 100MBit 15ms DropTail 1000
}

# configure each Base station 802.16
for {set i 0} {$i < $opt(nbBS) } {incr i} {
    set nd_bs($i) [$bstation($i) install-nd]
    $nd_bs($i) set-router TRUE
    $nd_bs($i) router-lifetime 1800
    $ns at 1 "$nd_bs($i) start-ra"
    set mih_bs($i) [$bstation($i) install-mih]
    set tmp2($i) [$bstation($i) set mac_(0)] ;#in 802.16 one interface is created
    $tmp2($i) mih $mih_bs($i)
    $mih_bs($i) add-mac $tmp2($i)
}

# configure each MN 802.16
for {set i 0} {$i < $opt(nbBS)} {incr i} {
    set nd_mn($i) [$m_node_($i) install-nd]
    set handover($i) [new Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1]
    $nd_mn($i) set-ifmanager $handover($i)
    $multiFaceNode($i) install-ifmanager $handover($i)

    # install MIH in multi-interface node
    set mih($i) [$multiFaceNode($i) install-mih]
    set nd_mn($i) [$m_node_($i) install-nd]
    $handover($i) connect-mih $mih($i)
    set tmp3($i) [$m_node_($i) set mac_(0)]
    $handover($i) nd_mac $nd_mn($i) $tmp3($i)
    $tmp3($i) mih $mih($i)
    $mih($i) add-mac $tmp3($i)
}

set nd_mn_wl [$wl_node install-nd]
set handover_wl [new Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1]
$multiFaceNode_wl install-ifmanager $handover_wl
$nd_mn_wl set-ifmanager $handover_wl

# install MIH in multi-interface node
set mih_wl [$multiFaceNode_wl install-mih]
$handover_wl connect-mih $mih_wl
set tmp_wl [$wl_node set mac_(0)]
$handover_wl nd_mac $nd_mn_wl $tmp_wl
$tmp_wl mih $mih_wl
$mih_wl add-mac $tmp_wl

#Create a UDP agent and attach it to node n0
set udp_ [new Agent/UDP]
$udp_ set packetSize_ 1500

set quiet 0

if {$quiet == 0} {
    puts "udp on node : $udp_"
}


# Create a CBR traffic source and attach it to udp0
set cbr_ [new Application/Traffic/CBR]
$cbr_ set packetSize_ 1500
$cbr_ set interval_ 0.05
$cbr_ attach-agent $udp_

# Create the Null agent to sink traffic
set null_ [new Agent/Null] 

if { $opt(mnSender) == 1} {
    #CN is receiver    
    $ns attach-agent $CN $null_
    
    #Multiface node is transmitter
    $multiFaceNode_wl attach-agent $udp_ $wl_node
    $handover_wl add-flow $udp_ $null_ $wl_node 1 2000.    
} else {
    #Multiface node is receiver    
    $multiFaceNode_wl attach-agent $null_ $wl_node
    $handover_wl add-flow $null_ $udp_ $wl_node 1 2000.    
    
    #CN is transmitter
    $ns attach-agent $CN $udp_ 
}

#Start the application 1sec before the MN is entering the WLAN cell
$ns at [expr $moveStart - 1] "$cbr_ start"

#Stop the application according to another poisson distribution (note that we don't leave the 802.11 cell)
$ns at [expr $moveStop  + 1] "$cbr_ stop"

#calculate the speed of the node
$ns at $moveStart "$wl_node setdest $X_dst $Y_dst $moveSpeed"
$ns at $moveStop "finish"
puts "Running simulation for $opt(nbMN) mobile nodes..."
$ns run
puts "Simulation done."
