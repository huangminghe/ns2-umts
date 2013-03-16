#
#
#
#
#                   CN  0.0.0
#                   |
#                   R1  1.0.0
#                   |
#                   |
#         +-----+------+------+
#         |     |      |      |
#         BS1   AP1   BS2    AP2
#
#
#         2.0.0 4.0.0 3.0.0 5.0.0
#
#        
#         MN1----------------->
#         (WIFI 4.0.1, WIMAX 2.0.2)
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


#define MAC 802_11 parameters
Mac/802_11 set bss_timeout_ 5
Mac/802_11 set pr_limit_ 1.2 ;#for link going down
Mac/802_11 set client_lifetime_ 40.0 ;# since we don't have traffic, the AP would disconnect the MN automatically after client_lifetime_
Mac/802_11 set basicRate_ 11Mb
Mac/802_11 set dataRate_ 11Mb
Mac/802_11 set bandwidth_ 11Mb

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
Mac/802_11 set debug_ 0
Agent/MIHUser/IFMNGMT/MIPV6 set debug_ 1
Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1 set debug_ 1

# Handover
#Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1 set case_ 1
Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1 set case_ 3

#define coverage area for base station: 
#default frequency 3.5e+9 hz 
Phy/WirelessPhy set Pt_ 15
Phy/WirelessPhy set RXThresh_  1.215e-09           ;#500m coverage
Phy/WirelessPhy set CSThresh_ [expr 0.8 *[Phy/WirelessPhy set RXThresh_]]

#node Position
set MN_80211_X 510.0
set MN_80211_Y 1000.0
set MN_80211_Z 0.0

set MN_80216_X 510.0
set MN_80216_Y 1000.0
set MN_80216_Z 0.0

set INIT_80216_X(0) 501.0
set INIT_80216_Y(0) 1000.0
set INIT_80216_Z(0) 0.0

set INIT_80216_X(1) 1251.0
set INIT_80216_Y(1) 1000.0
set INIT_80216_Z(1) 0.0

set BS_80216_X(0) 500.0
set BS_80216_Y(0) 1000.0
set BS_80216_Z(0) 0.0

set BS_80216_X(1) 1250.0
set BS_80216_Y(1) 1000.0
set BS_80216_Z(1) 0.0

set AP_80211_X(0) 610.0
set AP_80211_Y(0) 1000.0
set AP_80211_Z(0) 0.0

set AP_80211_X(1) 1160.0
set AP_80211_Y(1) 1000.0
set AP_80211_Z(1) 0.0

set X_dst $BS_80216_X(1)
set Y_dst $BS_80216_Y(1)

#Rate at which the nodes start moving
set moveStart 10.0
set moveStop 1500.0
#Speed of the mobile nodes (m/sec)
set moveSpeed 1


# Parameter for wireless nodes 802.16
set opt(chan)           Channel/WirelessChannel    ;# channel type
set opt(prop)           Propagation/TwoRayGround   ;# radio-propagation model
set opt(netif)          Phy/WirelessPhy/OFDM       ;# network interface type
set opt(mac)            Mac/802_16                 ;# MAC type
set opt(ifq)            Queue/DropTail/PriQueue    ;# interface queue type
set opt(ll)             LL                         ;# link layer type
set opt(ant)            Antenna/OmniAntenna        ;# antenna model
set opt(ifqlen)         50              	   ;# max packet in ifq
set opt(adhocRouting)   NOAH                       ;# routing protocol
set opt(nbMN)           2                          ;# number of Mobile Nodes
set opt(nbAP)           2                          ;# number of base stations
set opt(x)		3000			   ;# X dimension of the topography
set opt(y)		3000			   ;# Y dimension of the topography
set opt(mnSender)       1                          ;#
set opt(nbBS)           2                          ;# 

set quiet 0

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
lappend tmp 2                                      ;# 802.16 MNs+BS
lappend tmp 2                                      ;# 802.16 MNs+BS
lappend tmp 2                                      ;# 802.11 MNs+AP
lappend tmp 2                                      ;# 802.11 MNs+AP
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
create-god [expr ($opt(nbMN) + $opt(nbAP) + 1)]         ;

# Create multi-interface node

$ns node-config  -multiIf ON  

for {set i 0} {$i < $opt(nbBS)} {incr i} {
    set multiFaceNode_init($i) [$ns node  [expr 6 + $i].0.0]
}
set multiFaceNode_MN [$ns node 8.0.0]

$ns node-config  -multiIf OFF  

set sharedChannel [new $opt(chan)]

#creates the Access Point (Base station)
$ns node-config -adhocRouting $opt(adhocRouting) \
                 -llType $opt(ll) \
                 -macType $opt(mac) \
                 -ifqType $opt(ifq) \
                 -ifqLen $opt(ifqlen) \
                 -antType $opt(ant) \
                 -propType $opt(prop)    \
                 -phyType $opt(netif) \
                 -channel $sharedChannel \
                 -topoInstance $topo \
                 -wiredRouting ON \
                 -agentTrace ON \
                 -routerTrace ON \
                 -macTrace ON  \
                 -movementTrace OFF

for {set i 0} {$i < $opt(nbBS)} {incr i} {
    $ns node-config -wiredRouting ON \
	            -macTrace OFF  		
    # configure the Base station 802.16
    Mac/802_16 set debug_ 1
    set bstationWIMAX($i) [$ns node [expr 2+$i].0.0]  
    $bstationWIMAX($i) random-motion 0
    $bstationWIMAX($i) set X_ $BS_80216_X($i)
    $bstationWIMAX($i) set Y_ $BS_80216_Y($i)
    $bstationWIMAX($i) set Z_ $BS_80216_Z($i)

    set clas [new SDUClassifier/Dest]
    [$bstationWIMAX($i) set mac_(0)] add-classifier $clas
    #set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
    set bs_sched($i) [new WimaxScheduler/BS]
    $bs_sched($i) set-default-modulation $default_modulation
    $bs_sched($i) set-contention-size 5
    [$bstationWIMAX($i) set mac_(0)] set-scheduler $bs_sched($i)
    [$bstationWIMAX($i) set mac_(0)] set-channel $i 

    #add MOB_SCN handler
    set wimaxctrl($i) [new Agent/WimaxCtrl]
    $wimaxctrl($i) set-mac [$bstationWIMAX($i) set mac_(0)]
    $ns attach-agent $bstationWIMAX($i) $wimaxctrl($i)

    # add link to backbone
    $ns duplex-link $bstationWIMAX($i) $router1 100MBit 15ms DropTail 1000

    puts "BstationWIMAX: tcl=$bstationWIMAX($i); id=[$bstationWIMAX($i) id]; addr=[$bstationWIMAX($i) node-addr] X=$BS_80216_X($i) Y=$BS_80216_Y($i)"

    # creation of the mobile nodes
    $ns node-config -wiredRouting OFF \
	-macTrace ON  				                            ;# Mobile nodes cannot do routing.

    #create 1 node in each cell to init cells (only for 802.16)
    Mac/802_16 set debug_ 1
    set init_node($i) [$ns node [expr 2+$i].0.1] 
    $init_node($i) random-motion 0			                            ;# disable random motion
    $init_node($i) base-station [AddrParams addr2id [$bstationWIMAX($i) node-addr]] ;#attach mn to basestation
    $init_node($i) set X_ $INIT_80216_X($i)
    $init_node($i) set Y_ $INIT_80216_Y($i)
    $init_node($i) set Z_ $INIT_80216_Z($i)
    set clas_mn($i) [new SDUClassifier/Dest]
    [$init_node($i) set mac_(0)] add-classifier $clas_mn($i)
    #set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
    set ss_sched($i) [new WimaxScheduler/SS]
    [$init_node($i) set mac_(0)] set-scheduler $ss_sched($i)
    [$init_node($i) set mac_(0)] set-channel $i
    #add the interfaces to supernode
    $multiFaceNode_init($i) add-interface-node $init_node($i)
    puts "InitNode: tcl=$init_node($i); id=[$init_node($i) id]; addr=[$init_node($i) node-addr] X=$INIT_80216_X($i) Y=$INIT_80216_Y($i)"    
    #configure init node
    set nd_init_node($i) [$init_node($i) install-nd]
    set handover_init_node($i) [new Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1]
    $nd_init_node($i) set-ifmanager $handover_init_node($i)
    $multiFaceNode_init($i) install-ifmanager $handover_init_node($i)
    # install MIH in multi-interface node
    set mih_init_node($i) [$multiFaceNode_init($i) install-mih]
    set nd_init_node($i) [$init_node($i) install-nd]
    $handover_init_node($i) connect-mih $mih_init_node($i)
    set tmp3 [$init_node($i) set mac_(0)]
    $handover_init_node($i) nd_mac $nd_init_node($i) $tmp3
    $tmp3 mih $mih_init_node($i)
    $mih_init_node($i) add-mac $tmp3
}


$wimaxctrl(0) add-neighbor [$bstationWIMAX(1) set mac_(0)] $bstationWIMAX(1)
$wimaxctrl(1) add-neighbor [$bstationWIMAX(0) set mac_(0)] $bstationWIMAX(0)


#configure the MOBILE NODE
Mac/802_16 set debug_ 1
set 80216_node  [$ns node 2.0.2] 	                                   ;# create the node with given @.	
$80216_node random-motion 0			                           ;# disable random motion
$80216_node base-station [AddrParams addr2id [$bstationWIMAX(0) node-addr]]   ;# attach mn to basestation
$80216_node set X_ $MN_80216_X
$80216_node set Y_ $MN_80216_Y
$80216_node set Z_ $MN_80216_Z
set clas_80216 [new SDUClassifier/Dest]
[$80216_node set mac_(0)] add-classifier $clas_80216
#set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
set ss_sched_80216 [new WimaxScheduler/SS]
[$80216_node set mac_(0)] set-scheduler $ss_sched_80216
[$80216_node set mac_(0)] set-channel 0
$multiFaceNode_MN  add-interface-node $80216_node
puts "Mobile Node: tcl=$80216_node; id=[$80216_node id]; addr=[$80216_node node-addr] X=$MN_80216_X Y=$MN_80216_Y"
#configure 80216 node
set nd_80216_node [$80216_node install-nd]
set handover_MN [new Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1]
$nd_80216_node set-ifmanager $handover_MN
$multiFaceNode_MN install-ifmanager $handover_MN
# install MIH in multi-interface node
set mih_MN [$multiFaceNode_MN install-mih]
$handover_MN connect-mih $mih_MN
set tmp3 [$80216_node set mac_(0)]
$handover_MN nd_mac $nd_80216_node $tmp3
$tmp3 mih $mih_MN
$mih_MN add-mac $tmp3


for {set i 0} {$i < $opt(nbBS)} {incr i} {
    # configure each Base station 802.16
    set nd_bs_WIMAX($i) [$bstationWIMAX($i) install-nd]
    $nd_bs_WIMAX($i) set-router TRUE
    $nd_bs_WIMAX($i) router-lifetime 1800
    $ns at 1 "$nd_bs_WIMAX($i) start-ra"
    set mih_bs_WIMAX($i) [$bstationWIMAX($i) install-mih]
    set tmp_WIMAX($i) [$bstationWIMAX($i) set mac_(0)] ;#in 802.16 one interface is created
    $tmp_WIMAX($i) mih $mih_bs_WIMAX($i)
    $mih_bs_WIMAX($i) add-mac $tmp_WIMAX($i)
}

$ns node-config -wiredRouting ON \
                -macTrace OFF  	

#define coverage area for base station: 50m coverage 
Phy/WirelessPhy set Pt_ 0.0134
Phy/WirelessPhy set freq_ 2412e+6
Phy/WirelessPhy set RXThresh_ 5.25089e-10
Phy/WirelessPhy set CSThresh_ [expr 0.9* [Phy/WirelessPhy set RXThresh_]]

# parameter for wireless nodes 802.11
set opt(chan)           Channel/WirelessChannel    ;# channel type for 802.11
set opt(prop)           Propagation/TwoRayGround   ;# radio-propagation model 802.11
set opt(netif)          Phy/WirelessPhy            ;# network interface type 802.11
set opt(mac)            Mac/802_11                 ;# MAC type 802.11
set opt(ifq)            Queue/DropTail/PriQueue    ;# interface queue type 802.11
set opt(ll)             LL                         ;# link layer type 802.11
set opt(ant)            Antenna/OmniAntenna        ;# antenna model 802.11
set opt(ifqlen)         50              	   ;# max packet in ifq 802.11
set opt(adhocRouting)   NOAH                       ;# routing protocol 802.11
set opt(umtsRouting)    ""                         ;# routing for UMTS (to reset node config)
set opt(nbBS)           3
set opt(x)		400			   ;# X dimension of the topography
set opt(y)		400			   ;# Y dimension of the topography
set opt(interference)   0                          ;# Disable/Enable interference
set opt(nbMN)           1
set opt(mnSender)       1                          ;# 1 if MN is sender, 0 if receiver

#creates the Access Point (Base station)
$ns node-config -adhocRouting $opt(adhocRouting) \
                 -llType $opt(ll) \
                 -macType $opt(mac) \
                 -ifqType $opt(ifq) \
                 -ifqLen $opt(ifqlen) \
                 -antType $opt(ant) \
                 -propType $opt(prop)    \
                 -phyType $opt(netif) \
                 -channel $sharedChannel \
                 -topoInstance $topo \
                 -wiredRouting ON \
                 -agentTrace ON \
                 -routerTrace ON \
                 -macTrace ON  \
                 -movementTrace OFF

for {set i 0} {$i < $opt(nbAP)} {incr i} {
    # configure the Base station 802.11
    set bstation80211($i) [$ns node  [expr 4 + $i].0.0]
    $bstation80211($i) set X_ $AP_80211_X($i)
    $bstation80211($i) set Y_ $AP_80211_Y($i)
    $bstation80211($i) set Z_ $AP_80211_Z($i)
    if {$quiet == 0} {
	puts "bstation80211: tcl=$bstation80211($i); id=[$bstation80211($i) id]; addr=[$bstation80211($i) node-addr] X=$AP_80211_X($i) Y=$AP_80211_Y($i)"
    }
    # we need to set the BSS for the base station
    set bstationMac($i) [$bstation80211($i) getMac 0]
    $bstationMac($i) set-channel [expr 2 + $i]
    set AP_ADDR($i) [$bstationMac($i) id]
    if {$quiet == 0} {
	puts "bss_id for bstation=$AP_ADDR($i) channel=[expr ($i+2)]"
    }
    $bstationMac($i) bss_id $AP_ADDR($i)
    $bstationMac($i) enable-beacon

    if { $opt(interference) == 1} {
	set bstationPhy($i) [$bstation80211($i) set netif_(0)]
	$bstationPhy($i) enableInterference
	$bstationPhy($i) setTechno 802.11
    }
}

# add link to backbone
for {set i 0} {$i < $opt(nbAP) } {incr i} {
    $ns duplex-link $bstation80211($i) $router1 100MBit 15ms DropTail 1000
}

$ns node-config -wiredRouting OFF \
                -macTrace ON  	

# configure WLAN interface of each Base Satation
for {set i 0} {$i < $opt(nbAP) } {incr i} {
    set nd_bs_WIFI($i) [$bstation80211($i) install-nd]
    $nd_bs_WIFI($i) set-router TRUE
    $nd_bs_WIFI($i) router-lifetime 18
    $ns at 1 "$nd_bs_WIFI($i) start-ra"
    set mih_bs_WIFI($i) [$bstation80211($i) install-mih]
    set tmp2($i) [$bstation80211($i) set mac_(0)] ;#in 802.11 one interface is created
    $tmp2($i) mih $mih_bs_WIFI($i)
    $mih_bs_WIFI($i) add-mac $tmp2($i)
}

set 80211_node [$ns node 4.0.1]                                             ;#node id is 5
$80211_node random-motion 0		                                    ;#disable random motion
$80211_node base-station [AddrParams addr2id [$bstation80211(0) node-addr]] ;#attach mn to basestation
$80211_node set X_ $MN_80211_X
$80211_node set Y_ $MN_80211_Y
$80211_node set Z_ $MN_80211_Z
[$80211_node getMac 0] set-channel 2

if { $opt(interference) == 1} {
    set MNPhy($i) [$80211_node set netif_(0)]
    $MNPhy($i) enableInterference
    $MNPhy($i) setTechno 802.11
}

if {$quiet == 0} {
    puts "MN 802.11: tcl=$80211_node; id=[$80211_node id]; addr=[$80211_node node-addr] X=$MN_80211_X Y=$MN_80211_Y"
}

#add the interfaces to supernode
$multiFaceNode_MN add-interface-node $80211_node
set nd_80211_node [$80211_node install-nd]
$nd_80211_node set-ifmanager $handover_MN

# install MIH in multi-interface node
#set mih_80211_node [$multiFaceNode_MN install-mih]
#$handover_MN connect-mih $mih_80211_node
set tmp3 [$80211_node set mac_(0)]
$handover_MN nd_mac $nd_80211_node $tmp3
$tmp3 mih $mih_MN
$mih_MN add-mac $tmp3

#Create a UDP agent and attach it to node n0
set udp [new Agent/UDP]
$udp set packetSize_ 1500

if {$quiet == 0} {
    puts "udp on node : $udp"
}

# Create a CBR traffic source and attach it to udp0
set cbr [new Application/Traffic/CBR]
$cbr set packetSize_ 500
$cbr set interval_ 0.5
$cbr attach-agent $udp

#create an sink into the sink node
# Create the Null agent to sink traffic
set null [new Agent/Null] 

if { $opt(mnSender) == 1} {
    #CN is receiver    
    $ns attach-agent $CN $null
    
    #Multiface node is transmitter
    $multiFaceNode_MN attach-agent $udp $80211_node
    $handover_MN add-flow $udp $null $80211_node 1 2000.    
} else {
    #Multiface node is receiver    
    $multiFaceNode_MN attach-agent $null $80211_node
    $handover_MN add-flow $null $udp $80211_node 1 2000.    
    
    #CN is transmitter
    $ns attach-agent $CN $udp
}

#Start the application 1sec before the MN is entering the WLAN cell
$ns at [expr $moveStart - 1] "$cbr start"

#Stop the application according to another poisson distribution (note that we don't leave the 802.11 cell)
$ns at [expr $moveStop  + 1] "$cbr stop"

#calculate the speed of the node
$ns at $moveStart "$80211_node setdest $X_dst $Y_dst $moveSpeed"
$ns at $moveStart "$80216_node setdest $X_dst $Y_dst $moveSpeed"
$ns at $moveStop "finish"
puts "Running simulation for $opt(nbMN) mobile nodes..."
$ns run
puts "Simulation done."
