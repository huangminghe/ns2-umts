# Test for MutiFaceNodes.
# @author rouil
# @date 04/19/2005.
# Scenario: Create a multi-interface node using different technologies
#           There is a TCP connection between the router0 and MutiFaceNode.
#           We first use the UMTS interface and at t=3sec we switch the traffic 
#           to the 802.11 interface.

#
# Topology scenario:
#
#                                   bstation802(5.0.0)->)
#                                   /
#                                  /
# router0(3.0.0)---router1(4.0.0)--                              +------------------------------------+
#                                  \                             + iface0:802.11(5.0.1)|              |
#                                   \                            +---------------------+ MutiFaceNode |
#                                   ggsn(2.0.0)                  + iface1:UMTS(0.0.2)  |  (6.0.0)     |
#                                      |                         +------------------------------------+
#                                   sgsn(1.0.0)                  
#                                      |
#                                    rnc(0.0.0)
#                                      |
#                                   bstationUMTS(0.0.1)->)       dummyNode:UMTS(0.0.3)
#              
# Note: the dummy node is required since UMTS works with pairs of mobile nodes. If using 2-4..UMTS mobile node
#  the dummy node is not required.
# Note2: Previous statement is deprecated. A fix was made to ns so that
#        the dummy not is not required.
#
# rouil - 2007/02/16 - 1.1     - Fixed script. Configured channel on wireless nodes and AP to send beacon
#

#check input parameters
if {$argc != 0} {
	puts ""
	puts "Wrong Number of Arguments! No arguments in this topology"
	puts ""
	exit (1)
}

global ns

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

# configure UMTS
$ns set hsdschEnabled_ 1addr
$ns set hsdsch_rlc_set_ 0
$ns set hsdsch_rlc_nif_ 0

# set up for hierarchical routing (needed for routing over a basestation)
$ns node-config -addressType hierarchical
AddrParams set domain_num_  7                      ;# domain number
AddrParams set cluster_num_ {1 1 1 1 1 1 1}            ;# cluster number for each domain 
AddrParams set nodes_num_   {4 1 1 1 1 2 1}	           ;# number of nodes for each cluster             

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

set dummy_node [$ns create-Umtsnode 0.0.3] ;# node id is 3
    puts "Creating dummy node $dummy_node"


# Node id for sgsn0 and ggsn0 are 4 and 5, respectively.
set sgsn [$ns node 1.0.0]
puts "sgsn $sgsn"
set ggsn [$ns node 2.0.0]
puts "ggsn $ggsn"
# Node address for router0 and router1 are 6 and 7, respectively.
set router0 [$ns node 3.0.0]
puts "router0 $router0"
set router1 [$ns node 4.0.0]
puts "router1 $router1"

# do the connections in the UMTS part
$ns duplex-link $rnc $sgsn 622Mbit 0.4ms DropTail 1000
$ns duplex-link $sgsn $ggsn 622MBit 10ms DropTail 1000
$ns duplex-link $ggsn $router1 10MBit 15ms DropTail 1000
$ns duplex-link $router1 $router0 10MBit 35ms DropTail 1000
$rnc add-gateway $sgsn

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

set opt(x)		670			   ;# X dimension of the topography
set opt(y)		670			   ;# Y dimension of the topography

# configure rate for 802.11
Mac/802_11 set basicRate_ 11Mb
Mac/802_11 set dataRate_ 11Mb
Mac/802_11 set bandwidth_ 11Mb
Mac/802_11 set client_lifetime_ 10 ;#increase since iface 2 is not sending traffic for some time

#create the topography
set topo [new Topography]
$topo load_flatgrid $opt(x) $opt(y)
puts "Topology created"

# create God
create-god 11				                ;# give the number of nodes 

# creation of the MutiFaceNodes
$ns node-config  -multiIf ON                            ;#to create MultiFaceNode
set multiFaceNode [$ns node 6.0.0] 
$ns node-config  -multiIf OFF                           ;#reset attribute

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
                 -agentTrace ON \
                 -routerTrace OFF \
                 -macTrace ON  \
                 -movementTrace OFF

# configure Base station 802.11
set bstation802 [$ns node 5.0.0]
$bstation802 set X_ [expr 51.0]
$bstation802 set Y_ 55.0
$bstation802 set Z_ 0.0
[$bstation802 set mac_(0)] bss_id [[$bstation802 set mac_(0)] id]
[$bstation802 set mac_(0)] enable-beacon
[$bstation802 set mac_(0)] set-channel 1

# creation of the wireless interface 802.11
$ns node-config -wiredRouting OFF \
                -macTrace ON 				
set iface1 [$ns node 5.0.1] 	                                   ;# create the node with given @.	
$iface1 random-motion 0			                           ;# disable random motion
$iface1 base-station [AddrParams addr2id [$bstation802 node-addr]] ;#attach mn to basestation
$iface1 set X_ [expr 51.0]
$iface1 set Y_ 50.0
$iface1 set Z_ 0.0
[$iface1 set mac_(0)] set-channel 1
puts "wireless interface 1 created ..."			           ;# debug info


# add link to backbone
$ns duplex-link $bstation802 $router1 10MBit 15ms DropTail 1000

# add interfaces to MultiFaceNode
$multiFaceNode add-interface-node $iface1
$multiFaceNode add-interface-node $iface0

#
#create traffic: TCP application between router0 and Multi interface node
#

# create a TCP agent and attach it to multi-interface node
set tcp_(0) [new Agent/TCP/FullTcp]
#    $ns attach-agent $iface0 $tcp_(0) ;# old command to attach to node
$multiFaceNode attach-agent $tcp_(0) $iface0                   ;# new command: the interface is used for sending
set app_(0) [new Application/TcpApp $tcp_(0)]
puts "App0 id=$app_(0)"
    
# create a TPC agent and attach it to router0
set tcp_(1) [new Agent/TCP/FullTcp] 
$ns attach-agent $router0 $tcp_(1)
set app_(1) [new Application/TcpApp $tcp_(1)]
puts "App1 id=$app_(1)"
    
# connect both TCP agent
#$ns connect $tcp_(0) $tcp_(1) ;# old command to connect to agent
$multiFaceNode connect-agent $tcp_(0) $tcp_(1) $iface0 ;# new command: specify the interface to use
$tcp_(0) listen
puts "udp stream made from udp(0) and sink(0)"

# do some kind of registration in UMTS
$ns node-config -llType UMTS/RLC/AM \
		-downlinkBW 384kbs \
		-uplinkBW 384kbs \
		-downlinkTTI 20ms \
		-uplinkTTI 20ms \
   		-hs_downlinkTTI 2ms \
    		-hs_downlinkBW 384kbs

# for the first HS-DSCH, we must create. If any other, then use attach-hsdsch
$ns create-hsdsch $iface0 $tcp_(0)

# we must set the trace for the environment. If not, then bandwidth is reduced and
# packets are not sent the same way (it looks like they are queued, but TBC)
$bsUMTS setErrorTrace 0 "idealtrace"
#$bsUMTS setErrorTrace 1 "idealtrace"

# load the CQI (Channel Quality Indication)
$bsUMTS loadSnrBlerMatrix "SNRBLERMatrix"

# enable trace file
#$bsUMTS trace-outlink $f 2
#$iface0 trace-inlink $f 2
#$iface0 trace-outlink $f 3

# we cannot start the connect right away. Give time to routing algorithm to run
$ns at 0.5 "$app_(1) connect $app_(0)"

# install a procedure to print out the received data
Application/TcpApp instproc recv {data} {
    global ns
    $ns trace-annotate "$self received data \"$data\""
    puts "$self received data \"$data\""
}

# function to redirect traffic from iface1 to iface1
proc redirectTraffic {} {
    global multiFaceNode tcp_ iface1
    $multiFaceNode attach-agent $tcp_(0) $iface1 ;# the interface is used for sending
    $multiFaceNode connect-agent $tcp_(0) $tcp_(1) $iface1 ;# the interface is used for receiving
}

# send a message via TcpApp
# The string will be interpreted by the receiver as Tcl code.
for { set i 1 } { $i < 5 } { incr i} {
    $ns at [expr $i + 0.5] "$app_(1) send 100 {$app_(0) recv {my message $i}}"
}

# call to redirect traffic
$ns at 3 "redirectTraffic"
$ns at 3 "$ns trace-annotate \"Redirecting traffic\""

$ns at 5 "finish"

puts " Simulation is running ... please wait ..."
$ns run
