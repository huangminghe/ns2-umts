#检查输入
if {$argc != 0} {
	puts ""
	puts "Wrong Number of Arguments! No arguments in this topology"
	puts ""
	exit (1)
}
global ns

#定义结束过程
proc finish {} {
    global ns f
    $ns flush-trace
    close $f
    puts " Simulation ended."
    exit 0
}

# function: create UDP Agent for UE
# args
#     ue: ue which need agent
#     agent_name: agent name
#     interface: the network interface which ue is using
proc createUDPAgentForUE {ue agent_name interface} {
    set agent_name [new Agent/UDP]
    $ue attach-agent $agent_name $interface
}

# function: create FullTcp Agent for UE
# args
#     ue: ue which need agent
#     agent_name: agent name
#
proc createTCPAgentForUE {ue agent_name} {
    set agent_name [new Agent/TCP/FullTcp]
    $ue attach-agent $agent_name $interface
}

# function: connect send agent and recieve agent of two UEs 
# args
#     agent_src: source agent
#     agent_des: destination agent
#
proc connectTwoAgents {agent_src agent_des} {
  $ns connect $agent_ue1 $agent_ue2
}

# function: set Node position
# args
#  	node: umts node
#  	pos_x: X_ value
#  	pos_y: Y_ value
#  	pos_z: Z_ value
proc setNodePosition {node pos_x pos_y pos_z} {
	$node set X_ $pos_x
	$node set Y_ $pos_y
	$node set Z_ $pos_z
}

# function: set random position for node
# args
# 	node: node
# 	min: minimum value you want
# 	max: maximum value you want
proc setRandomPositionForNode {node min max} {
	set rand_x [expr rand()]
	set rand_y [expr rand()]
	set pos_x [expr $rand_x * ($max - $min) + $min]
	set pos_y [expr $rand_y * ($max - $min) + $min]
	$node set X_ $pos_x
	$node set Y_ $pos_y
	$node set Z_ 0.0
}

# funcition: make umts ue move by setting the X_ and Y_ value
# args 
# 	umts_ue: umts node
proc moveUmtsNode {umts_ue} {
	set prev_x [$umts_ue set X_]
	set prev_y [$umts_ue set Y_]
	set rand_x [expr rand()]
	set rand_y [expr rand()]
	set mov_x [expr $rand_x * (2 - (-2)) + (-2)]
	set mov_y [expr $rand_y * (2 - (-2)) + (-2)]
	$umts_ue set X_ [expr $prev_x + $mov_x]
	$umts_ue set Y_ [expr $prev_y + $mov_y]
}

# function: move umts node every certain time slot
# args:
# 	umts_ue: umts node
# 	time_slot: time slot
# 	from_time: time to start to move
# 	time_length: the length of whole time
proc umtsNodeMove {umts_ue from_time time_slot time_lenght} {
	global ns
	for {set i 0} {$i < $time_lenght} {incr i 1} {
		$ns at [expr $from_time + $i * $time_slot] "moveUmtsNode $umts_ue"	
	}
}

# function: print position of node
# args: 
# 	node: node
proc printNodePosition {node} {
	set tmp_x [$node set X_]
	set tmp_y [$node set Y_]
	puts "X: $tmp_x Y:$tmp_y"	
}
# function: print Node id
# args 
# 	node: node name
proc printNodeID {node node_name} {
	set node_id [$node id]
	puts "$node_name id=$node_id"
}

# function: do handover action
# args
#     terminal: the UE which want to handover
#     interface: the network interface which ue want to handover to
#     sent_agent: agent for sending
#     receieve_agent: agent for receiving

proc redirectTraffic {terminal interface send_agent recieve_agent} {
    puts "handover happening now"
	$terminal attach-agent $send_agent $interface 
	$terminal connect-agent $send_agent $recieve_agent $interface  
}

# function: record position of node 
# args
#     ue: the UE we want to record 
proc record {ue} {
   set ns [Simulator instance]
   set time 0.1;# record 0.5 second
   set pos_x [$ue set X_]
   set pos_y [$ue set Y_]
   set now [$ns now]

   puts "$pos_x\t$pos_y"
   $ns at [expr $now + $time] "record $ue"
}

proc getMeanDelay {} {
  set trace_tmp [new Agent/RealtimeTrace]
  set mean_delay [$trace_tmp GetMeanDelay "5.0.0" "3.0.2" "cbr" ]
  puts "mean_delay is $mean_delay"
  set current_delay [$trace_tmp GetCurrentDelay "5.0.0" "3.0.2" "cbr"]
  puts "current_delay is $current_delay"
}
#当前路径
set output_dir .

#创建仿真实例
set ns [new Simulator]
#$ns use-newtrace

$ns color 1 Blue
$ns color 2 Red
#将仿真过程写入追踪文件
set f [open out.tr w]
$ns trace-all $f
set namfile [open out.nam w]
$ns namtrace-all-wireless $namfile 800 800 

# 配置 UMTS
$ns set hsdschEnabled_ 1addr
$ns set hsdsch_rlc_set_ 0
$ns set hsdsch_rlc_nif_ 0

# 配置层次结构（域：簇：ip号） (needed for routing over a basestation)
$ns node-config -addressType hierarchical
AddrParams set domain_num_  25           	           			;# 域数目
AddrParams set cluster_num_ {1  1 1 1  1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1}            		;# 簇数目 
AddrParams set nodes_num_   {22 1 1 21 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1}	      			;# 每个簇的节点数目             

#配置RNC 
puts "##############################################################"
puts "***********************Now, Creating RNC**********************"

$ns node-config -UmtsNodeType rnc 
set RNC [$ns create-Umtsnode 0.0.0] ;# node id is 0.
setNodePosition $RNC 200 100 0 
$RNC color "yellow"
$ns initial_node_pos $RNC 20
printNodeID $RNC "RNC"

# 配置UMTS基站
$ns node-config -UmtsNodeType bs \
		-downlinkBW 384kbs \
		-downlinkTTI 10ms \
		-uplinkBW 384kbs \
		-uplinkTTI 10ms \
     		-hs_downlinkTTI 2ms \
      		-hs_downlinkBW 384kbs 

set BS [$ns create-Umtsnode 0.0.1] ;# node id is 1 基站和RNC处在同一个域
setNodePosition $BS 100 100 0
$BS color "blue"
$ns initial_node_pos $BS 10
printNodeID $BS "BS"

#链接BS和RNC 
$ns setup-Iub $BS $RNC 622Mbit 622Mbit 15ms 15ms DummyDropTail 2000

#创建UMTS无线节点
$ns node-config -UmtsNodeType ue \
		-baseStation $BS \
		-radioNetworkController $RNC

#umts_ue(0)携带的是UMTS的网卡，可作为后面多面终端的网卡之一
for {set i 0} {$i < 20} {incr i 1} {
	set umts_ue($i) [$ns create-Umtsnode 0.0.[expr $i+2]]
	setRandomPositionForNode $umts_ue($i) 50 400
	printNodeID $umts_ue($i) "umts_ue($i)"
	printNodePosition $umts_ue($i)
	$umts_ue($i) color "red"
	$ns initial_node_pos $umts_ue(0) 5
	umtsNodeMove $umts_ue($i) 1.0 0.2 5.0
}

for {set i 0} {$i < 20} {incr i 1} {
	$ns at 2.0 "printNodePosition $umts_ue($i)"
}

#创建一个虚假节点，只是仿真的顺利需要。
# it seems no need anymore
#set dummy_node [$ns create-Umtsnode 0.0.3] ;# node id is 3
#$dummy_node set Y_ 150
#$dummy_node set X_ 100 
#$dummy_node set Z_ 0
#$dummy_node color "brown"

#创建SGSN 和GGSN。处在不同的域里。Node id for SGSN0 and GGSN0 are 4 and 5, respectively.

set SGSN [$ns node 1.0.0]
setNodePosition $SGSN 300 100 0 
printNodeID $SGSN "SGSN"
$SGSN color "blue"
$ns initial_node_pos $SGSN 10

set GGSN [$ns node 2.0.0]
setNodePosition $GGSN 350 100 0
printNodeID $GGSN "GGSN"
$GGSN color "blue"
$ns initial_node_pos $GGSN 10

#创建两个节点，在这里这两个节点我们可以这是核心网的节点。
set CN_host0 [$ns node 4.0.0]
setNodePosition $CN_host0 400 100 0 
printNodeID $CN_host0 "CN_host0"
$CN_host0 color "black"
$ns initial_node_pos $CN_host0 5

puts "finished"
puts ""
# do the connections in the UMTS part
puts "Connecting RNC SGSN GGSN CN_host0 CN host1"

$ns duplex-link $RNC $SGSN 622Mbit 0.4ms DropTail 1000
$ns duplex-link $SGSN $GGSN 622MBit 10ms DropTail 1000
$ns duplex-link $GGSN $CN_host0 10MBit 15ms DropTail 1000

$RNC add-gateway $SGSN                                  		;#这一句应该放在链路搭建完成之后，一般情况放在这个位置

#添加WLAN网络。 

# parameter for wireless nodes
set opt(chan)           Channel/WirelessChannel   			;# channel type for 802.11
set opt(prop)           Propagation/TwoRayGround   			;# radio-propagation model 802.11
set opt(netif)          Phy/WirelessPhy            			;# network interface type 802.11
set opt(mac)            Mac/802_11                 			;# MAC type 802.11
set opt(ifq)            Queue/DropTail/PriQueue    			;# interface queue type 802.11
set opt(ll)             LL                         			;# link layer type 802.11
set opt(ant)            Antenna/OmniAntenna        			;# antenna model 802.11
set opt(ifqlen)         50              	   			      ;# max packet in ifq 802.11
set opt(adhocRouting)   DSDV                       			;# routing protocol 802.11
set opt(umtsRouting)    ""                         			;# routing for UMTS (to reset node config)

set opt(x)    1000 			   			                        ;# X dimension of the topography
set opt(y)		1000			   			                        ;# Y dimension of the topography

#配置WLAN的速率为11Mb 
Mac/802_11 set basicRate_ 11Mb
Mac/802_11 set dataRate_ 11Mb
Mac/802_11 set bandwidth_ 11Mb
Mac/802_11 set client_lifetime_ 10 					            ;#increase since iface 2 is not sending traffic for some time

#配置拓扑
puts "1 Creating topology"
puts ""
set topo [new Topography]
$topo load_flatgrid $opt(x) $opt(y)

puts "Topology created"
puts ""

# create God
set god [create-god 66]				                		     ;# give the number of nodes 

#创建多摸节点 
puts "2 Creating UE"
puts ""
$ns node-config  -multiIf ON                            		;#to create MultiFaceNode
for {set i 0} {$i < 20} {incr i 1} {
	set ue($i) [$ns node [expr 5+$i].0.0]
}
$ns node-config  -multiIf OFF                           		;#reset attribute

#设置节点之间的最小跳数，减少计算时间。
$god set-dist 1 2 1 
$god set-dist 0 2 2 
$god set-dist 0 1 1 
set god [God instance]


puts "finished"
puts ""

#配置接入点AP 
puts "3 Creating AP"
puts ""

puts "coverge:20m"

Phy/WirelessPhy set Pt_ 0.025
Phy/WirelessPhy set RXThresh_ 2.025e-12
Phy/WirelessPhy set CSThresh_ [expr 0.9*[Phy/WirelessPhy set RXThresh_]]

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
                 -agentTrace OFF\
                 -routerTrace OFF \
                 -macTrace ON  \
                 -movementTrace OFF	

# configure Base station 802.11
set AP0 [$ns node 3.0.0]
setNodePosition $AP0 200 50 0 
printNodeID $AP0 "AP0"
$AP0 color "red"
$ns initial_node_pos $AP0 10

[$AP0 set mac_(0)] bss_id [[$AP0 set mac_(0)] id]
[$AP0 set mac_(0)] enable-beacon
[$AP0 set mac_(0)] set-channel 1

# creation of the wireless interface 802.11
puts "5 Creating WLAN UEs"
puts ""
$ns node-config -wiredRouting OFF \
                -macTrace ON 				
for {set i 0} {$i < 20} {incr i 1} {
	set wlan_ue($i) [$ns node 3.0.[expr $i+1]]
	$wlan_ue($i) random-motion 1
	$wlan_ue($i) base-station [AddrParams addr2id [$AP0 node-addr]]
	setRandomPositionForNode $wlan_ue($i) 100 300 
	printNodeID $wlan_ue($i) "wlan_ue($i)"
	printNodePosition $wlan_ue($i)
}

# add link to backbone
puts "5 Connecting AP0 to RNC and Connecting AP1 to SGSN"
puts ""
$ns duplex-link $AP0 $GGSN 10MBit 15ms DropTail 1000

# add interfaces to MultiFaceNode
for {set i 0} {$i < 20} {incr i 1} {
	$ue($i) add-interface-node $wlan_ue($i)
	$ue($i) add-interface-node $umts_ue($i)
}

puts "***********************Completed successfully*****************"
puts "##############################################################"
puts ""
puts ""

# create a TCP agent and attach it to multi-interface node
puts "##############################################################"
puts "***************Generating traffic: using UDP***************"
puts ""

for {set i 0} {$i < 20} {incr i 1} {
	set src_umts($i)  [new Agent/UDP]
	$ue($i) attach-agent $src_umts($i) $umts_ue($i)
	set sink_umts($i) [new Agent/Null]
	#$ue(0) attach-agent $sink(0) $umts_ue(0) 
    $ns attach-agent $umts_ue($i) $sink_umts($i)
    $src_umts($i) set fid_ $i
    $sink_umts($i) set fid_ $i

    set src_wlan($i)  [new Agent/UDP]
    $ue($i) attach-agent $src_wlan($i) $wlan_ue($i)
    set sink_wlan($i) [new Agent/Null]
    #$ue(0) attach-agent $sink(0) $umts_ue(0) 
    $ns attach-agent $wlan_ue($i) $sink_wlan($i)
    $src_wlan($i) set fid_ [expr $i+20]
    $sink_wlan($i) set fid_ [expr $i+20]
}

$ns connect $src_umts(0) $sink_wlan(1)
set e_app(0) [new Application/Traffic/CBR]
$e_app(0) attach-agent $src_umts(0)
$e_app(0) set packet_size_ 666
$e_app(0) set type_ CBR

$ns connect $src_wlan(1) $sink_umts(2)

set e_app(1) [new Application/Traffic/CBR]
$e_app(1) attach-agent $src_wlan(1)
$e_app(1) set packet_size_ 777
$e_app(1) set type_ CBR

$ns connect $src_umts(2) $sink_umts(0)
set e_app(2) [new Application/Traffic/CBR]
$e_app(2) attach-agent $src_umts(2)
$e_app(2) set packet_size_ 888
$e_app(2) set type_ CBR

$ns at 1.0 "$e_app(0) start"
$ns at 1.0 "$e_app(1) start"
$ns at 1.0 "$e_app(2) start"

puts "finished"
puts ""

# do some kind of registration in UMTS
puts "****************************************************************"
puts "do some kind of registration in UMTS......"
$ns node-config -llType UMTS/RLC/AM \
		-downlinkBW 384kbs \
		-uplinkBW 384kbs \
		-downlinkTTI 20ms \
		-uplinkTTI 20ms \
   		-hs_downlinkTTI 2ms \
    		-hs_downlinkBW 384kbs

# for the first HS-DSCH, we must create. If any other, then use attach-hsdsch

$ns create-hsdsch $umts_ue(0) $src_umts(0)
$ns attach-hsdsch $umts_ue(1) $src_umts(1)
$ns attach-hsdsch $umts_ue(1) $sink_umts(1)
$ns attach-hsdsch $umts_ue(2) $sink_umts(2)

#puts "create hsdsch umts_ue(1) sink(1)"

#for {set i 2} {$i < 20} {incr i 2} {
#	$ns attach-hsdsch $umts_ue($i) $src($i)
 # puts "attach hsdsch to umts_ue($i) src($i)"
	#$ns attach-hsdsch $umts_ue([expr $i+1]) $sink([expr $i+1])
  #puts "attach hsdsch umts_ue([expr $i+1]) sink([expr $i+1])"
#}
# we must set the trace for the environment. If not, then bandwidth is reduced and
# packets are not sent the same way (it looks like they are queued, but TBC)
puts "set trace for the environment......"

$BS setErrorTrace 0 "idealtrace"

# load the CQI (Channel Quality Indication)
puts "loading Channel Quality Indication......"

$BS loadSnrBlerMatrix "SNRBLERMatrix"

$BS trace-outlink $f 2

#for {set i 0} {$i < 20} {incr i 1} {
#  $umts_ue($i) trace-inlink $f $i
#  $umts_ue($i) trace-outlink $f $i
#  $wlan_ue($i) trace-outlink $f [expr $i + 20]
#  $wlan_ue($i) trace-inlink $f [expr $i + 20]
#}


puts "finished"
puts "****************************************************************"
puts "################################################################"

$ns at 2.3 "getMeanDelay"
$ns at 4.0 "getMeanDelay"
$ns at 3.2 "getMeanDelay"
$ns at 3.6 "getMeanDelay"
$ns at 10.0 "getMeanDelay"
$ns at 0.1 "record $wlan_ue(1)"

# call to redirect traffic
$ns at 2 "redirectTraffic $ue(0) $wlan_ue(0) $src_wlan(0) $sink_wlan(1)"
$ns at 2 "$ns trace-annotate \"Redirecting traffic\""

$ns at 15 "finish"

puts " Simulation is running ... please wait ..."
$ns run
