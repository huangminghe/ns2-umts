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
global ns
#check input parameters
if {$argc > 2 } {
	puts ""
	puts "Wrong Number of Arguments! No arguments in this topology"
	puts ""
	exit (1)
}


set output_dir .

set quiet 0

# seed the default RNG
global defaultRNG
if {$argc == 1} {
    set seed [lindex $argv 1]
    if { $seed == "random"} {
	$defaultRNG seed 0
    } else {
	$defaultRNG seed [lindex $argv 1]
    }
}

#define coverage area for base station: 50m coverage 
Phy/WirelessPhy set Pt_ 0.0134
Phy/WirelessPhy set freq_ 2412e+6
Phy/WirelessPhy set RXThresh_ 5.25089e-10
Phy/WirelessPhy set CSThresh_ [expr 0.9* [Phy/WirelessPhy set RXThresh_]]

#define frequency of RA at base station
Agent/ND set maxRtrAdvInterval_ 1.0
Agent/ND set minRtrAdvInterval_ 0.5
Agent/ND set minDelayBetweenRA_ 3
Agent/ND set maxRADelay_ 0.5

#define MAC 802_11 parameters
Mac/802_11 set bss_timeout_ 5
Mac/802_11 set pr_limit_ 1.2 ;#for link going down
Mac/802_11 set client_lifetime_ 40.0 ;# since we don't have traffic, the AP would disconnect the MN automatically after client_lifetime_
Mac/802_11 set basicRate_ 11Mb
Mac/802_11 set dataRate_ 11Mb
Mac/802_11 set bandwidth_ 11Mb


#wireless routing algorithm update frequency (in seconds)
Agent/DSDV set perup_ 8

# Define global simulation parameters

#define DEBUG parameters
set quiet 0
Agent/ND set debug_ 1
Agent/MIH set debug_ 1
Agent/MIHUser/IFMNGMT/MIPV6 set debug_ 1
Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1 set debug_ 1
Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1 set case_ 2
Mac/802_11 set debug_ 0

#Rate at which the nodes start moving
set moveStart 10.0
set moveStop 160.0
#Speed of the mobile nodes (m/sec)
set moveSpeed 1

#origin of the MN
set X_src 60.0
set Y_src 100.0
set X_dst 200.0
set Y_dst 100.0

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
AddrParams set domain_num_  7                      ;# domain number
AddrParams set cluster_num_ {1 1 1 1 1 1 1}          ;# cluster number for each domain 
# 1st cluster: nb of mobile nodes
# 2nd cluster: CN
# 3rd cluster: core network
# 4th cluster: WLAN: 1BS + nb of mobile nodes
# 5th cluster: super nodes
lappend tmp 1                                      ;# router CN
lappend tmp 1                                      ;# router 1
lappend tmp 3                                      ;# 802.11 MNs+BS
lappend tmp 3                                      ;# 802.11 MNs+BS
lappend tmp 3                                      ;# 802.11 MNs+BS
lappend tmp 1                                      ;# MULTIFACE nodes
lappend tmp 1                                      ;# MULTIFACE nodes
AddrParams set nodes_num_ $tmp

# Node address for router0 and router1 are 4 and 5, respectively.
set CN [$ns node 0.0.0]
$CN install-default-ifmanager

set router1 [$ns node 1.0.0]
if {$quiet == 0} {
    puts "CN: tcl=$CN; id=[$CN id]; addr=[$CN node-addr]"
    puts "router1: tcl=$router1; id=[$router1 id]; addr=[$router1 node-addr]"
}
 
# connect links 
$ns duplex-link $router1 $CN 100MBit 30ms DropTail 1000

# creation of the MutiFaceNodes. It MUST be done before the 802.11
$ns node-config  -multiIf ON                       ;#to create MultiFaceNode 

set multiFaceNode(0) [$ns node 5.0.0]
set multiFaceNode(1) [$ns node 6.0.0]

$ns node-config  -multiIf OFF                      ;#reset attribute
if {$quiet == 0} {
    puts "multiFaceNode(s) has/have been created"
}

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
set opt(nbBS)           3
set opt(x)		400			   ;# X dimension of the topography
set opt(y)		400			   ;# Y dimension of the topography
set opt(interference)   0                          ;# Disable/Enable interference
set opt(nbMN)           1
set opt(mnSender)       1                          ;# 1 if MN is sender, 0 if receiver

#create the topography
set topo [new Topography]
$topo load_flatgrid $opt(x) $opt(y)

if {$quiet == 0} {
    puts "Topology created"
}

# create God
create-god $opt(nbMN)				   ;# give the number of nodes 


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


# configure each Base station 802.11
for {set i 0} {$i < $opt(nbBS) } {incr i} {
    set bstation802($i) [$ns node  [expr (2+$i)].0.0]
    $bstation802($i) set X_ [expr 50.0 + $i*75]  
    $bstation802($i) set Y_ 100.0
    $bstation802($i) set Z_ 0.0
    if {$quiet == 0} {
	puts "bstation802: tcl=$bstation802($i); id=[$bstation802($i) id]; addr=[$bstation802($i) node-addr] X=[expr 50.0 + $i*75] Y=100.0"
    }
    # we need to set the BSS for the base station
    set bstationMac($i) [$bstation802($i) getMac 0]
    #$bstationMac($i) set-channel [expr ($i*5+1) % 15]
    $bstationMac($i) set-channel [expr ($i+1)]
    set AP_ADDR($i) [$bstationMac($i) id]
    if {$quiet == 0} {
	#puts "bss_id for bstation=$AP_ADDR($i) channel=[expr ($i*5+1) % 15]"
	puts "bss_id for bstation=$AP_ADDR($i) channel=[expr ($i+1)]"
    }
    $bstationMac($i) bss_id $AP_ADDR($i)
    $bstationMac($i) enable-beacon

    if { $opt(interference) == 1} {
    	set bstationPhy($i) [$bstation802($i) set netif_(0)]
    	$bstationPhy($i) enableInterference
	$bstationPhy($i) setTechno 802.11
    }
}



# creation of the wireless interface 802.11
$ns node-config -wiredRouting OFF \
                -macTrace ON 		

for {set i 0} {$i < $opt(nbMN) } {incr i} {
    set iface($i) [$ns node [expr (2+$i)].0.[expr 1+$i]]                                ;#node id is 5
    $iface($i) random-motion 0		                                      ;#disable random motion
    $iface($i) base-station [AddrParams addr2id [$bstation802($i) node-addr]] ;#attach mn to basestation
    $iface($i) set X_ [expr $X_src + $i*75]
    $iface($i) set Y_ [expr $Y_src]
    $iface($i) set Z_ 0.0
    [$iface($i) getMac 0] set-channel [expr 1+$i]

    if { $opt(interference) == 1} {
	set MNPhy($i) [$iface($i) set netif_(0)]
	$MNPhy($i) enableInterference
	$MNPhy($i) setTechno 802.11
    }
    
    if {$quiet == 0} {
	puts "Iface $i = $iface($i) X=[expr $X_src+$i*75] Y=$Y_src"
    }
    
    #add the interfaces to supernode
    $multiFaceNode($i) add-interface-node $iface($i)
}

#calculate the speed of the node
$ns at $moveStart "$iface(0) setdest $X_dst $Y_dst $moveSpeed"


# add link to backbone
for {set i 0} {$i < $opt(nbBS) } {incr i} {
    $ns duplex-link $bstation802($i) $router1 100MBit 15ms DropTail 1000
}

# configure WLAN interface of each Base Satation
for {set i 0} {$i < $opt(nbBS) } {incr i} {
    set nd_bs($i) [$bstation802($i) install-nd]
    $nd_bs($i) set-router TRUE
    $nd_bs($i) router-lifetime 18
    $ns at 1 "$nd_bs($i) start-ra"
    set mih_bs($i) [$bstation802($i) install-mih]
    set tmp2($i) [$bstation802($i) set mac_(0)] ;#in 802.11 one interface is created
    $tmp2($i) mih $mih_bs($i)
    $mih_bs($i) add-mac $tmp2($i)
}

for {set i 0} {$i < $opt(nbMN) } {incr i} {
    set nd_mn($i) [$iface($i) install-nd]
    set handover($i) [new Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1]
    $multiFaceNode($i) install-ifmanager $handover($i)
    $nd_mn($i) set-ifmanager $handover($i)


    # install MIH in multi-interface node
    set mih($i) [$multiFaceNode($i) install-mih]
    $handover($i) connect-mih $mih($i)
    set tmp3($i) [$iface($i) set mac_(0)]
    $handover($i) nd_mac $nd_mn($i) $tmp3($i)
    $tmp3($i) mih $mih($i)
    $mih($i) add-mac $tmp3($i)

    #Create a UDP agent and attach it to node n0
    set udp($i) [new Agent/UDP]
    $udp($i) set packetSize_ 1500

    if {$quiet == 0} {
	puts "udp on node : $udp($i)"
    }

    # Create a CBR traffic source and attach it to udp0
    set cbr($i) [new Application/Traffic/CBR]
    $cbr($i) set packetSize_ 500
    $cbr($i) set interval_ 0.5
    $cbr($i) attach-agent $udp($i)

    #create an sink into the sink node
    # Create the Null agent to sink traffic
    set null($i) [new Agent/Null] 
    
    if { $opt(mnSender) == 1} {
	#CN is receiver    
	$ns attach-agent $CN $null($i)
	
	#Multiface node is transmitter
	$multiFaceNode($i) attach-agent $udp($i) $iface($i)
	$handover($i) add-flow $udp($i) $null($i) $iface($i) 1 2000.    
    } else {
	#Multiface node is receiver    
	$multiFaceNode($i) attach-agent $null($i) $iface($i)
	$handover($i) add-flow $null($i) $udp($i) $iface($i) 1 2000.    
	
	#CN is transmitter
	$ns attach-agent $CN $udp($i) 
    }

    #Start the application 1sec before the MN is entering the WLAN cell
    $ns at [expr $moveStart - 1] "$cbr($i) start"

    #Stop the application according to another poisson distribution (note that we don't leave the 802.11 cell)
    $ns at [expr $moveStop  + 1] "$cbr($i) stop"
}

# set original status of interface. By default they are up..so to have a link up, 
# we need to put them down first.

$ns at $moveStart "puts \"At $moveStart Mobile Node starts moving\""
$ns at [expr $moveStart+10] "puts \"++At [expr $moveStart+10] Mobile Node enters wlan\""
$ns at [expr $moveStart+110] "puts \"++At [expr $moveStart+110] Mobile Node leaves wlan\""
$ns at $moveStop "puts \"Mobile Node stops moving\""
$ns at [expr $moveStop + 40] "puts \"Simulation ends at [expr $moveStop+40]\"" 
$ns at [expr $moveStop + 40] "finish" 

if {$quiet == 0} {
puts " Simulation is running ... please wait ..."
}

$ns run
