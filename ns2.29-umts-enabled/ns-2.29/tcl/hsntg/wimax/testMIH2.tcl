# Test for MutiFaceNodes.
# @author rouil
# @date 05/19/2005.
# Scenario: Create a multi-interface node using different technologies
#           There is a TCP connection between the router0 and MultiFaceNode.
#           1- Traffic start on UMTS interface
#           2- Node enters IEEE 802.16 network and traffic is redirected to new network
#           3- Node enters IEEE 802.11 network and traffic is redirected to new network
#           4- Node leaves IEEE 802.11 network and traffic is redirected to IEEE 802.16
#           5- Node leaves IEEE 802.16 network and traffic is redirected to UMTS
#
#
#
# Topology scenario:
#
#                                   bstation802(3.0.0)->)
#                                   /
#                                  /
# router0(1.0.0)---router1(2.0.0)-------vlan(2.0.1)              +------------------------------------+
#                                | \               \             + iface1:802.11(3.0.1)|              |
#                                |  \               \            +---------------------+              |
#                                |  rnc(0.0.0)       ------------+ iface2:802.3(2.0.2) + MutiFaceNode |
#                                |     |                         +---------------------+  (5.0.0)     |
#                                |bstationUMTS(0.0.1)->)         + iface0:UMTS(0.0.2)  |              |
#                                |                               +---------------------+              |
#                                |                               + iface3:802.16(4.0.1)|              |
#                                bstation802_16(4.0.0)->)        +------------------------------------+
#                                                   
#                                                                
# rouil - 2007/02/16 - 1.1     - Fixed script to work with updated mobility model              
#

#check input parameters
if {$argc != 0} {
	puts ""
	puts "Wrong Number of Arguments! No arguments in this topology"
	puts ""
	exit (1)
}

global ns

#set debug attributes
Agent/ND set debug_ 1
Agent/MIH set debug_ 1
Agent/MIHUser/IFMNGMT/MIPV6 set debug_ 1
Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1 set debug_ 1
#Mac/802_16 set debug_ 1 

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
set f [open out.res w]
$ns trace-all $f

# set up for hierarchical routing (needed for routing over a basestation)
$ns node-config -addressType hierarchical
AddrParams set domain_num_  6                       ;# domain number
AddrParams set cluster_num_ {1 1 1 1 1 1}           ;# cluster number for each domain 
AddrParams set nodes_num_   {3 1 3 2 2 1}           ;# number of nodes for each cluster             

# configure UMTS. 
# Note: The UMTS configuration MUST be done first otherwise it does not work
#       furthermore, the node creation in UMTS MUST be as follow
#       rnc, base station, and UE (User Equipment)
$ns set hsdschEnabled_ 1addr
$ns set hsdsch_rlc_set_ 0
$ns set hsdsch_rlc_nif_ 0

# configure RNC node
$ns node-config -UmtsNodeType rnc 
set rnc [$ns create-Umtsnode 0.0.0] ;# node id is 0.
puts "rnc: tcl=$rnc; id=[$rnc id]; addr=[$rnc node-addr]"

# configure UMTS base station
$ns node-config -UmtsNodeType bs \
		-downlinkBW 384kbs \
		-downlinkTTI 10ms \
		-uplinkBW 384kbs \
		-uplinkTTI 10ms \
     		-hs_downlinkTTI 2ms \
      		-hs_downlinkBW 384kbs 

set bsUMTS [$ns create-Umtsnode 0.0.1] ;# node id is 1
puts "bsUMTS: tcl=$bsUMTS; id=[$bsUMTS id]; addr=[$bsUMTS node-addr]"

# connect RNC and base station
$ns setup-Iub $bsUMTS $rnc 622Mbit 622Mbit 15ms 15ms DummyDropTail 2000

$ns node-config -UmtsNodeType ue \
		-baseStation $bsUMTS \
		-radioNetworkController $rnc

set iface0 [$ns create-Umtsnode 0.0.2] ;# node id is 2
puts "iface0(UMTS): tcl=$iface0; id=[$iface0 id]; addr=[$iface0 node-addr]" 

# Node address for router0 and router1 are 4 and 5, respectively.
set router0 [$ns node 1.0.0]
puts "router0: tcl=$router0; id=[$router0 id]; addr=[$router0 node-addr]"
set router1 [$ns node 2.0.0]
puts "router1: tcl=$router1; id=[$router1 id]; addr=[$router1 node-addr]"
# creating lan to iface2
lappend nodelist $router1
set iface2 [$ns node 2.0.2]
lappend nodelist $iface2
puts "iface2: tcl=$iface2; id=[$iface2 id]; addr=[$iface2 node-addr]"

set lan0 [$ns newLan $nodelist 10Mb 1ms \
	      -llType LL -ifqType Queue/DropTail \
	      -macType Mac/802_3 -chanType Channel -address 2.0.1]
puts "lan created between router1 and iface2"

# connect links 
$ns duplex-link $rnc $router1 622Mbit 0.4ms DropTail 1000
$ns duplex-link $router1 $router0 100MBit 5ms DropTail 1000
$ns duplex-link $router1 $iface2 100MBit 5ms DropTail 1000
$rnc add-gateway $router1


# creation of the MutiFaceNodes. It MUST be done before the 802.11
$ns node-config  -multiIf ON                            ;#to create MultiFaceNode 
set multiFaceNode [$ns node 5.0.0]                      ;# node id is 6
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
set bstation802 [$ns node 3.0.0] ;
$bstation802 set X_ 680.0
$bstation802 set Y_ 1000.0
$bstation802 set Z_ 0.0
puts "bstation802: tcl=$bstation802; id=[$bstation802 id]; addr=[$bstation802 node-addr]"
# we need to set the BSS for the base station
set bstationMac [$bstation802 getMac 0]
set AP_ADDR_0 [$bstationMac id]
puts "bss_id for bstation 1=$AP_ADDR_0"
$bstationMac bss_id $AP_ADDR_0
$bstationMac enable-beacon
$bstationMac set-channel 1

# creation of the wireless interface 802.11
$ns node-config -wiredRouting OFF \
                -macTrace ON 				
set iface1 [$ns node 3.0.1] 	                                   ;# node id is 8.	
$iface1 random-motion 0			                           ;# disable random motion
$iface1 base-station [AddrParams addr2id [$bstation802 node-addr]] ;#attach mn to basestation
$iface1 set X_ 480.0
$iface1 set Y_ 1000.0
$iface1 set Z_ 0.0
[$iface1 set mac_(0)] set-channel 1
# define node movement. We start from outside the coverage, cross it and leave.
$ns at 0 "$iface1 setdest 1600.0 1000.0 3.0"
puts "iface1: tcl=$iface1; id=[$iface1 id]; addr=[$iface1 node-addr]"		    

# add link to backbone
$ns duplex-link $bstation802 $router1 100MBit 15ms DropTail 1000

# add Wimax nodes
set opt(netif)          Phy/WirelessPhy/OFDM       ;# network interface type 802.16
set opt(mac)            Mac/802_16                 ;# MAC type 802.16

Phy/WirelessPhy set Pt_ 0.025
Phy/WirelessPhy set RXThresh_ 2.025e-12
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

# configure Base station 802.16
set bstation802_16 [$ns node 4.0.0] ;
$bstation802_16 set X_ 1000
$bstation802_16 set Y_ 1000
$bstation802_16 set Z_ 0.0
puts "bstation802_16: tcl=$bstation802_16; id=[$bstation802_16 id]; addr=[$bstation802_16 node-addr]"
set clas [new SDUClassifier/Dest]
[$bstation802_16 set mac_(0)] add-classifier $clas
#set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
set bs_sched [new WimaxScheduler/BS]
[$bstation802_16 set mac_(0)] set-scheduler $bs_sched
[$bstation802_16 set mac_(0)] set-channel 1

# creation of the wireless interface 802.11
$ns node-config -wiredRouting OFF \
                -macTrace ON 				
set iface3 [$ns node 4.0.1] 	                                   ;# node id is 8.	
$iface3 random-motion 0			                           ;# disable random motion
$iface3 base-station [AddrParams addr2id [$bstation802_16 node-addr]] ;#attach mn to basestation
$iface3 set X_ 480.0
$iface3 set Y_ 1000.0
$iface3 set Z_ 0.0
set clas [new SDUClassifier/Dest]
[$iface3 set mac_(0)] add-classifier $clas
#set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
set ss_sched [new WimaxScheduler/SS]
[$iface3 set mac_(0)] set-scheduler $ss_sched
[$iface3 set mac_(0)] set-channel 1
# define node movement. We start from outside the coverage, cross it and leave.
$ns at 0 "$iface3 setdest 1600.0 1000.0 3.0"
puts "iface3: tcl=$iface3; id=[$iface3 id]; addr=[$iface3 node-addr]"		    

# add link to backbone
$ns duplex-link $bstation802_16 $router1 100MBit 15ms DropTail 1000


# add interfaces to MultiFaceNode
$multiFaceNode add-interface-node $iface1
$multiFaceNode add-interface-node $iface0
$multiFaceNode add-interface-node $iface2
$multiFaceNode add-interface-node $iface3


# install ND modules

# take care of UMTS
# Note: The ND module is on the rnc node NOT in the base station
set nd_rncUMTS [$rnc install-nd]
$nd_rncUMTS set-router TRUE
$nd_rncUMTS router-lifetime 1800
$nd_rncUMTS enable-broadcast FALSE
$nd_rncUMTS add-ra-target 0.0.2 ;#in UMTS there is no notion of broadcast. 
                                 #We fake it by sending unicast to a list of nodes
set nd_ue [$iface0 install-nd]

# now WLAN
set nd_bs [$bstation802 install-nd]
$nd_bs set-router TRUE
$nd_bs router-lifetime 1800

set nd_mn [$iface1 install-nd]

# now WIMAX
set nd_bs2 [$bstation802_16 install-nd]
$nd_bs2 set-router TRUE
$nd_bs2 router-lifetime 1800

set nd_mn2 [$iface3 install-nd]

# finally Ethernet
set nd_router [$router1 install-nd]
$nd_router set-router TRUE
$nd_router router-lifetime 1800

set nd_eth [$iface2 install-nd]

# install interface manager into multi-interface node and CN
Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1 set case_ 2
set handover [new Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1]
$multiFaceNode install-ifmanager $handover
$nd_mn set-ifmanager $handover
$handover nd_mac $nd_mn [$iface1 set mac_(0)] ;#to know how to send RS
$nd_mn2 set-ifmanager $handover
$handover nd_mac $nd_mn2 [$iface3 set mac_(0)] ;#to know how to send RS
$nd_ue set-ifmanager $handover
$nd_eth set-ifmanager $handover

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

#
#create traffic: TCP application between router0 and Multi interface node
#

# create a TCP agent and attach it to multi-interface node
set tcp_(0) [new Agent/TCP/FullTcp]
$multiFaceNode attach-agent $tcp_(0) $iface0            ;# new command: the interface is used for sending
set app_(0) [new Agent/Null] ;#we can use this or the next line
    
# create a TPC agent and attach it to router0
set tcp_(1) [new Agent/TCP/FullTcp] 
$ns attach-agent $router0 $tcp_(1) 
  
# Create a CBR traffic source and attach it to tcp_(1)
set cbr_(0) [new Application/Traffic/CBR]
$cbr_(0) set packetSize_ 1000
$cbr_(0) set interval_ 0.5
$cbr_(0) attach-agent $tcp_(1)

# connect both TCP agent
$handover add-flow $tcp_(0) $tcp_(1) $iface0 1
$tcp_(0) listen
puts "tcp stream made from [$router0 node-addr] and [$iface0 node-addr]"

# do registration in UMTS. This will create the MACs in UE and base stations
$ns node-config -llType UMTS/RLC/AM \
		-downlinkBW 384kbs \
		-uplinkBW 384kbs \
		-downlinkTTI 20ms \
		-uplinkTTI 20ms \
   		-hs_downlinkTTI 2ms \
    		-hs_downlinkBW 384kbs

# for the first HS-DCH, we must create. If any other, then use attach-dch
set dch0 [$ns create-dch $iface0 $tcp_(0)]
$ns attach-dch $iface0 $handover $dch0
$ns attach-dch $iface0 $nd_ue $dch0

# Now we can register the MIH module with all the MACs
set tmp2 [$iface0 set mac_(2)] ;#in UMTS and using DCH the MAC to use is 2 (0 and 1 are for RACH and FACH)
$tmp2 mih $mih
$mih add-mac $tmp2             ;#inform the MIH about the local MAC
set tmp2 [$iface1 set mac_(0)] ;#in 802.11 one interface is created
$tmp2 mih $mih
$mih add-mac $tmp2             ;#inform the MIH about the local MAC
set tmp2 [$iface2 set mac_(0)] ;#in 802.3 one interface is created
$tmp2 mih $mih
$mih add-mac $tmp2             ;#inform the MIH about the local MAC
set tmp2 [$iface3 set mac_(0)] ;#in 802.16 one interface is created
$tmp2 mih $mih
$mih add-mac $tmp2             ;#inform the MIH about the local MAC

# enable trace file
#$bsUMTS trace-outlink $f 2

$ns at 10 "$cbr_(0) start" ;#we should make sure we have UMTS link up before starting to send.

# set original status of interface. By default they are up..so to have a link up, 
# we need to put them down first.
$ns at 0 "[eval $iface0 set mac_(2)] disconnect-link" ;#UMTS UE
$ns at 0 "[eval $iface2 set mac_(0)] disconnect-link" ;#Ethernet

# set the starting time for Router Advertisements
$ns at 3 "$nd_bs start-ra"
$ns at 3 "$nd_bs2 start-ra"
#$ns at 1 "$nd_rncUMTS start-ra"
#$ns at 1 "$nd_router start-ra"

$ns at 9 "[eval $iface0 set mac_(2)] connect-link"     ;#umts link 
$ns at 1200 "[eval $iface2 set mac_(0)] connect-link"    ;#ethernet link
$ns at 1240 "[eval $iface2 set mac_(0)] disconnect-link" ;#ethernet link

$ns at 500 "finish"

puts " Simulation is running ... please wait ..."
$ns run
