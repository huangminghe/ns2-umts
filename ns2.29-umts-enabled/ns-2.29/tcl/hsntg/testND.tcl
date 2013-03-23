# Test for Neighbor discovery protocol.
# @author rouil
# @date 09/13/2005.
# Scenario: Create a multi-interface node using different technologies
#           Configure the ND protocol in different node and test them.

#
# Topology scenario:
#
#   host0(1.0.1)                    bstation802.16(4.0.0)->)
#      |                           /
#      |                          /
# router0(1.0.0)---router1(2.0.0)----bstation802.11(3.0.0)->)    +-------------------------------------+
#                                \                               + iface2:802.16(4.0.1)|               |
#                                 \                              +---------------------+ MultiFaceNode |
#                                  \                             + iface1:802.11(3.0.1)|    (5.0.0)    |
#                                   \                            +---------------------+               |
#                                   rnc(0.0.0)                   + iface0:UMTS(0.0.2)  |               |
#                                      |                         +-------------------------------------+
#                                 bstationUMTS(0.0.1)->)                  
#                                                                
#              

#check input parameters
if {$argc != 0} {
	puts ""
	puts "Wrong Number of Arguments! No arguments in this topology"
	puts ""
	exit (1)
}

global ns

#set debug output for components
Agent/ND set debug_ 1
Mac/802_11 set debug_ 0
Mac/802_16 set debug_ 0

#defines ND attributes to fit the example
Agent/ND set maxRtrAdvInterval_ 1.0
Agent/ND set minRtrAdvInterval_ 0.5
Agent/ND set minDelayBetweenRA_ 0.5

#defines function for flushing and closing files
proc finish {} {
    global ns f
    $ns flush-trace
    close $f
    puts " Simulation ended."
    exit 0
}

# set global variables
set output_dir .

#create the simulator
set ns [new Simulator]
$ns use-newtrace

#open file for trace
set f [open out_testND.res w]
$ns trace-all $f

# set up for hierarchical routing (needed for routing over a basestation)
$ns node-config -addressType hierarchical
AddrParams set domain_num_  6                      ;# domain number
AddrParams set cluster_num_ {1 1 1 1 1 1}          ;# cluster number for each domain 
AddrParams set nodes_num_   {3 2 1 2 2 1}          ;# number of nodes for each cluster             

# configure RNC node
$ns node-config -UmtsNodeType rnc 
set rnc [$ns create-Umtsnode 0.0.0] ;# node id is 0.
puts "rnc $rnc"

# configure UMTS base station
$ns node-config -UmtsNodeType bs \
		-downlinkBW 384kbs \
		-downlinkTTI 10ms \
		-uplinkBW 384kbs \
		-uplinkTTI 10ms \
     		-hs_downlinkTTI 2ms \
      		-hs_downlinkBW 384kbs 

set bsUMTS [$ns create-Umtsnode 0.0.1] ;# node id is 1
puts "bsUMTS $bsUMTS"

# connect RNC and base station
$ns setup-Iub $bsUMTS $rnc 622Mbit 622Mbit 15ms 15ms DummyDropTail 2000

$ns node-config -UmtsNodeType ue \
		-baseStation $bsUMTS \
		-radioNetworkController $rnc

set iface0 [$ns create-Umtsnode 0.0.2] ;# node id is 2
  puts "iface0 created $iface0" 

# Node address for router0 and router1 are 3 and 4, respectively.
set router0 [$ns node 1.0.0]
puts "router0 $router0"
set router1 [$ns node 2.0.0]
puts "router1 $router1"
set host0 [$ns node 1.0.1] ;# node id is 5
puts "host0 $host0"

$ns duplex-link $rnc $router1 622Mbit 0.4ms DropTail 1000
$ns duplex-link $router1 $router0 100MBit 5ms DropTail 1000
$ns duplex-link $router0 $host0 100MBit 5ms DropTail 1000
$rnc add-gateway $router1

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
set opt(channel)        [new $opt(chan)]           ;# channel to be shared by all wireless nodes

set opt(x)		600			   ;# X dimension of the topography
set opt(y)		600			   ;# Y dimension of the topography


#define coverage area for 802.11 stations: 50m coverage 
Phy/WirelessPhy set Pt_ 0.0134
Phy/WirelessPhy set freq_ 2412e+6
Phy/WirelessPhy set RXThresh_ 5.25089e-10

# configure rate for 802.11
Mac/802_11 set basicRate_ 11Mb
Mac/802_11 set dataRate_ 11Mb
Mac/802_11 set bandwidth_ 11Mb

#create the topography
set topo [new Topography]
$topo load_flatgrid $opt(x) $opt(y)
puts "Topology created"

# create God
create-god 13				                ;# give the number of nodes 

# creation of the MutiFaceNodes
$ns node-config  -multiIf ON                            ;#to create MultiFaceNode 
set multiFaceNode [$ns node 5.0.0]                      ;# node id is 6
$ns node-config  -multiIf OFF                           ;#reset attribute

# configure Access Points
$ns node-config  -adhocRouting $opt(adhocRouting) \
                 -llType $opt(ll) \
                 -macType $opt(mac) \
                 -channel $opt(channel) \
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
set bstation802_11 [$ns node 3.0.0] ;# node id is 7
$bstation802_11 set X_ 200.0
$bstation802_11 set Y_ 300.0
$bstation802_11 set Z_ 0.0
# we need to set the BSS for the base station
set bstationMac [$bstation802_11 getMac 0]
set AP_ADDR_0 [$bstationMac id]
$bstationMac bss_id $AP_ADDR_0
$bstationMac enable-beacon
$bstationMac set-channel 1

# creation of the wireless interface 802.11
$ns node-config -wiredRouting OFF \
                -macTrace ON 				
set iface1 [$ns node 3.0.1] 	                                   ;# node id is 8.	
$iface1 random-motion 0			                           ;# disable random motion
$iface1 base-station [AddrParams addr2id [$bstation802_11 node-addr]] ;#attach mn to basestation
$iface1 set X_ 100.0
$iface1 set Y_ 300.0
$iface1 set Z_ 0.0
[$iface1 getMac 0] set-channel 1
$ns at 8 "$iface1 setdest 300.0 300.0 25.0"
puts "wireless interface 1 created ..."			           ;# debug info


# add link to backbone
$ns duplex-link $bstation802_11 $router1 100MBit 15ms DropTail 1000

# configure 802.16 nodes

#define coverage area for 802.16 stations: 50m coverage (not realist, but for testing purposes)
Phy/WirelessPhy set Pt_ 0.0134
Phy/WirelessPhy set freq_ 2412e+6
Phy/WirelessPhy set RXThresh_ 5.25089e-10

#note: we put 16 on a different channel to avoid dealing with interferences
set opt(mac)            Mac/802_16
set opt(netif)          Phy/WirelessPhy/OFDM

$ns node-config  -adhocRouting $opt(adhocRouting) \
                 -llType $opt(ll) \
                 -macType $opt(mac) \
                 -channel $opt(channel) \
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

# configure Base station 802.16
set bstation802_16 [$ns node 4.0.0] ;# node id is 9
$bstation802_16 set X_ 400.0
$bstation802_16 set Y_ 300.0
$bstation802_16 set Z_ 0.0
set bstationMac [$bstation802_16 getMac 0]
set clas [new SDUClassifier/Dest]
$bstationMac add-classifier $clas
#set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
set bs_sched [new WimaxScheduler/BS]
$bstationMac set-scheduler $bs_sched
$bstationMac set-channel 0
puts "BS 802.16 created"

# creation of the wireless interface 802.16
$ns node-config -wiredRouting OFF \
                -macTrace ON 				
set iface2 [$ns node 4.0.1] 	                                   ;# node id is 8.	
$iface2 random-motion 0			                           ;# disable random motion
$iface2 base-station [AddrParams addr2id [$bstation802_16 node-addr]] ;#attach mn to basestation
$iface2 set X_ 330.0
$iface2 set Y_ 300.0
$iface2 set Z_ 0.0
set clas [new SDUClassifier/Dest]
[$iface2 set mac_(0)] add-classifier $clas
#set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
set ss_sched [new WimaxScheduler/SS]
[$iface2 set mac_(0)] set-scheduler $ss_sched
[$iface2 set mac_(0)] set-channel 0
$ns at 24 "$iface2 setdest 500.0 300.0 10.0"
puts "wireless interface 2 created ..."			           ;# debug info

# add link to backbone
$ns duplex-link $bstation802_16 $router1 100MBit 15ms DropTail 1000

# add interfaces to MultiFaceNode
$multiFaceNode add-interface-node $iface0
$multiFaceNode add-interface-node $iface1
$multiFaceNode add-interface-node $iface2

# install ND modules

set nd_bs [$bstation802_11 install-nd]
$nd_bs set-router TRUE
$nd_bs router-lifetime 3

set nd_bs2 [$bstation802_16 install-nd]
$nd_bs2 set-router TRUE
$nd_bs2 router-lifetime 3

set nd_mn [$iface1 install-nd]

set nd_mn2 [$iface2 install-nd]

set nd_router0 [$router0 install-nd]
$nd_router0 set-router TRUE
$nd_router0 router-lifetime 3

set nd_host0 [$host0 install-nd]

set nd_bsUMTS [$rnc install-nd]
$nd_bsUMTS set-router TRUE
$nd_bsUMTS router-lifetime 3

set nd_ue [$iface0 install-nd]

# do some kind of registration in UMTS
$ns node-config -llType UMTS/RLC/AM \
		-downlinkBW 384kbs \
		-uplinkBW 384kbs \
		-downlinkTTI 20ms \
		-uplinkTTI 20ms \
   		-hs_downlinkTTI 2ms \
    		-hs_downlinkBW 384kbs

# for the first, we must create. If any other, then use attach
set dch0 [$ns create-dch $iface0 $nd_ue]

# play with ND configuration 
$ns at 8 "puts \" ***** Start testing 802.11 RA using broadcast ***** \""
$ns at 8 "$nd_bs start-ra"
$ns at 10 "puts \" Mobile node is entering the coverage area\""
$ns at 16 "puts \" Mobile node is leaving the coverage area\""
$ns at 20 "$nd_bs stop-ra"
$ns at 20 "puts \" ***** Stop testing 802.11 RA using broadcast ***** \n\""

$ns at 24 "puts \" ***** Start testing 802.16 RA using broadcast ***** \""
$ns at 24 "$nd_bs2 start-ra"
$ns at 26 "puts \" Mobile node is entering the coverage area\""
$ns at 35 "puts \" Mobile node is leaving the coverage area\""
$ns at 39 "$nd_bs2 stop-ra"
$ns at 39 "puts \" ***** Stop testing 802.16 RA using broadcast ***** \n\""


$ns at 40 "puts \" ***** Start testing Ethernet RA using unicast ***** \""
$ns at 40 "$nd_router0 enable-broadcast FALSE"
$ns at 40 "$nd_router0 add-ra-target 1.0.1"

# Add the following line to try adding an invalid address
# Expected error: no target for slot 10.
# $ns at 40 "$nd_router0 add-ra-target 10.0.1"

$ns at 41 "$nd_router0 start-ra"
$ns at 43 "$nd_router0 remove-ra-target 1.0.1"

#Add the following line to test the method remove-ra-target when trying to remove
# an entry that is not in the list.
#$ns at 44 "$nd_router0 remove-ra-target 1.0.1"

$ns at 46 "$nd_router0 stop-ra"
$ns at 46 "puts \" ***** Stop testing Ethernet RA using unicast ***** \n\""

$ns at 47 "puts \" ***** Start testing UMTS RA using unicast ***** \""
$ns at 47 "$nd_bsUMTS enable-broadcast FALSE"
$ns at 47 "$nd_bsUMTS add-ra-target 0.0.2"

# Add the following line to try adding an invalid address
# Expected error: no target for slot 10.
# $ns at 47 "$nd_bsUMTS add-ra-target 10.0.1"

$ns at 48 "$nd_bsUMTS start-ra"
$ns at 49.8 "$nd_bsUMTS remove-ra-target 0.0.2"

# Add the following line to test the method remove-ra-target when trying to remove
# an entry that is not in the list.
#$ns at 51 "$nd_bsUMTS remove-ra-target 0.0.2"

$ns at 51 "$nd_bsUMTS stop-ra"
$ns at 53 "puts \" ***** Stop testing UMTS RA using unicast ***** \n\""

$ns at 56 "puts \" ***** Start testing 802.11 RS using broadcast ***** \""
$ns at 54 "$iface1 setdest 230.0 300.0 25.0" ;#schedule to re-enter the cell in 2 seconds
$ns at 57 "$nd_mn send-rs"
$ns at 63 "puts \" ***** Stop testing 802.11 RS using broadcast ***** \n\""

#+16
$ns at 65 "puts \" ***** Start testing 802.16 RS using broadcast ***** \""
$ns at 63 "$iface2 setdest 430.0 300.0 25.0" ;#schedule to re-enter the cell in 2 seconds
$ns at 71 "$nd_mn2 send-rs"
$ns at 75 "puts \" ***** Stop testing 802.16 RS using broadcast ***** \n\""


$ns at 78 "puts \" ***** Start testing Ethernet RS using unicast ***** \""
$ns at 78 "$nd_host0 enable-broadcast FALSE"
$ns at 78 "$nd_host0 add-rs-target 1.0.0"
$ns at 79 "$nd_host0 send-rs"
$ns at 83 "puts \" ***** Stop testing Ethernet RS using unicast ***** \n\""

$ns at 84 "puts \" ***** Start testing UMTS RS using unicast ***** \""
$ns at 84 "$nd_ue enable-broadcast FALSE"
$ns at 84 "$nd_ue add-rs-target 0.0.0"
$ns at 85 "$nd_ue send-rs"
$ns at 89 "puts \" ***** Stop testing UMTS RS using unicast ***** \n\""

$ns at 90 "finish"

puts " Simulation is running ... please wait ..."
$ns run
