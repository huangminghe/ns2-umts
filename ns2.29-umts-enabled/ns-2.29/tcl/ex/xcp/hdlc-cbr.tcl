#
# Use simple mac rather than 802.11
#   
#
set val(chan)           Channel/WirelessChannel    ;#Channel Type
set val(prop)           Propagation/TwoRayGround   ;# radio-propagation model
set val(netif)          Phy/WirelessPhy            ;# network interface type
set val(mac)            Mac/Simple                 ;# MAC type
#set val(ifq)            Queue/DropTail/PriQueue    ;# interface queue type
set val(ifq)            Queue/XCP
set val(ll)             LL/HDLC                    ;# link layer type
set val(ant)            Antenna/OmniAntenna        ;# antenna model
#set val(err)            ComplexMarkovErrorModel
set val(err)            MarkovErrorModel
#set val(err)             UniformErrorProc
set val(FECstrength)    1
set val(ifqlen)         50                         ;# max packet in ifq
set val(nn)             2                          ;# number of mobilenodes

# routing protocol
set val(rp)              DumbAgent  
#set val(rp)             DSDV                     
#set val(rp)             DSR                      
#set val(rp)             AODV                     

set val(x)		250
set val(y)		250

#set durlistA        "11.2 4.0"      ;# unblocked and blocked state duration avg value
set durlistA         "5.0 1.0"   ;# for allowing more timeouts
set durlistB        "27.0 12.0 0.4 0.4"  ;# for complexmarkoverror model

set stoptime        50.0

# set bandwidth and delay
LL set bandwidth_ 20Mb
LL set delay_ 100ms

# setting mac-simple to full duplex mode
Mac/Simple set fullduplex_mode_ 1

# doing selective repeat
#LL/HDLC set selRepeat_ 1


proc UniformErrorProc {} {
	global val
	set	errObj [new ErrorModel]
	$errObj unit bit
	#$errObj FECstrength $val(FECstrength) 
	#$errObj datapktsize 512
	#$errObj cntrlpktsize 80
	return $errObj
}

proc MarkovErrorModel {} {
	global durlistA
	
	puts [lindex $durlistA 0]
	puts [lindex $durlistA 1]

	set errmodel [new ErrorModel/TwoStateMarkov $durlistA time]
	
	$errmodel drop-target [new Agent/Null]
	return $errmodel
}

proc ComplexMarkovErrorModel {} {
	global durlistB

	set errmodel [new ErrorModel/ComplexTwoStateMarkov $durlistB time]
	
	$errmodel drop-target [new Agent/Null] 
	return $errmodel
}


# Initialize Global Variables
set ns_		[new Simulator]
set tracefd     [open xcp-wireless.tr w]
$ns_ trace-all $tracefd

#for event-tracing
#$ns_ eventtrace-all

# set up topography object
set topo       [new Topography]

$topo load_flatgrid $val(x) $val(y)

# Create God
create-god $val(nn)

# Create channel
set chan_ [new $val(chan)]

# Create node(0) and node(1)

# configure node, please note the change below.

$ns_ node-config -adhocRouting $val(rp) \
    -llType $val(ll) \
    -macType $val(mac) \
    -ifqType $val(ifq) \
    -ifqLen $val(ifqlen) \
    -antType $val(ant) \
    -propType $val(prop) \
    -phyType $val(netif) \
    -topoInstance $topo \
    -agentTrace ON \
    -routerTrace ON \
    -macTrace ON \
    -movementTrace ON \
    -channel $chan_ \
    -OutgoingErrProc $val(err)

set node_(0) [$ns_ node]

$ns_ node-config -adhocRouting $val(rp) \
    -llType $val(ll) \
    -macType $val(mac) \
    -ifqType $val(ifq) \
    -ifqLen $val(ifqlen) \
    -antType $val(ant) \
    -propType $val(prop) \
    -phyType $val(netif) \
    -topoInstance $topo \
    -agentTrace ON \
    -routerTrace ON \
    -macTrace ON \
    -movementTrace ON \
    -channel $chan_ \
    -OutgoingErrProc $val(err)

set node_(1) [$ns_ node]

for {set i 0} {$i < $val(nn)} {incr i} {
	#set node_($i) [$ns_ node]
	$node_($i) random-motion 0
	$ns_ initial_node_pos $node_($i) 20
}

#
# Provide initial (X,Y, for now Z=0) co-ordinates for mobilenodes
#
$node_(0) set X_ 15.0
$node_(0) set Y_ 15.0
$node_(0) set Z_ 0.0

$node_(1) set X_ 150.0
$node_(1) set Y_ 150.0
$node_(1) set Z_ 0.0

#
# Now produce some simple node movements
# Node_(1) starts to move towards node_(0)
#
#$ns_ at 0.0 "$node_(0) setdest 50.0 50.0 5.0"
#$ns_ at 0.0 "$node_(1) setdest 60.0 40.0 10.0"


# Node_(1) then starts to move away from node_(0)
#$ns_ at 3.0 "$node_(1) setdest 240.0 240.0 30.0" 

# Setup traffic flow between nodes
# TCP connections between node_(0) and node_(1)

# set tcp [new Agent/TCP/Reno]
# $tcp set class_ 2
# set sink [new Agent/TCPSink]
# $ns_ attach-agent $node_(0) $tcp
# $ns_ attach-agent $node_(1) $sink
# $ns_ connect $tcp $sink
# set ftp [new Application/FTP]
# $ftp attach-agent $tcp
# $ns_ at 0.0 "$ftp start" 

set udp [new Agent/UDP]
$ns_ attach-agent $node_(1) $udp
set cbr [new Application/Traffic/CBR]
$cbr set packetSize_ 512
#$cbr set interval_ 3.75ms
$cbr set rate_ 100Kb
$cbr set maxpkts_ 2000
$cbr attach-agent $udp

set null [new Agent/Null]
$ns_ attach-agent $node_(0) $null

$ns_ connect $udp $null
$ns_ at 0.0 "$cbr start"




#
# Tell nodes when the simulation ends
#
for {set i 0} {$i < $val(nn) } {incr i} {
    $ns_ at $stoptime "$node_($i) reset";
}
$ns_ at $stoptime "stop"
$ns_ at $stoptime "puts \"NS EXITING...\" ; $ns_ halt"

proc stop {} {
    global ns_ tracefd
    $ns_ flush-trace
    close $tracefd
}

puts "Starting Simulation..."
$ns_ run
