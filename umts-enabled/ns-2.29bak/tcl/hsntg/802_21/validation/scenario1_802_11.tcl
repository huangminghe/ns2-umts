#check input parameters
if {$argc != 0} {
	puts ""
	puts "Wrong Number of Arguments! No arguments in this topology"
	puts ""
	exit (1)
}


set output_dir .
# Define global simulation parameters

#define DEBUG parameters
set quiet 0
Agent/ND set debug_ 0
Agent/MIH set debug_ 1
Agent/MIHUser/IFMNGMT/MIPV6/Handover/Simple set debug_ 1
Mac/802_11 set debug_ 1

Mac/802_11 set bss_timeout_ 5
Mac/802_11 set pr_limit_ 1.2 ;#for link going down
Mac/802_11 set client_lifetime_ 40.0 ;# since we don't have traffic, the AP would disconnect the MN automatically after client_lifetime_

#define coverage area for base station: 50m coverage 
Phy/WirelessPhy set Pt_ 0.0134
Phy/WirelessPhy set freq_ 2412e+6
Phy/WirelessPhy set RXThresh_ 5.25089e-10
Phy/WirelessPhy set CSThresh_ [expr 0.9 * 5.25089e-10]

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

set opt(x)		200			   ;# X dimension of the topography
set opt(y)		200			   ;# Y dimension of the topography

Mac/802_11 set basicRate_ 11Mb
Mac/802_11 set dataRate_ 11Mb
Mac/802_11 set bandwidth_ 11Mb

#defines function for flushing and closing files
proc finish {} {
        global ns tf output_dir nb_mn
        $ns flush-trace
        close $tf
    #exec nam out.nam &
	exit 0
}

#create the simulator
set ns [new Simulator]

#set nf [open out.nam w]
#$ns namtrace-all $nf

#$ns use-newtrace

#create the topography
set topo [new Topography]
$topo load_flatgrid $opt(x) $opt(y)
puts "Topology created"

#open file for trace
set tf [open $output_dir/out.res w]
$ns trace-all $tf
#puts "Output file configured"

$ns node-config -addressType hierarchical
AddrParams set domain_num_  2                     ;# domain number
AddrParams set cluster_num_ {1 1}                   ;# cluster number for each domain b
lappend tmp 2                                     ;# 802.11 MNs+BS
lappend tmp 1                                     ;# multi-interface node
AddrParams set nodes_num_ $tmp

# create God
create-god 3                                            ;# 3 nodes here

puts "created wireless channels"

# configure nodes
# creation of the MutiFaceNodes. It MUST be done before the 802.11
# Multi-interface nodes are virtual and only serves to link interfaces
$ns node-config  -multiIf ON                      ;#to create MultiFaceNode 
set multiFaceNode [$ns node 1.0.0] 
$ns node-config  -multiIf OFF                     ;#reset attribute

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

puts "did node configure"

# configure Base station 802.11
set bstation802 [$ns node 0.0.0];
$bstation802 set X_ 100.0
$bstation802 set Y_ 100.0
$bstation802 set Z_ 0.0
puts "bstation802: tcl=$bstation802; id=[$bstation802 id]; addr=[$bstation802 node-addr]"
# we need to set the BSS for the base station
set bstationMac [$bstation802 getMac 0]
set AP_ADDR_0 [$bstationMac id]
puts "bss_id for bstation 1=$AP_ADDR_0"
$bstationMac bss_id $AP_ADDR_0
$bstationMac enable-beacon
$bstationMac set-channel 1

# configure MN interface
$ns node-config -wiredRouting OFF \
                -macTrace ON 	

set iface [$ns node 0.0.1]
$iface set X_ 110.0
$iface set Y_ 100.0
$iface set Z_ 0.0
$iface random-motion 0
#[$node1 getMac 0] bss_id $AP_ADDR_0
$iface base-station [AddrParams addr2id [$bstation802 node-addr]] ;#attach mn to basestation

$multiFaceNode add-interface-node $iface

puts "configured nodes"

#################################
#
# Set up mih here... not the easiest thing for me to do 
#
#################################

# for bstation802: 

set nd_bs [$bstation802 install-nd]
$nd_bs set-router TRUE
$nd_bs router-lifetime 18
$ns at 1 "$nd_bs start-ra"

set mih_bs [$bstation802 install-mih]
set tmp_bs [$bstation802 set mac_(0)] 
$tmp_bs mih $mih_bs
$mih_bs add-mac $tmp_bs


# for node1:

set nd_mn [$iface install-nd]

set handover [new Agent/MIHUser/IFMNGMT/MIPV6/Handover/Simple]
$multiFaceNode install-ifmanager $handover
$nd_mn set-ifmanager $handover

set mih [$multiFaceNode install-mih]
$handover connect-mih $mih
set tmp [$iface set mac_(0)]
#$ifmgmt nd_mac $nd_mn [$node1 set mac_(0)]
$handover nd_mac $nd_mn $tmp
# add the handover module for the Interface Management

$tmp mih $mih
$mih add-mac $tmp


#$ifmgmt2 nd_mac $nd_mn2 [$bstation802 set mac_(0)]

# now that these are set up, we need to
# figure out how to start sending capability request
# packets
#
###############################################

# node movement
$ns at 5 "$iface setdest 199 100 5"
$ns at 13 "$iface setdest 105 100 5"
$ns at 18 "$iface setdest 199 100 5"

$ns at 50 "finish"

$ns run
