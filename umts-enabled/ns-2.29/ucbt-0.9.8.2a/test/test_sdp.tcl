set val(mac)            Mac/BNEP                 ;# MAC type
set val(nn)             8                          ;# number of mobilenodes

set StartTime [list 0.0 0.0006 0.1031 0.1134 0.3878 0.8531 0.6406 0.0627]

set ns_		[new Simulator]

$ns_ node-config -macType $val(mac) 	;# set node type to BTNode

for {set i 0} {$i < $val(nn) } {incr i} {
	set node($i) [$ns_ node $i ]
        $node($i) rt AODV
	[$node($i) set bb_] set ver_ 11
	$ns_ at [lindex $StartTime $i] "$node($i) on"
}

set sdpcli [$node(0) set sdp_]
$ns_ at  1 "$sdpcli test"

$ns_ at 10.01 "$ns_ halt"

$ns_ run

