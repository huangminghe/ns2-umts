# Test for MutiFaceNodes.
# @author rouil
# @date 04/19/2005.
# Scenario: Create a multi-interface node using ethernet and802.11 technologies
#           Traffic is being sent to all the interfaces and received
#           by a common agent (could be different agents too).
#           Traffic is also sent from Multi-interface node to external node.

#
# Topology scenario:
#
#
#               
#               
#      Router0-------Router1->)
#        |             |     
#        |             |   
#      Router2-------Router3
#        |              \           
#        +->)            \ 
#                         +------------------------+
#                         + if0:Eth   | if1:802.11 |
#                         +-----------+------------|
#                         + if2:802.11|            |
#                         +------------------------+
#                                MultiFaceNode
#
#
# rouil - 2007/02/16 - 1.1     - Fixed script. Configured channel on wireless nodes and AP to send beacon


#check input parameters
if {$argc != 0} {
	puts ""
	puts "Wrong Number of Arguments! No arguments in this topology"
	puts ""
	exit (1)
}

#enable debug information
Mac/802_11 set debug_ 1

# set global variables
set packet_size	1500			           ;# packet size in bytes at CBR applications 
set output_dir .
set bw [expr 11*1024*1024]      		   ;# for 801.11: 11Mbps
set gap_size 0.5                                   ;# compute gap size between packets
puts "gap size=$gap_size"
set traffic_start 10
set traffic_stop  12
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
Mac/802_11 set client_lifetime_ 10 ;#increase since iface 2 is not sending traffic for some time

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
puts "Topology created"

#open file for trace
set tf [open $output_dir/out.res w]
$ns trace-all $tf
puts "Output file configured"

# set up for hierarchical routing (needed for routing over a basestation)
#puts "start hierarchical addressing"
$ns node-config -addressType hierarchical
AddrParams set domain_num_ 5          			;# domain number
lappend cluster_num 1 1 1 1 1          			;# cluster number for each domain 
AddrParams set cluster_num_ $cluster_num
lappend eilastlevel 1 2 2 2 1 		                ;# number of nodes for each cluster 
AddrParams set nodes_num_ $eilastlevel
puts "Configuration of hierarchical addressing done"

# create God
create-god 8				                ;# nb_mn + 2 (base station and sink node)

# creates the wired routers
set router0 [$ns node 0.0.0]
set router3 [$ns node 3.0.0]

# create the wired interface node
set iface0 [$ns node 3.0.1]

# creation of the MutiFaceNodes
$ns node-config  -multiIf ON                            ;#to create MultiFaceNode
set multiFaceNode [$ns node 4.0.0] 
$ns node-config  -multiIf OFF                           ;#reset attribute

# create wireless channels
set chan1 [new $opt(chan)]
#set chan2 [new $opt(chan)]

# configure Access Points
$ns node-config  -adhocRouting $opt(adhocRouting) \
                 -llType $opt(ll) \
                 -macType $opt(mac) \
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

# configure Base station 1
$ns node-config  -channel $chan1
set router1 [$ns node 1.0.0]
$router1 set X_ [expr 51.0]
$router1 set Y_ 55.0
$router1 set Z_ 0.0
[$router1 set mac_(0)] bss_id [[$router1 set mac_(0)] id]
[$router1 set mac_(0)] enable-beacon
[$router1 set mac_(0)] set-channel 1

# configure Base station 2
$ns node-config  -channel $chan1
set router2 [$ns node 2.0.0] 
$router2 set X_ [expr 52.0]
$router2 set Y_ 55.0
$router2 set Z_ 0.0
[$router2 set mac_(0)] bss_id [[$router2 set mac_(0)] id]
[$router2 set mac_(0)] enable-beacon
[$router2 set mac_(0)] set-channel 2

# creation of the wireless interface 1
$ns node-config -wiredRouting OFF \
                -macTrace ON  \
                -channel $chan1				
set iface1 [$ns node 1.0.1] 	                               ;# create the node with given @.	
$iface1 random-motion 0			                       ;# disable random motion
$iface1 base-station [AddrParams addr2id [$router1 node-addr]] ;#attach mn to basestation
$iface1 set X_ [expr 51.0]
$iface1 set Y_ 50.0
$iface1 set Z_ 0.0
[$iface1 set mac_(0)] set-channel 1
puts "wireless interface 1 created ..."			       ;# debug info

# creation of the wireless interface 2
$ns node-config -wiredRouting OFF \
                -channel $chan1 \
                -macTrace ON  				
set iface2 [$ns node 2.0.1] 	                               ;# create the node with given @.	
$iface2 random-motion 0			                       ;# disable random motion
$iface2 base-station [AddrParams addr2id [$router2 node-addr]] ;#attach mn to basestation
$iface2 set X_ [expr 52.0]
$iface2 set Y_ 50.0
$iface2 set Z_ 0.0
[$iface2 set mac_(0)] set-channel 2
puts "wireless interface 2 created ..."			;# debug info
	
# add interfaces to MultiFaceNode
$multiFaceNode add-interface-node $iface0
$multiFaceNode add-interface-node $iface1
$multiFaceNode add-interface-node $iface2

# creates the links
$ns duplex-link $router0 $router1 100Mb 1ms DropTail
$ns duplex-link $router0 $router2 100Mb 1ms DropTail
$ns duplex-link $router1 $router3 100Mb 1ms DropTail
$ns duplex-link $router2 $router3 100Mb 1ms DropTail
$ns duplex-link $router3 $iface0 100Mb 1ms DropTail

#create source traffic 3: from ethernet interface to router0
#Create a TCP agent and attach it to multi-interface node
set tcp_(0) [new Agent/TCP/FullTcp]
$multiFaceNode attach-agent $tcp_(0) $iface1 ;# the interface is used for sending
set app_(0) [new Application/TcpApp $tcp_(0)]
puts "App0 id=$app_(0)"

#create an sink into the MN1
# Create the Null agent to sink traffic
set tcp_(1) [new Agent/TCP/FullTcp] 
$ns attach-agent $router0 $tcp_(1)
set app_(1) [new Application/TcpApp $tcp_(1)]
puts "App1 id=$app_(1)"

# Attach the 2 agents
$multiFaceNode connect-agent $tcp_(0) $tcp_(1) $iface1 ;# the interface is used for receiving
$tcp_(0) listen ;# we connect to multi-interface node.

# we cannot start the connect right away. Give time to routing algorithm to run
$ns at 0.5 "$app_(1) connect $app_(0)"

# install a procedure to print out the received data
Application/TcpApp instproc recv {data} {
    global ns
    $ns trace-annotate "$self received data \"$data\""
    puts "$self received data \"$data\""
}

# procedure that redirect the connection from iface1 to iface2
proc redirectTraffic {} {
    global multiFaceNode tcp_ iface2 iface0
    $multiFaceNode attach-agent $tcp_(0) $iface2 ;# the interface is used for sending
    $multiFaceNode connect-agent $tcp_(0) $tcp_(1) $iface2 ;# the interface is used for receiving
}

# send a message via TcpApp
# The string will be interpreted by the receiver as Tcl code.
for { set i 1 } { $i < 5 } { incr i} {
    $ns at [expr $i + 0.5] "$app_(1) send 100 {$app_(0) recv {my message $i}}"
}

# call the redirection 
$ns at 3 "redirectTraffic"
$ns at 3 "$ns trace-annotate \"Redirecting traffic\""

$ns at $simulation_stop "finish"

# Run the simulation
puts "Running simulation ..."
$ns run
puts "Simulation done."
