# Test for case 2 with one wman. 
# @author rouil
# @date 05/19/2005.
# Scenario: Create a multi-interface node using different technologies
#           There is a UDP connection between the router0 and MultiFaceNode.
#           We first use the UMTS interface, then we switch the traffic 
#           to the 802.16 interface when it becomes available. 
#           When the node leaves the coverage area of 802.16, it creates a link going down 
#           event to redirect to UMTS.
#
#           By adjusting the power_level_coefficient argument, the MN will trigger
#           the handover at different times. The higher the coefficient, the sooner
#           the trigger will be
#
# Topology scenario:
#
#                                   bstation802.16(3.0.0)->)
#                                   /
#                                  /      
# router0(1.0.0)---router1(2.0.0)--                              +------------------------------------+
#                                  \                             + iface1:802.16(3.0.1)|              |
#                                   \                            +---------------------+ MutiFaceNode |
#                                   rnc(0.0.0)                   + iface0:UMTS(0.0.2)  |  (4.0.0)     |
#                                      |                         +------------------------------------+
#                                 bstationUMTS(0.0.1)->)         
#                                                                
#                                                   
#                                                                
#              
# 
#
# rouil - 2007/02/16 - 1.1     - Fixed script to work with updated mobility model
#                              - Updated description


#check input parameters
if {$argc != 3} {
    puts ""
    puts "Wrong Number of Arguments! "
    puts "command is ns scenario1.tcl seed power_level_coefficient ms_speed"
    exit 1
}

global ns

#define DEBUG parameters
set quiet 1
Agent/ND set debug_ 1 
Agent/MIH set debug_ 1
Agent/MIHUser/IFMNGMT set debug_ 1
Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1 set debug_ 1
Mac/802_16 set debug_ 0

#define frequency of RA at base station
Agent/ND set minRtrAdvInterval_ 200
Agent/ND set maxRtrAdvInterval_ 600
Agent/ND set router_lifetime_   1800
Agent/ND set minDelayBetweenRA_ 0.03
Agent/ND set maxRADelay_        0

#defines function for flushing and closing files
proc finish {} {
    global ns f quiet
    $ns flush-trace
    close $f
    if {$quiet == 0} {
    puts " Simulation ended."
    }
    exit 0
}

#Rate at which the nodes start moving
set moveStart 10
set moveStop 100
#Speed of the mobile nodes (m/sec)

set moveSpeed [lindex $argv 2] 
#origin of the MN
#set X_src 40.0
set X_src 52.0
set Y_src 50.0
set X_dst 700.0
set Y_dst 50.0

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
AddrParams set domain_num_  5                      ;# domain number
AddrParams set cluster_num_ {1 1 1 1 1}            ;# cluster number for each domain 

# 1st cluster: UMTS: 2 network entities + nb of mobile nodes
# 2nd cluster: CN
# 3rd cluster: core network
# 4th cluster: WLAN: 1BS + nb of mobile nodes
# 5th cluster: super nodes
lappend tmp 3                                      ;# UMTS MNs+RNC+BS
lappend tmp 1                                      ;# router 0
lappend tmp 1                                      ;# router 1
lappend tmp 2                                      ;# 802.11 MNs+BS
lappend tmp 1                                      ;# MULTIFACE nodes 
AddrParams set nodes_num_ $tmp

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
if {$quiet == 0} {
    puts "rnc: tcl=$rnc; id=[$rnc id]; addr=[$rnc node-addr]"
}

# configure UMTS base station
$ns node-config -UmtsNodeType bs \
		-downlinkBW 384kbs \
		-downlinkTTI 10ms \
		-uplinkBW 384kbs \
		-uplinkTTI 10ms \
		-uplinkTTI 10ms \
     		-hs_downlinkTTI 2ms \
      		-hs_downlinkBW 384kbs 

set bsUMTS [$ns create-Umtsnode 0.0.1] ;# node id is 1
if {$quiet == 0} {
    puts "bsUMTS: tcl=$bsUMTS; id=[$bsUMTS id]; addr=[$bsUMTS node-addr]"
}

# connect RNC and base station
$ns setup-Iub $bsUMTS $rnc 622Mbit 622Mbit 15ms 15ms DummyDropTail 2000

$ns node-config -UmtsNodeType ue \
		-baseStation $bsUMTS \
		-radioNetworkController $rnc

set iface0 [$ns create-Umtsnode 0.0.2] ; #Node Id begins with 2. 

# Node address for router0 and router1 are 4 and 5, respectively.
set router0 [$ns node 1.0.0]
set router1 [$ns node 2.0.0]
if {$quiet == 0} {
    puts "router0: tcl=$router0; id=[$router0 id]; addr=[$router0 node-addr]"
    puts "router1: tcl=$router1; id=[$router1 id]; addr=[$router1 node-addr]"
}

# connect links 
$ns duplex-link $rnc $router1 622Mbit 0.4ms DropTail 1000
$ns duplex-link $router1 $router0 100MBit 30ms DropTail 1000
$rnc add-gateway $router1

# creation of the MutiFaceNodes. It MUST be done before the 802.11
$ns node-config  -multiIf ON                            ;#to create MultiFaceNode 

set multiFaceNode [$ns node 4.0.0] 

$ns node-config  -multiIf OFF                           ;#reset attribute
if {$quiet == 0} {
    puts "multiFaceNode(s) has/have been created"
}

# Define global simulation parameters
Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1 set case_ 3

# seed the default RNG
global defaultRNG
set seed [lindex $argv 0]
if { $seed == "random"} {
    $defaultRNG seed 0
} else {
    $defaultRNG seed [lindex $argv 0]
}

#
# Now we add 802.16 nodes
#
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

set opt(x)		1100			   ;# X dimension of the topography
set opt(y)		1100			   ;# Y dimension of the topography

#create the topography
set topo [new Topography]
$topo load_flatgrid $opt(x) $opt(y)
#puts "Topology created"
set chan [new $opt(chan)]

# create God
create-god 11				                ;# give the number of nodes 

#define coverage area for base station: 500m coverage 
Phy/WirelessPhy set Pt_ 0.025
Phy/WirelessPhy set RXThresh_ 2.025e-12
Phy/WirelessPhy set CSThresh_ [expr 0.9*[Phy/WirelessPhy set RXThresh_]]

# configure Access Points
$ns node-config  -adhocRouting $opt(adhocRouting) \
                 -llType $opt(ll) \
                 -macType $opt(mac) \
                 -channel [new $opt(chan)] \
                 -ifqType $opt(ifq) \
                 -ifqLen $opt(ifqlen) \
                 -antType $opt(ant) \
                 -propType $opt(prop)    \
                 -phyType $opt(netif) \
                 -topoInstance $topo \
                 -wiredRouting ON \
                 -agentTrace OFF \
                 -routerTrace OFF \
                 -macTrace ON  \
                 -movementTrace ON

# configure Base station 802.11
set bstation802 [$ns node 3.0.0]
$bstation802 set X_ 50.0
$bstation802 set Y_ 50.0
$bstation802 set Z_ 0.0
puts "bstation802: tcl=$bstation802; id=[$bstation802 id]; addr=[$bstation802 node-addr]"
set clas [new SDUClassifier/Dest]
[$bstation802 set mac_(0)] add-classifier $clas
#set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
set bs_sched [new WimaxScheduler/BS]
[$bstation802 set mac_(0)] set-scheduler $bs_sched
[$bstation802 set mac_(0)] set-channel 0

#averaging settings for power level
#Mac/802_16 set rxp_avg_alpha_ 0.05
Mac/802_16 set lgd_factor_ [lindex $argv 1] 

# creation of MNs
$ns node-config -wiredRouting OFF \
                -macTrace ON 		
set iface1 [$ns node 3.0.1]     ;# node id is 8. 
$iface1 random-motion 0		;# disable random motion
$iface1 base-station [AddrParams addr2id [$bstation802 node-addr]] ;#attach mn to basestation
$iface1 set X_ $X_src
$iface1 set Y_ $Y_src
$iface1 set Z_ 0.0
set clas [new SDUClassifier/Dest]
[$iface1 set mac_(0)] add-classifier $clas
#set the scheduler for the node. Must be changed to -shed [new $opt(sched)]
set ss_sched [new WimaxScheduler/SS]
[$iface1 set mac_(0)] set-scheduler $ss_sched
[$iface1 set mac_(0)] set-channel 0
if {$quiet == 0} {
    puts "Iface 1 = $iface1"
}

#calculate the speed of the node
$ns at $moveStart "$iface1 setdest $X_dst $Y_dst $moveSpeed"

#add the interfaces to supernode
$multiFaceNode add-interface-node $iface0
$multiFaceNode add-interface-node $iface1

# add link to backbone
$ns duplex-link $bstation802 $router1 100MBit 15ms DropTail 1000

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

# now WIMAX
set nd_bs [$bstation802 install-nd]
$nd_bs set-router TRUE
$nd_bs router-lifetime 1800
$ns at 0.01 "$nd_bs start-ra"

set nd_mn [$iface1 install-nd]

set ifmgmt_bs [$bstation802 install-default-ifmanager]
set mih_bs [$bstation802 install-mih]
$ifmgmt_bs connect-mih $mih_bs
set tmp2 [$bstation802 set mac_(0)] ;#in 802.16 one interface is created
$tmp2 mih $mih_bs
$mih_bs add-mac $tmp2


# install interface manager into multi-interface node and CN
set handover [new Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1]
$multiFaceNode install-ifmanager $handover
$nd_mn set-ifmanager $handover
$handover nd_mac $nd_mn [$iface1 set mac_(0)] ;#to know how to send RS
$nd_ue set-ifmanager $handover

set ifmgmt_cn [$router0 install-default-ifmanager]

# install MIH in multi-interface node
set mih [$multiFaceNode install-mih]

$handover connect-mih $mih ;#create connection between MIH and iface management

#
#create traffic: UDP application between router0 and Multi interface node
#
#Create a UDP agent and attach it to node n0
set udp_ [new Agent/UDP]
$udp_ set packetSize_ 1500

if {$quiet == 0} {
    puts "udp on node : $udp_"
}

# Create a CBR traffic source and attach it to udp0
set cbr_ [new Application/Traffic/CBR]
$cbr_ set packetSize_ 500
$cbr_ set interval_ 0.02
$cbr_ attach-agent $udp_

#create an sink into the sink node

# Create the Null agent to sink traffic
set null_ [new Agent/Null] 
    
#Router0 is receiver    
#$ns attach-agent $router0 $null_

#Router0 is transmitter    
$ns attach-agent $router0 $udp_
    
# Attach the 2 agents
#$ns connect $udp_ $null_ OLD COMMAND
    
#Multiface node is transmitter
#$multiFaceNode attach-agent $udp_ $iface0
#$handover add-flow $udp_ $null_ $iface0 1 2000.
    
#Multiface node is receiver
$multiFaceNode attach-agent $null_ $iface0
$handover add-flow $null_ $udp_ $iface0 1 ;#2000.
        
# do registration in UMTS. This will create the MACs in UE and base stations
$ns node-config -llType UMTS/RLC/AM \
    -downlinkBW 384kbs \
    -uplinkBW 384kbs \
    -downlinkTTI 20ms \
    -uplinkTTI 20ms \
    -hs_downlinkTTI 2ms \
    -hs_downlinkBW 384kbs

# for the first HS-DCH, we must create. If any other, then use attach-dch
#set dch0($n_) [$ns create-dch $iface0($n_) $udp_($n_)]; # multiface node transmitter
set dch0 [$ns create-dch $iface0 $null_]; # multiface node receiver
$ns attach-dch $iface0 $handover $dch0
$ns attach-dch $iface0 $nd_ue $dch0

# Now we can register the MIH module with all the MACs
set tmp2 [$iface0 set mac_(2)] ;#in UMTS and using DCH the MAC to use is 2 (0 and 1 are for RACH and FACH)
$tmp2 mih $mih
$mih add-mac $tmp2
set tmp2 [$iface1 set mac_(0)] ;#in 802.16 one interface is created
$tmp2 mih $mih
$mih add-mac $tmp2


#Start the application 1sec before the MN is entering the WLAN cell
$ns at [expr $moveStart - 1] "$cbr_ start"

#Stop the application according to another poisson distribution (note that we don't leave the 802.11 cell)
$ns at [expr $moveStop  + 1] "$cbr_ stop"

# set original status of interface. By default they are up..so to have a link up, 
# we need to put them down first.
$ns at 0 "[eval $iface0 set mac_(2)] disconnect-link" ;#UMTS UE
$ns at 0.1 "[eval $iface0 set mac_(2)] connect-link"     ;#umts link 

#$ns at $moveStart "puts \"At $moveStart Mobile Node starts moving\""
#$ns at [expr $moveStart+10] "puts \"++At [expr $moveStart+10] Mobile Node enters wlan\""
#$ns at [expr $moveStart+110] "puts \"++At [expr $moveStart+110] Mobile Node leaves wlan\""
#$ns at $moveStop "puts \"Mobile Node stops moving\""
#$ns at [expr $moveStop + 40] "puts \"Simulation ends at [expr $moveStop+40]\"" 
$ns at [expr $moveStop + 5] "finish" 

if {$quiet == 0} {
puts " Simulation is running ... please wait ..."
}

$ns run
