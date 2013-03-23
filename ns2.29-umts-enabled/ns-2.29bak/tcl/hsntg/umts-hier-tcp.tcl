
# Name: appTahoe-ftp.tcl
# The Uu interface is assumed to be ideal.
# TCP Tahoe

global ns

set nb_mn 1 				
set node_odd [expr $nb_mn % 2]
set packet_size	1000			;# packet size in bytes at CBR applications 
set output_dir umts
set bw [expr 1024*1024]      			;# 48KB/s=384Kb/s
#set gap_size [expr ($packet_size*8.0*$nb_mn)/$bw]
set gap_size [expr ($packet_size*8.0)/$bw]
puts "gap_size $gap_size"

#remove-all-packet-headers
#add-packet-header MPEG4 MAC_HS RLC LL Mac RTP TCP UDP IP Common Flags


set ns [new Simulator]

set f [open out.tr w]
$ns trace-all $f

proc finish {} {
    global ns nb_mn
    global f
    $ns flush-trace
    close $f
    #exec ./datarate out.tr datarate.res [expr $nb_mn+4] [expr $nb_mn+5]
    puts " Simulation ended."
    exit 0
}

$ns set debug_ 1

$ns set hsdschEnabled_ 1addr
$ns set hsdsch_rlc_set_ 0
$ns set hsdsch_rlc_nif_ 0

# set up for hierarchical routing
# (needed for routing over a basestation)
set num_wired_nodes 4
set num_mobile_nodes $nb_mn
set num_bs_nodes 2
$ns node-config -addressType hierarchical
AddrParams set domain_num_  5          ;# domain number
AddrParams set cluster_num_ {1 1 1 1 1}       ;# cluster number for each domain 
AddrParams set nodes_num_  {4 1 1 1 1}	;# number of nodes for each cluster             

$ns node-config -UmtsNodeType rnc 

# Node address is 0.
set rnc [$ns create-Umtsnode 0.0.0]
puts "rnc $rnc"

$ns node-config -UmtsNodeType bs \
		-downlinkBW 384kbs \
		-downlinkTTI 10ms \
		-uplinkBW 384kbs \
		-uplinkTTI 10ms \
     		-hs_downlinkTTI 2ms \
      		-hs_downlinkBW 384kbs \


# Node address is 1.
set bs [$ns create-Umtsnode 0.0.1]
puts "bs $bs"

$ns setup-Iub $bs $rnc 622Mbit 622Mbit 15ms 15ms DummyDropTail 2000

$ns node-config -UmtsNodeType ue \
		-baseStation $bs \
		-radioNetworkController $rnc

# Node address for ue1 and ue2 is 2 and 3, respectively.
for {set i 0} {$i < $nb_mn} {incr i} {
   set ue($i) [$ns create-Umtsnode 0.0.[expr 2+$i]]
  puts "ue($i) created $ue($i)" 
}
if { $node_odd == 1 } {
    set dummy_node [$ns create-Umtsnode 0.0.[expr 2+$nb_mn]]
    puts "Creating dummy node $dummy_node"
}

# Node address for sgsn0 and ggsn0 is 4 and 5, respectively.
set sgsn0 [$ns node 1.0.0]
puts "sgsn0 $sgsn0"
set ggsn0 [$ns node 2.0.0]
puts "ggsn0 $ggsn0"
# Node address for node1 and node2 is 6 and 7, respectively.
set node1 [$ns node 3.0.0]
puts "node1 $node1"
set node2 [$ns node 4.0.0]
puts "node2 $node2"

$ns duplex-link $rnc $sgsn0 622Mbit 0.4ms DropTail 1000
$ns duplex-link $sgsn0 $ggsn0 622MBit 10ms DropTail 1000
$ns duplex-link $ggsn0 $node1 10MBit 15ms DropTail 1000
$ns duplex-link $node1 $node2 10MBit 35ms DropTail 1000
$rnc add-gateway $sgsn0

for {set i 0} {$i < 1} {incr i} {
    #create source traffic 3: from ethernet interface to router0
    #Create a TCP agent and attach it to multi-interface node
    set tcp_(0) [new Agent/TCP/FullTcp]
    $ns attach-agent $ue(0) $tcp_(0) ;# the interface is used for sending
    set app_(0) [new Application/TcpApp $tcp_(0)]
    puts "App0 id=$app_(0)"
    
    #create an sink into the MN1
    # Create the Null agent to sink traffic
    set tcp_(1) [new Agent/TCP/FullTcp] 
    $ns attach-agent $node2 $tcp_(1)
    set app_(1) [new Application/TcpApp $tcp_(1)]
    puts "App1 id=$app_(1)"
    
    $ns connect $tcp_(0) $tcp_(1)
    $tcp_(0) listen
    puts "udp stream made from udp($i) and sink($i)"
}

$ns node-config -llType UMTS/RLC/AM \
		-downlinkBW 384kbs \
		-uplinkBW 384kbs \
		-downlinkTTI 20ms \
		-uplinkTTI 20ms \
   		-hs_downlinkTTI 2ms \
    		-hs_downlinkBW 384kbs


$ns create-hsdsch $ue(0) $tcp_(0)

for {set i 1} {$i < 1} {incr i} {
	$ns attach-hsdsch $ue($i) $udp($i)
}

$bs setErrorTrace 0 "idealtrace"
$bs setErrorTrace 1 "idealtrace"
$bs loadSnrBlerMatrix "SNRBLERMatrix"

$bs trace-outlink $f 2
$ue(0) trace-inlink $f 2
$ue(0) trace-outlink $f 3

# we cannot start the connect right away. Give time to routing algorithm to run
$ns at 0.5 "$app_(1) connect $app_(0)"

# install a procedure to print out the received data
Application/TcpApp instproc recv {data} {
    global ns
    $ns trace-annotate "$self received data \"$data\""
    puts "$self received data \"$data\""
}

# send a message via TcpApp
# The string will be interpreted by the receiver as Tcl code.
for { set i 1 } { $i < 5 } { incr i} {
    $ns at [expr $i + 0.5] "$app_(1) send 100 {$app_(0) recv {my mesage $i}}"
}

#for {set i 0} {$i < 1} {incr i} {
#	$ns at 10 "$cbr($i) start"
#	$ns at 11.0 "$cbr($i) stop"
#}
$ns at 11.401 "finish"

puts " Simulation is running ... please wait ..."
$ns run
