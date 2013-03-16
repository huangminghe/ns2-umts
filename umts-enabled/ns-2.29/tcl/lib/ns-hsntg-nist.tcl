# Contains classes created to support multi-interfaces and mobility.
#
# This software was developed at the National Institute of Standards and
# Technology by employees of the Federal Government in the course of
# their official duties. Pursuant to title 17 Section 105 of the United
# States Code this software is not subject to copyright protection and
# is in the public domain.
# NIST assumes no responsibility whatsoever for its use by other parties,
# and makes no guarantees, expressed or implied, about its quality,
# reliability, or any other characteristic.
# <BR>
# We would appreciate acknowledgement if the software is used.
# <BR>
# NIST ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION AND
# DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING
# FROM THE USE OF THIS SOFTWARE.
# </PRE></P>
# @author  rouil
# @version 1.0
# <P><PRE>
# VERSION CONTROL<BR>
# -------------------------------------------------------------------------<BR>
# Name  - YYYY/MM/DD - VERSION - ACTION<BR>
# rouil - 2005/04/10 - 1.0     - Source created<BR>
# <BR>
# <BR>
# </PRE><P>
#

# Define default values for information we added
Agent/ND set router_ 0
Agent/ND set useAdvInterval_ 0
Agent/ND set router_lifetime_ 5
Agent/ND set prefix_lifetime_ 5
Agent/ND set broadcast_  1
Agent/ND set maxRtrAdvInterval_ 600.0
Agent/ND set minRtrAdvInterval_ 200.0
Agent/ND set minDelayBetweenRA_ 3
Agent/ND set maxRADelay_ 0.5
Agent/ND set rs_timeout_ 1.0
Agent/ND set max_rtr_solicitation_ 3
Agent/ND set default_port_ 254

Agent/MIHUser set default_port_ 253

Agent/MIHUser/IFMNGMT/MIPV6 set flowRequestTimeout_ 0.02

Agent/MIHUser/IFMNGMT/MIPV6/Handover set detectionTimeout_ 1

Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1 set case_ 1 ;# no trigger
Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover1 set confidence_th_ 80 ;#80% confidence if going down are used (case 3)

Agent/MIH set default_port_ 252     
Agent/MIH set supported_events_        255  ;#0xFF: all events
Agent/MIH set supported_commands_      4095 ;#0xFFF: all commands
Agent/MIH set supported_transport_     33686018  ;#0x02020202: L3 only
Agent/MIH set supported_query_type_    1    ;#0x1: TLV
Agent/MIH set es_timeout_              0.5
Agent/MIH set cs_timeout_              0.5
Agent/MIH set is_timeout_              1
Agent/MIH set retryLimit_              3

Mac/802_11 set max_error_frame_ 5
Mac/802_11 set max_beacon_loss_ 3
Mac/802_11 set bw_timeout_ 2.0

Mac/802_11 set print_stats_ 0

Mac/802_11 set retx_avg_alpha_ 1.0
Mac/802_11 set rtt_avg_alpha_ 1.0
Mac/802_11 set rxp_avg_alpha_ 1.0
Mac/802_11 set numtx_avg_alpha_ 1.0
Mac/802_11 set numrx_avg_alpha_ 1.0
Mac/802_11 set qlen_avg_alpha_ 1.0
Mac/802_11 set pktloss_avg_alpha_ 1.0
Mac/802_11 set qwait_avg_alpha_ 1.0
Mac/802_11 set collsize_avg_alpha_ 1.0
Mac/802_11 set txerror_avg_alpha_ 1.0
Mac/802_11 set txloss_avg_alpha_ 1.0
Mac/802_11 set txframesize_avg_alpha_ 1.0
Mac/802_11 set occupiedbw_avg_alpha_ 1.0
Mac/802_11 set delay_avg_alpha_ 1.0
Mac/802_11 set jitter_avg_alpha_ 1.0
Mac/802_11 set rxtp_avg_alpha_ 1.0
Mac/802_11 set txtp_avg_alpha_ 1.0
Mac/802_11 set rxbw_avg_alpha_ 1.0
Mac/802_11 set txbw_avg_alpha_ 1.0
Mac/802_11 set rxsucc_avg_alpha_ 1.0
Mac/802_11 set txsucc_avg_alpha_ 1.0
Mac/802_11 set rxcoll_avg_alpha_ 1.0
Mac/802_11 set txcoll_avg_alpha_ 1.0

Mac/802_11 set beacon_interval_ 0.1
Mac/802_11 set minChannelTime_ 0.020
Mac/802_11 set maxChannelTime_ 0.060
Mac/802_11 set pr_limit_ 1.0
Mac/802_11 set client_lifetime_ 3.0
Mac/802_11 set RetxThreshold_ 5.99999

Phy/WirelessPhy set noise_percentage_ 0

Queue/DropTailHSNTG set drop_front_ false
Queue/DropTailHSNTG set summarystats_ false
Queue/DropTailHSNTG set queue_in_bytes_ false
Queue/DropTailHSNTG set mean_pktsize_ 500

Queue/DropTailHSNTG/PriQueueHSNTG set Prefer_Routing_Protocols    1

#
# Define some global methods for Simulator and node
#

# add node config for multi-interface support
Simulator instproc multiIf {val} { $self set multiIf_ $val }

# lookup for a node that has the given mac address
Simulator instproc get-node-by-mac { mac } {
    $self instvar Node_ 
    set n [Node set nn_]
    #puts "Looking for mac address $mac"
    for {set q 0} {$q < $n} {incr q} {
	if { [info exists Node_($q)] } {
	    #puts "  Checking node $q addr [$Node_($q) node-addr]"
	    set nq $Node_($q)
	    set res [$nq get-mac-by-addr $mac]
	    if { $res != ""} {
		return $nq
	    }
	}
    }
    return ""
}

# lookup for a node that has the given mac address
Simulator instproc get-node-id-by-mac { mac } {
    $self instvar Node_ 
    set n [Node set nn_]
    #puts "Looking for mac address $mac"
    for {set q 0} {$q < $n} {incr q} {
	if { [info exists Node_($q)] } {
	    #puts "  Checking node $q addr [$Node_($q) node-addr]"
	    set nq $Node_($q)
	    set res [$nq get-mac-by-addr $mac]
	    if { $res != ""} {
		return [$nq id]
	    }
	}
    }
    return "-1"
}

Node instproc get-mac-by-addr { addr } {
    $self instvar mac_ nifs_

    if ([info exists mac_]&&[info exist nifs_]) {
	for {set r 0} {$r < $nifs_} {incr r} {
	    #puts "   Checking interface $r with addr [$mac_($r) id] for addr $addr"
	    if {[eval $mac_($r) id]== $addr} {
		#puts "found node"
		return $mac_($r)
	    }
	}	
    }
    #puts "Mac not in node"
    return ""
}

LanNode instproc get-mac-by-addr { addr } {
    return ""
}

# Install and return the neighbor discovery module in the given node
Node instproc install-nd {} {
    $self instvar nd_ 
    set nd [new Agent/ND]
    $self attach $nd [$nd set default_port_] ;#default port for nd agent
    set nd_ $nd ;#register the nd module to be accesible from the node
    return $nd
}

# Return the neighbor discovery module in the given node
Node instproc get-nd {} {
    $self instvar nd_ 
    if ([info exists nd_]) {
	return $nd_
    }
    return ""
}

# Install and return the neighbor discovery module in the given node
Node instproc install-ifmanager { ag } {
    $self attach $ag [$ag set default_port_]
}

# Install a default interface manager (used for CN to redirect traffic
# Install and return the neighbor discovery module in the given node
Node instproc install-default-ifmanager { } {
    set ag [new Agent/MIHUser/IFMNGMT/MIPV6]
    $self attach $ag [$ag set default_port_]
    return $ag
}

Agent/MIHUser/IFMNGMT instproc attach-nd { nd } {
    puts "Deprecated: use \$nd set-ifmanager \$ifmanager instead"
    #$nd set-ifmanager $self
}

# Install and return the MIH module in the given node
Node instproc install-mih {} {
    set ag [new Agent/MIH]
    $self attach $ag [$ag set default_port_]
    #set tmp2 [$self getMac 0]
    #$tmp2 mih $ag

    return $ag
}

Node instproc register-interface { mac } {
    $self instvar nifs_ mac_
    
    if [info exists nifs_] {
	set t $nifs_
	incr nifs_
    } else {
	set t 0
	set nifs_ 1
    }
    #puts "Registering new mac $mac into node [$self node-addr]"
    set mac_($t) $mac
}

Mac instproc set-node { node } {
    $self instvar node_

    set node_ $node
}

Mac instproc get-node {} {
    $self instvar node_

    return $node_
}


########################## END GLOBAL METHODS ##########################

#
# Class Multi-interface node.
#

Class MultiFaceNode -superclass Node

# Initialize the Muti-interface node
MultiFaceNode instproc init args {
    eval $self next $args 
    $self instvar address_ ifaceArray_ ifaceNb_
    set ifaceNb_ 0                               ;#there is not interface at first
    #puts "MultiFaceNode $address_ created."
}

# Add the given interface (node) and register its port demux
MultiFaceNode instproc add-interface-node { ifaceNode } {
    #puts "Adding interface node [$ifaceNode node-addr]."
    $self instvar dmux_ address_

    #
    # If a port demuxer doesn't exist, create it.
    #
    if { $dmux_ == "" } {
       	set dmux2_ [new Classifier/Port]
	#puts "Creating dmux_ $dmux2_ in node $address_"
	$self install-demux $dmux2_
    }

    # install the SAME dmux into the interface.
    #$ifaceNode install-demux $dmux_
    # set the default target of the interface node to this demux
    if {[$ifaceNode set dmux_] == ""} {
	set tmp [new Classifier/Port]
	$ifaceNode install-demux $tmp
    }
    [$ifaceNode set dmux_] defaulttarget $dmux_

    # add interface to the list
    $self registerIf $ifaceNode

    #puts "Interface node added."
}

# Connect the remote agent to the given agent using the given interface
MultiFaceNode instproc connect-agent { home_agent remote_agent iface } {
    # get NS instance
    set nstmp [Simulator instance]
    # connect home to remote agent
    $nstmp simplex-connect $home_agent $remote_agent
    # retrieve interface address
    set destAddr [AddrParams addr2id [$iface node-addr]]
    # the remote agent must send packets to the interface address
    $remote_agent set dst_addr_ $destAddr
    # the remote agent must send packets to the home agent's port
    $remote_agent set dst_port_ [$home_agent set agent_port_]
}

# Connect the given agent to this node using the given interface
MultiFaceNode instproc attach-agent { home_agent iface {port "" } } {
    # call super method
    $self attach $home_agent $port
    # redirect the agent's target to the interface
    $home_agent target [$iface entry]
}

#
# Manipulation of interfaces
#

# look for the interface with the given ID
MultiFaceNode instproc getInterfaceByID { ifaceID } {
    $self instvar ifaceArray_ ifaceNb_

    for {set i 0} { $i < $ifaceNb_ } { incr i } {
	if { [eval $ifaceArray_($i) getId] == $ifaceID } {
	    return ifaceArray_($i)
	}
    }
    return ""
}

# register the interface 
MultiFaceNode instproc registerIf { iface } {
    $self instvar ifaceArray_ ifaceNb_

    # check if the interface is already stored
    set tmpNode [$self getInterfaceByID [$iface id]]
    if { $tmpNode ==""} {
	set ifaceArray_($ifaceNb_) [new InterfaceInfo $iface]
	incr ifaceNb_
    }
}

# unreigster the interface
MultiFaceNode instproc unregisterIf { iface } {
    $self instvar ifaceArray_ ifaceNb_

    # check if the interface is already stored
    for {set i 0} { $i < $ifaceNb_ } { incr i } {
	if { [eval $ifaceArray_($i) getId] == [eval $iface id] } {
	    # we found it, slide the other interfaces in the table
	    for { set j [expr i+1] } { $j < $ifaceNb_ } { incr j } {
		set ifaceArray_([expr $j-1]) $ifaceArray_($j)
	    }
	}
	# there is one interface less
	set ifaceNb_ [expr $ifaceNb_ -1]
    }
}

# return the best interface index according to the priority
MultiFaceNode instproc getBestInterface {} {
    $self instvar ifaceArray_ ifaceNb_
    
    set result -1
    if { $ifaceNb_ > 0 } {
	set maxPriority [$ifaceArray_(0) getPriority] 
    }
    # look through all interfaces
    for {set i 1} { $i < $ifaceNb_ } { incr i } {
	if { [$ifaceArray_($i) getPriority] > $maxPriority } {
	    set result $i
	}
    }
    return $result
}

# Debug method to dump the addresses of the interfaces
MultiFaceNode instproc dump-interfaces {} {
    $self instvar ifaceArray_ ifaceNb_ address_

    puts "Node $address_ contains $ifaceNb_ interfaces:"
    for { set i 0 } { $i < $ifaceNb_ } { incr i } {
	puts "iface-addr($i)=[$ifaceArray_($i) node-addr]"
    }
}

# Creates a trace to move node and its interfaces
# This node is virutal, so it's position is the one of its interfaces
MultiFaceNode instproc setdest args {
    $self instvar ifaceArray_ ifaceNb_ address_

    set dstX [lindex $args 0]
    set dstY [lindex $args 1]
    set speed [lindex $args 2]
    set dstZ 0

    #1-Get position of interface and update
    $self set X_ [[$ifaceArray_(0) getInterface] set X_]
    $self set Y_ [[$ifaceArray_(0) getInterface] set Y_]
    $self set Z_ [[$ifaceArray_(0) getInterface] set Z_]

    #2-create entry for the interface node
    [Simulator instance] puts-ns-traceall [format \
				    "M %.5f %s (%.2f, %.2f, %.2f), (%.2f, %.2f), %.2f" \
					       [[Simulator instance] now] [AddrParams addr2id [$self node-addr]] [$self set X_] [$self set Y_] [$self set Z_] \
				    $dstX $dstY $speed ]
    #4-move each interface
    for { set i 0 } { $i < $ifaceNb_ } { incr i } {
	[$ifaceArray_($i) getInterface] setdest $dstX $dstY $speed
    }
}


########################## END CLASS MultiFaceNode ##########################

#
# Class to store interface information
# Current data stored: interface, priority
#

Class InterfaceInfo

# Create the interface information datastructure
InterfaceInfo instproc init args {
    $self instvar id_ iface_ priority_

    set iface_ [lindex $args 0]
    set id_ [eval $iface_ id]
    set priority_ 0 ;#default priority
}

# return the interface associated with the datastructure
InterfaceInfo instproc getInterface {} {
    return [$self set iface_]
}

# return the priority of the interface
InterfaceInfo instproc getPriority {} {
    return [$self set priority_]
}

# return the id of the interface
InterfaceInfo instproc getId {} {
    return [$self set id_]
}

# return the priority of the interface
InterfaceInfo instproc setPriority args {
    $self instvar priority_
    set priority_ [lindex $args 0]
}

# return a string version of the structure
InterfaceInfo instproc toString {} {
    $self instvar id_ iface_ priority_
    puts "Iface_ID: $id_, Addr: [$iface_ node-addr], priority: $priority_"
}

########################## END CLASS InterfaceInfo ##########################


