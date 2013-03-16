# Copyright (c) 2003 Ericsson Telecommunicatie B.V.
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the
#     distribution.
# 3. Neither the name of Ericsson Telecommunicatie B.V. may be used
#     to endorse or promote products derived from this software without
#     specific prior written permission.
# 
# 
# THIS SOFTWARE IS PROVIDED BY ERICSSON TELECOMMUNICATIE B.V. AND
# CONTRIBUTORS "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL ERICSSON TELECOMMUNICATIE B.V., THE AUTHOR OR HIS
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# 
# Contact for feedback on EURANE: eurane@ti-wmc.nl
# EURANE = Enhanced UMTS Radio Access Network Extensions
# website: http://www.ti-wmc.nl/eurane/
#
# $Id: ns-umts.tcl,v 1.1.1.1 2006/03/08 13:53:00 rouil Exp $
#
#======================================================================
#
# Enhanced UMTS:  Extensions for NS-2 (support for HS-DSCH, MAC-hs)
#
# ======================================================================
#
# Configuration related functions
#
# ======================================================================


Simulator instproc UmtsNodeType	{val} { $self set UmtsNodeType_  $val }
Simulator instproc downlinkBW	{val} { $self set downlinkBW_  $val }
Simulator instproc downlinkTTI	{val} { $self set downlinkTTI_  $val }
Simulator instproc uplinkBW	{val} { $self set uplinkBW_  $val }
Simulator instproc uplinkTTI	{val} { $self set uplinkTTI_  $val }
Simulator instproc baseStation	{val} { $self set baseStation_  $val }
Simulator instproc radioNetworkController {val} { $self set radioNetworkController_  $val }

#HSDPA additions################################################################
Simulator instproc hs_downlinkBW	{val} { $self set hs_downlinkBW_  $val }
Simulator instproc hs_downlinkTTI	{val} { $self set hs_downlinkTTI_  $val }
Simulator instproc hsdschEnabled	{val} { $self set hsdschEnabled_  $val }
Simulator instproc hsdsch_rlc_set	{val} { $self set hsdsch_rlc_set_  $val }
Simulator instproc hs_am {val} { $self set hs_am_  $val }
Simulator instproc hs_mac {val} { $self set hs_mac_  $val }
Simulator instproc hs_chan	{val} { $self set hs_chan_  $val }
################################################################################


# ======================================================================
#
# IP PDU - RLC PDU Demultiplexer at UmtsNode entry
#
# ======================================================================

RtModule/Demuxer instproc register { node } {
    $self attach-node $node
    
    $self instvar classifier_
    set classifier_ [new Demuxer]

    $node insert-entry-connector $self $classifier_ up-target
}


# ======================================================================
#
# The Node/UmtsNode class 
#
# ======================================================================

Class Node/UmtsNode -superclass Node

Node/UmtsNode instproc init args {
    eval $self next $args		;# parent class constructor

    $self instvar nifs_ type_
    $self instvar bs_ rnc_ Iub_ port_classifier_ nif_classifier_
    $self instvar chan_tx_ chan_rx_ phy_tx_ phy_rx_ mac_ drophead_ cs_ tcm_ connector_ nif_ ll_ 

    set nifs_	0		;# number of network interfaces
}



#
# - Place the existing classifier entry at slot $hook of the new connector. 
#

Node/UmtsNode instproc insert-entry-connector { module clsfr hook } {

    $self instvar classifier_ mod_assoc_ hook_assoc_

    set hook_assoc_($clsfr) $classifier_

    if { $hook == "up-target" } {
	$clsfr up-target $classifier_
    } elseif { $hook == "down-target" } {
	$clsfr down-target $classifier_
    } else {
	puts "Error: wrong value of hook; exiting"
	exit 1
    }

    # Associate this module to the classifier, so if the classifier is
    # removed later, we'll remove the module as well.
    set mod_assoc_($clsfr) $module
    set classifier_ $clsfr
}


#
# Attach an agent to a node.  Pick a port and
# bind the agent to the port number.
# if portnumber is 255, default target is set to the routing agent
#

Node/UmtsNode instproc add-target {agent port } {

    $self instvar dmux_ address_
    
    if { $dmux_ == "" } {
	# Use the default mask_ and port_ values
	set dmux_ [new Classifier/Port]
	# point the node's routing entry to itself
	# at the port demuxer (if there is one)
	$self add-route $address_ $dmux_
    }
    
    if { $port == [Node set rtagent_port_] } {
	# Set the default target at C++ level.  
	set tmp [$self set classifier_]
	[$tmp up-target] defaulttarget $agent
	$dmux_ install $port $agent
    } else {
	# Send Target
	$agent target [$self entry]

	# Recv Target
	$dmux_ install $port $agent
    }
}


# ======================================================================
#
# methods for creating UmtsNodes
#
# ======================================================================

Simulator instproc create-Umtsnode {{address ""}} {

    $self instvar UmtsNodeType_
    $self instvar radioNetworkController_ baseStation_
    $self instvar downlinkBW_ uplinkBW_ downlinkTTI_ uplinkTTI_
    #HSDPA addition######################################################
    $self instvar hsdschEnabled_ hs_downlinkBW_ hs_downlinkTTI_
    $self instvar hsdsch_rlc_set_ hsdsch_rlc_nif_
    #####################################################################
    
    if {![info exists UmtsNodeType_]} {
	puts "Error: create-Umtsnode called, but no UmtsNodeType_; exiting"
	exit 1
    }
    
    # Case conversion (for compatibility)
    if {$UmtsNodeType_ == "BS"} {set UmtsNodeType_ "bs"}
    if {$UmtsNodeType_ == "UE"} {set UmtsNodeType_ "ue"}
    if {$UmtsNodeType_ == "RNC"} {set UmtsNodeType_ "rnc"}
    if {[lsearch {bs ue rnc} $UmtsNodeType_] < 0} {
	puts "Error: undefined UmtsNodeType: $UmtsNodeType_; exiting"
	exit 1
    }
    
    if {$UmtsNodeType_ == "ue" } {
	if {![info exists baseStation_] || ![info exists radioNetworkController_]} {
	    puts "Error: Failed to set BS and RNC for UE; exiting"
	    exit 1
	}
    }
    if {$UmtsNodeType_ == "ue" || $UmtsNodeType_ == "bs"} {
	if {![info exists downlinkBW_] || ![info exists uplinkBW_]} {
	    puts "Error: Failed to set uplink and downlink RLC data rate. exiting"
	    exit 1
	}
	if {![info exists downlinkTTI_] || ![info exists uplinkTTI_]} {
	    puts "Error: Failed to set uplink and downlink TTI. exiting"
	    exit 1
	}
    }
    
    # Create the Umtsnode
    set tmp [$self newUmtsnode $address]

    return $tmp
}


Simulator instproc newUmtsnode {address} {
    $self instvar UmtsNodeType_ baseStation_ radioNetworkController_ Node_
    $self instvar downlinkBW_ uplinkBW_ downlinkTTI_ uplinkTTI_ Node_ link_
    #HSDPA addition######################################################
    $self instvar hsdschEnabled_ hs_downlinkBW_ hs_downlinkTTI_
    $self instvar hsdsch_rlc_set_ hsdsch_rlc_nif_
    #####################################################################
    if ![info exists UmtsNodeType_] {
	puts "Error: UmtsNodeType_ does not exist in newUmtsnode; exiting"
	exit 1
    }
    if {$address==""} {
	set node [new Node/UmtsNode]
    } else {
	set node [new Node/UmtsNode $address]
    }

    $node set type_ $UmtsNodeType_
    $node register-module [new RtModule/Demuxer]
    if {$UmtsNodeType_ == "ue"} {
	$node instvar bs_ rnc_ classifier_ port_classifier_ nif_classifier_ mac_ id_
	set bs_ $baseStation_
	set rnc_ $radioNetworkController_
	set port_classifier_ [new Classifier/SrcPort]
	set nif_classifier_ [new Classifier/Nif]
	$node add-target $port_classifier_ 255
	$classifier_ down-target $nif_classifier_
	$node setup-common rach $uplinkBW_ $uplinkTTI_ [$bs_ set chan_rx_(0)]
	$node setup-common fach [$bs_ set chan_tx_(1)]
   
   # set MAC source address (for RLC) to FACH NIF identifier
   $mac_(1) SA $id_
	
	#################################################################
	$rnc_ add-dest-classifier [$node set address_]

	# Set up- and downlinks from UE and BS??
	set link_([$node id]:[$bs_ id]) [new Link $node $bs_]
	$link_([$node id]:[$bs_ id]) set queue_ [new Queue/DropTail]
	set link_([$bs_ id]:[$node id]) [new Link $bs_ $node]
	$link_([$bs_ id]:[$node id]) set queue_ [new Queue/DropTail]
	#NIST: added debug
	#puts "created ue"
   }
    
    if {$UmtsNodeType_ == "bs"} {
	$node instvar classifier_ nif_classifier_ mac_ id_
	set nif_classifier_ [new Classifier/Nif]
	$classifier_ down-target $nif_classifier_
	$node setup-common rach
	$node setup-common fach $downlinkBW_ $downlinkTTI_
	
	$mac_(0) SA $id_
	#NIST: added debug
	#puts "created bs umts"
    }

    if {$UmtsNodeType_ == "rnc"} {
	$node instvar classifier_ nif_classifier_
	set nif_classifier_ [new Classifier/Nif]
	$classifier_ down-target $nif_classifier_
	#NIST: add the rnc in order to ne reachable by hierarchical routing
	$self add-node $node [$node id]
	#NIST: added debug
	#puts "Created rnc node"
    }
    set Node_([$node id]) $node
    $node set ns_ $self

    $self check-node-num
    return $node
}


Node/UmtsNode instproc add-dest-classifier { address } {

    $self instvar type_ classifier_ port_classifier_
    if {$type_ == "rnc"} {
	set port_classifier_($address) [new Classifier/Port]
	[$classifier_ up-target] install $address $port_classifier_($address)
    } 
}

Node/UmtsNode instproc add-gateway { node } {
    $self instvar type_ classifier_ 
    [Simulator instance] instvar link_
    
    if {$type_ == "rnc"} {
	set link $link_([$self id]:[$node id])
	[$classifier_ up-target] defaulttarget [$link head]
    } else {
	puts "Error."
	puts "Correct Syntax is : $RNC add-gateway $Gateway"
    }
    
}

# This is quite dirty: It passes the method from the node to the second
# MAC, which should be the Mac-hs.

Node/UmtsNode instproc setErrorTrace { flow filename } {
   $self instvar mac_

   #TODO: add checks whether it is really a BS with HSDPA enabled.
   $mac_(2) setErrorTrace $flow $filename
}

Node/UmtsNode instproc loadSnrBlerMatrix { filename } {
   $self instvar mac_

   #TODO: add checks whether it is really a BS with HSDPA enabled.
   $mac_(2) loadSnrBlerMatrix $filename
}

# ======================================================================
#
# methods for creating Umts channels, and error models
#
# ======================================================================

Simulator instproc setup-Iub { bs rnc ubw dbw udelay ddelay type args } {

    $self instvar link_ traceAllFile_ traceAllFileBackup_

    set i1 [$bs id]
    set i2 [$rnc id]

    if [info exists traceAllFile_] {
	set traceAllFileBackup_ $traceAllFile_
	set traceAllFile_ ""
    }

    eval $self simplex-link $bs $rnc $ubw $udelay $type
    eval $self simplex-link $rnc $bs $dbw $ddelay $type

    if [info exists traceAllFileBackup_] {
	set traceAllFile_ $traceAllFileBackup_
	set traceAllFileBackup_ ""
	$self trace-rlc-queue $bs $rnc $traceAllFile_
	$self trace-rlc-queue $rnc $bs $traceAllFile_
    }

    $bs set Iub_ $link_($i1:$i2)
    $rnc set Iub_ $link_($i2:$i1)
    
    $bs update-Iub-path
    $rnc update-Iub-path
}


Node/UmtsNode instproc update-Iub-path {} {

    $self instvar type_ nifs_ nif_ mac_ Iub_

    if {$type_ == "bs"} {
	set head [$Iub_ set head_]

	for {set i 0} {$i < $nifs_} {incr i} {
	    if {$i != 1 && [info exists mac_($i)]} {
		$mac_($i) up-target $head 
	    }
	}
    } elseif {$type_ == "rnc"} {

	set head [$Iub_ set head_]

	for {set i 0} {$i < $nifs_} {incr i} {
	    if {[info exists nif_($i)]} {
		$nif_($i) target $head
	    }
	}
	
    } 
}


Simulator instproc create-dch { node l_agent } {

    $self instvar llType_ downlinkBW_ uplinkBW_ downlinkTTI_ uplinkTTI_

    set inchan [new Channel]
    set outchan [new Channel]
    set cs [new ChannelSwitcher]
    
    set bs [$node set bs_]
    set rnc [$node set rnc_]

    set n_tti [[$node set mac_(0)] TTI]
    set n_bw [[$node set mac_(0)] BW]
    
    set b_tti [[$bs set mac_(1)] TTI]
    set b_bw [[$bs set mac_(1)] BW]
    
    $cs rlc_DS_info $n_tti $n_bw $b_tti $b_bw

    set nifs1 [eval $rnc add-interface dch $llType_ $downlinkTTI_ $downlinkBW_]
    set ll [$rnc set ll_($nifs1)]
    set nifs2 [eval $bs add-interface dch $llType_ $downlinkTTI_ $downlinkBW_ $inchan $outchan $cs $ll]
    set nifs3 [eval $node add-interface dch $llType_ $uplinkTTI_ $uplinkBW_ $outchan $inchan $cs]
    
    set n_tti [[$node set mac_($nifs3)] TTI]
    set n_bw [[$node set mac_($nifs3)] BW]
    
    set b_tti [[$bs set mac_($nifs2)] TTI]
    set b_bw [[$bs set mac_($nifs2)] BW]

    $cs rlc_US_info $n_tti $n_bw $b_tti $b_bw

    [$node set nif_($nifs3)] label $nifs1
    [$rnc set nif_($nifs1)] label $nifs1
    
    [$node set ll_($nifs3)] addr [$node node-addr]
    [$node set ll_($nifs3)] daddr [$rnc node-addr]
    
    [$rnc set ll_($nifs1)] addr [$rnc node-addr]
    [$rnc set ll_($nifs1)] daddr [$node node-addr]

    [$node set ll_($nifs3)] macDA [$bs node-addr]
    [$rnc set ll_($nifs1)] macDA [$node node-addr]

[$node set mac_($nifs3)] SA [$node node-addr]
[$bs set mac_($nifs2)] SA [$bs node-addr]

$rnc set-nif-classifier $nifs1 $nifs1
$bs set-nif-classifier $nifs1 $nifs2
$node set-nif-classifier $nifs1 $nifs3

$node set-port-path [$l_agent port] $nifs3
$rnc set-port-path [$l_agent port] $nifs1 [$node set address_]

$bs update-Iub-path
$rnc update-Iub-path

return $nifs3
}


Simulator instproc attach-dch { node l_agent nifs } {

    $node instvar chan_tx_ chan_rx_ id_

    if { ![info exists chan_tx_($nifs)] || ![info exists chan_rx_($nifs)] } {
	puts "Invalid DCH stack given for Node ID:$id_"
	exit 1

    } else {
	set bs [$node set bs_]
	set rnc [$node set rnc_]

	$node set-port-path [$l_agent port] $nifs
	set nifs1 [[$node set nif_($nifs)] label]
	$rnc set-port-path [$l_agent port] $nifs1 [$node set address_]
    }
}

Simulator instproc create-hsdsch { node l_agent} {

   # TODO: check whether this method has been called before (ie: check whether
   # hs_am, hs_mac and hs_caddrhan are initialized)
   $self instvar llType_ hsdschEnabled_ hsdsch_rlc_set_ hsdsch_rlc_nif_
#   $self instvar hs_downlinkTTI_ hs_downlinkBW_ uplinkTTI_ uplinkBW_
   $self instvar hs_downlinkTTI_ hs_downlinkBW_
   $self instvar hs_am hs_mac hs_chan
   set hs_chan [new Channel]

   set bs [$node set bs_]
   set rnc [$node set rnc_]
   if {$llType_ == "UMTS/RLC/AM"} {
      set hs_am [eval $rnc add-interface hsdsch UMTS/RLC/AMHS $hs_downlinkTTI_ $hs_downlinkBW_]
   } else {
      set hs_am [eval $rnc add-interface hsdsch UMTS/RLC/UMHS $hs_downlinkTTI_ $hs_downlinkBW_]
   }
   set hs_mac [eval $bs add-interface hsdsch $llType_ $hs_downlinkTTI_ $hs_downlinkBW_ $hs_chan]
   set tmp [$self attach-hsdsch $node $l_agent]
   return tmp
}

Simulator instproc attach-hsdsch { node l_agent} {
   # TODO: check whether create-hsdsch has been called before this method

   $self instvar llType_ hsdschEnabled_ hsdsch_rlc_set_ hsdsch_rlc_nif_
   $self instvar hs_downlinkTTI_ hs_downlinkBW_ uplinkTTI_ uplinkBW_
   $self instvar hs_am hs_mac hs_chan

   set bs [$node set bs_]
   set rnc [$node set rnc_]

   set hs_ue [eval $node add-interface hsdsch $llType_ $uplinkTTI_ $uplinkBW_ $hs_chan]

   [$node set nif_($hs_ue)] label $hs_am
   [$node set nif_([expr $hs_ue + 1])] label $hs_am
   [$rnc set nif_($hs_am)] label $hs_am

   [$node set ll_($hs_ue)] addr [$node node-addr]
   [$node set ll_($hs_ue)] daddr [$rnc node-addr]

   [$rnc set ll_($hs_am)] addr [$rnc node-addr]
   [$rnc set ll_($hs_am)] daddr [$node node-addr]
 
   [$node set ll_($hs_ue)] macDA [$bs node-addr]]
   [$rnc set ll_($hs_am)] macDA [$node node-addr]

   [$node set mac_($hs_ue)] SA [$node id]
   [$node set mac_([expr $hs_ue + 1])] SA [$node node-addr]
   [$bs set mac_($hs_mac)] SA [$bs id]
   [$bs set mac_([expr [$node id] + 1])] SA [$bs node-addr]

   $rnc set-nif-classifier $hs_am $hs_am
   $bs set-nif-classifier $hs_am $hs_mac
   $node set-nif-classifier $hs_am $hs_ue


   $node set-port-path [$l_agent port] $hs_ue
   $rnc set-port-path [$l_agent port] $hs_am [$node set address_]

#   $node set-port-path 0 $hs_ue
#   $rnc set-port-path 0 $hs_am [$node set address_]

   $bs update-Iub-path
   $rnc update-Iub-path

   # now create the uplink



return $hs_ue



}

Simulator instproc attach-common { node l_agent} {

    $self instvar llType_ hsdschEnabled_

    set bs [$node set bs_]
    set rnc [$node set rnc_]

    set n_tti [[$node set mac_(0)] TTI]
    set n_bw [[$node set mac_(0)] BW]

    set b_tti [[$bs set mac_(1)] TTI]
    set b_bw [[$bs set mac_(1)] BW] 
    

    set nifs1 [eval $rnc add-interface common $llType_ $b_tti $b_bw]
    set nifs3 [eval $node add-interface common $llType_ $n_tti $n_bw]

    [$node set nif_($nifs3)] label $nifs1
    [$rnc set nif_($nifs1)] label $nifs1

    [$node set ll_($nifs3)] addr [$node node-addr]
    [$node set ll_($nifs3)] daddr [$rnc node-addr]
    
    [$rnc set ll_($nifs1)] addr [$rnc node-addr]
    [$rnc set ll_($nifs1)] daddr [$node node-addr]
    
    [$node set ll_($nifs3)] macDA [$bs node-addr]
    [$rnc set ll_($nifs1)] macDA [$node node-addr]
    
    $rnc set-nif-classifier $nifs1 $nifs1
    
    
    $bs set-nif-classifier $nifs1 1
  
    #########################################
    
    $node set-nif-classifier $nifs1 $nifs3

    $node set-port-path [$l_agent port] $nifs3
    $rnc set-port-path [$l_agent port] $nifs1 [$node set address_]

    $bs update-Iub-path
    $rnc update-Iub-path
}



Node/UmtsNode instproc set-nif-classifier {nif_value stack} {

    $self instvar nif_classifier_ tcm_ connector_ ll_ type_

   if {$type_ == "ue" || $type_ == "rnc"} {
      $nif_classifier_ install $nif_value $ll_($stack)
   } elseif {$type_ == "bs"} {
      if {[info exists connector_($stack)]} {
         $nif_classifier_ install $nif_value $connector_($stack)
      } else {
         $nif_classifier_ install $nif_value $tcm_($stack)
      }
   }
}


Node/UmtsNode instproc set-port-path {port nif {address 0}} {

    $self instvar port_classifier_ ll_ type_

    if {$type_ == "ue" } {
         $port_classifier_ install $port $ll_($nif)
    } elseif {$type_ == "rnc"} {
         $port_classifier_($address) install $port $ll_($nif)
    }
}



#============================================================================
#
#  The following sets up link layer, mac layer and physical layer structures
#  etc for the Umts nodes.
#
#============================================================================

Node/UmtsNode instproc add-interface args {
   $self instvar bs_ nifs_ chan_rx_ chan_tx_ phy_rx_ phy_tx_ mac_
   $self instvar drophead_ cs_ tcm_ connector_ nif_ ll_ type_

   set t $nifs_
   incr nifs_
   set linktype [lindex $args 0]

   if {$type_ == "bs"} {
      if {$linktype == "dch"} {

         set tcm_($t)      [new TrChMeasurer]
         set drophead_($t) [new Connector]   ;# drop target for mac queue
         set mac_($t)      [new Mac/Umts]    ;# mac layer
         set mac_tti       [lindex $args 2]
         set mac_bw        [lindex $args 3]
         set phy_tx_($t)   [new Phy/Umts]    ;# interface
         set phy_rx_($t)   [new Phy/Umts]    ;# interface
         set chan_rx_($t)  [lindex $args 4]
         set chan_tx_($t)  [lindex $args 5]
         set cs_($t)       [lindex $args 6]
         set ll            [lindex $args 7]

	 #NIST: add link to node
	 $mac_($t) set-node $self

         #
         # Local Variables
         #

         set tcm $tcm_($t)
         set cs $cs_($t)
         set drophead $drophead_($t)
         set mac $mac_($t)
         set phy_rx $phy_rx_($t)
         set phy_tx $phy_tx_($t)
         set inchan $chan_rx_($t)
         set outchan $chan_tx_($t)

         #
         # Stack Head
         #


         $tcm up-target $mac
         $tcm down-target $mac_(1)
         $tcm CS $cs
         $tcm BS 1
         $tcm RLC-AM $ll

         $cs downlink-AM $ll
         $cs downlink-tcm $tcm


         #
         # Mac Layer
         #
         $drophead target [[Simulator instance] set nullAgent_]
         $mac drop-target $drophead

         $mac down-target $phy_tx
         $mac set bandwidth_ $mac_bw;
         $mac set TTI_ $mac_tti;
         $mac start_TTI

         #
         # Physical Layer
         #

         $phy_tx node $self         ;# Bind node <---> interface
         $phy_tx channel $outchan
         $phy_rx node $self         ;# Bind node <---> interface
         $phy_rx up-target $mac
         $inchan addif $phy_rx      ;# Add to list of Phys receiving on the channel

      } elseif {$linktype == "hsdsch" } {
         # nodetype bs + hsdsch
         if {$t>2} {
   #ifdef TCL_DEBUG
            puts -nonewline "add-interface: t>2: t == "
            puts -nonewline $t
            puts " (t == nifs_ -1)"
   #endif
            set connector_($t)   [new Connector]
            set nif_($t)         [new UmtsNetworkInterface]
            set drophead_($t)    [new Connector]      ;# drop target for queue
            set mac_($t)         [new Mac/Umts]       ;# mac layer
            set mac_tti          [lindex $args 2]     ;# interface queue
            set mac_bw           [lindex $args 3]
            set phy_rx_($t)      [new Phy/Umts]       ;# interface
            set chan_rx_($t)     [lindex $args 4]


            ##NIST: add link to node
	    $mac_($t) set-node $self

            # Local Variables
            #
            set connector $connector_($t)
            set nif $nif_($t)
            set drophead $drophead_($t)
            set mac $mac_($t)
            set phy_rx $phy_rx_($t)
            set inchan $chan_rx_($t)

            $drophead target [[Simulator instance] set nullAgent_]
            $mac drop-target $drophead

            #$mac up-target $nif
            $mac set bandwidth_ $mac_bw;
            $mac set TTI_ $mac_tti;
            $mac start_TTI

            $phy_rx node $self         ;# Bind node <---> interface
            $phy_rx up-target $mac
            $inchan addif $phy_rx      ;# Add to list of Phys receiving on the channel

         } else {
         # nodetype bs + hsdsch && t <= 2
            set connector_($t)   [new Connector]
            set drophead_($t)    [new Connector]      ;# drop target for mac queue
            set mac_($t)         [new Mac/Hsdpa]      ;# mac layer
            set mac_tti          [lindex $args 2]
            set mac_bw           [lindex $args 3]
            set phy_tx_($t)      [new Phy/Hsdpa]      ;# interface
            set chan_tx_($t)     [lindex $args 4]

	    #NIST: add link to node
	    $mac_($t) set-node $self

            #
            # Local Variables
            #

            set connector $connector_($t)
            #set tcm $tcm_($t)
            #set cs $cs_($t)
            set drophead $drophead_($t)
            set mac $mac_($t)
            set phy_tx $phy_tx_($t)
            set outchan $chan_tx_($t)

            #
            # Stack Head
            #

            #$tcm up-target $mac
            #$tcm down-target $mac_(1)
            #$tcm CS $cs
            #$tcm BS 1
            #$tcm RLC-AM $ll

            #$cs downlink-AM $ll
            #$cs downlink-tcm $tcm

            $connector target $mac

            #
            # Mac Layer
            #
            $drophead target [[Simulator instance] set nullAgent_]
            $mac drop-target $drophead

            $mac down-target $phy_tx
            $mac set bandwidth_ $mac_bw;
            $mac set TTI_ $mac_tti;
            $mac start_TTI
            $mac start_sched

            #
            # Physical Layer
            #

            $phy_tx node $self      ;# Bind node <---> interface
            $phy_tx channel $outchan
         }
      }

   } elseif {$type_ == "rnc" } {
      if {$linktype == "hsdsch"} {

            ## RNC LL setup for 'hsdsch' channel

            set ll_($t) [new [lindex $args 1]]     ;# link layer

            #
            # Local Variables
            #
            set ll $ll_($t)

            #
            # Link Layer
            #

            $ll set TTI_ [lindex $args 2];
            $ll set bandwidth_ [lindex $args 3];
            $ll start_TTI



            $ll up-target [$self entry]
            set nif_($t) [new UmtsNetworkInterface]
            $ll down-target $nif_($t)
      } else {
         # nodetype rnc + hsdsch

         ## RNC LL setup for normal 'common' channels

         set ll_($t) [new [lindex $args 1]]     ;# link layer

         #
         # Local Variables
         #
         set ll $ll_($t)

         #
         # Link Layer
         #

         $ll set TTI_ [lindex $args 2];
         $ll set bandwidth_ [lindex $args 3];
         $ll start_TTI



         $ll up-target  [$self entry]
         set nif_($t)   [new UmtsNetworkInterface]
         $ll down-target $nif_($t)
      }
   } elseif {$type_ == "ue"} {
      if {$linktype == "common"} {
         set ll_($t)    [new [lindex $args 1]]        ;# link layer
         set nif_($t)   [new UmtsNetworkInterface]

         #
         # Local Variables
         #
         set ll $ll_($t)
         set nif $nif_($t)

         #
         # Link Layer
         #

         $ll set TTI_ [lindex $args 2];
         $ll set bandwidth_ [lindex $args 3];
         $ll start_TTI


         $ll down-target $nif
         $ll up-target [$self entry]

         #
         # NetworkInterface
         #

         $nif target $connector_(0)

      } elseif {$linktype == "dch"} {
         # UE + dch
         set ll_($t)       [new [lindex $args 1]]     ;# link layer
         set nif_($t)      [new UmtsNetworkInterface]
         set tcm_($t)      [new TrChMeasurer]
         set mac_tti       [lindex $args 2]           ;# interface queue
         set drophead_($t) [new Connector]            ;# drop target for queue
         set mac_($t)      [new Mac/Umts]             ;# mac layer
         set mac_bw        [lindex $args 3]
         set phy_tx_($t)   [new Phy/Umts]             ;# interface
         set phy_rx_($t)   [new Phy/Umts]             ;# interface
         set chan_rx_($t)  [lindex $args 4]
         set chan_tx_($t)  [lindex $args 5]
         set cs_($t)       [lindex $args 6]

	 #NIST: add link to node
	 $mac_($t) set-node $self

         #
         # Local Variables
         #
         set ll $ll_($t)
         set nif $nif_($t)
         set tcm $tcm_($t)
         set cs $cs_($t)
         set drophead $drophead_($t)
         set mac $mac_($t)
         set phy_rx $phy_rx_($t)
         set phy_tx $phy_tx_($t)
         set inchan $chan_rx_($t)
         set outchan $chan_tx_($t)

         #
         # Link Layer
         #

         $ll set TTI_ $mac_tti;
         $ll set bandwidth_ $mac_bw;
         $ll start_TTI


         $ll down-target $nif
         $ll up-target [$self entry]

         #
         # NetworkInterface
         #

         $nif target $tcm
         #$nif target $connector
         #
         # Stack Head
         #

         $tcm up-target $mac
         $tcm down-target $mac_(0)
         $tcm CS $cs
         $tcm RLC-AM $ll

         $cs uplink-AM $ll
         $cs uplink-tcm $tcm


         #
         # Mac Layer
         #

         $drophead target [[Simulator instance] set nullAgent_]
         $mac drop-target $drophead

         $mac down-target  $phy_tx
         $mac up-target    $ll
         $mac set bandwidth_  $mac_bw;
         $mac set TTI_        $mac_tti;
         $mac start_TTI

         #
         # Physical Layer
         #

         $phy_tx node $self      ;# Bind node <---> interface
         $phy_rx node $self      ;# Bind node <---> interface
         $phy_tx channel $outchan
         $phy_rx up-target $mac
         $inchan addif $phy_rx   ;# Add to list of Phys receiving on the channel
      } elseif {$linktype == "hsdsch"} {
         # UE + hsdsch

         set connector_($t)   [new Connector]
         set ll_($t)          [new [lindex $args 1]]		;# link layer
         set nif_($t)         [new UmtsNetworkInterface]
         set mac_tti          [lindex $args 2]		;# interface queue
         set drophead_($t)    [new Connector]			;# drop target for queue
         set mac_($t)         [new Mac/Hsdpa]			;# mac layer
         set mac_bw           [lindex $args 3]
         set phy_rx_($t)      [new Phy/Hsdpa]			;# interface
         set chan_rx_($t)     [lindex $args 4]

	 #NIST: add link to node
	 $mac_($t) set-node $self

         #
         # Local Variables
         #
         set connector $connector_($t)
         set ll $ll_($t)
         set nif $nif_($t) ;# This nif is only created in order to have 2 elements
                           # in the array nif_
         set drophead $drophead_($t)
         set mac $mac_($t)
         set phy_rx $phy_rx_($t)
         set inchan $chan_rx_($t)

         incr nifs_
         incr t

         set nif_($t) [new UmtsNetworkInterface]
         set drophead_($t) [new Connector]
         set mac_($t) [new Mac/Umts]
         set mac_tti [lindex $args 2]
         set mac_bw [lindex $args 3]
         set phy_tx_($t) [new Phy/Umts]
         set chan_tx_($t) [new Channel]
         set connector_($t) [new Connector]

	 #NIST: add link to node
	 $mac_($t) set-node $self

         set connector2 $connector_($t)
         set nif2 $nif_($t)
         set drophead2 $drophead_($t)
         set mac2 $mac_($t)
         set phy_tx $phy_tx_($t)
         set outchan $chan_tx_($t)

         #
         # Link Layer
         #

         $ll set TTI_ $mac_tti;
         $ll set bandwidth_ $mac_bw;
         $ll start_TTI


         $ll down-target $nif2
         #$ll down-target $mac2
         $ll up-target [$self entry]

         #
         # NetworkInterface
         #

         $nif2 target $connector2
         $connector2 target $mac2

         #
         # Mac Layer
         #

         $drophead target [[Simulator instance] set nullAgent_]
         $mac drop-target $drophead
         $drophead2 target [[Simulator instance] set nullAgent_]
         $mac2 drop-target $drophead2

         $mac up-target $ll
         $mac set bandwidth_ $mac_bw;
         $mac set TTI_ $mac_tti;
         $mac start_TTI

         $mac2 down-target $phy_tx
         $mac2 set bandwidth_ $mac_bw;
         $mac2 set TTI_ $mac_tti;
         $mac2 start_TTI

         #
         # Physical Layer
         #
         $phy_rx node $self		;# Bind node <---> interface
         $phy_rx up-target $mac
         $inchan addif $phy_rx		;# Add to list of Phys receiving on the channel

         $phy_tx node $self		;# Bind node <---> interface
         $phy_tx channel $outchan

         $bs_ add-interface hsdsch UMTS/RLC/AM [lindex $args 2] [lindex $args 3] $outchan

         set t [expr $t - 1]

      }


   } else {
      #unknown node type

   }
   return $t
}



Node/UmtsNode instproc setup-common args {
    $self instvar nifs_ chan_rx_ chan_tx_ phy_rx_ phy_tx_ mac_ drophead_ connector_ nif_ ll_ type_

    set linktype [lindex $args 0]

    if {$type_ == "ue"} {
	set t $nifs_
	incr nifs_

	if {$linktype == "rach"} {
	    set connector_($t)	[new Connector]
	    set drophead_($t) 	[new Connector]			;# drop target for queue
	    set mac_($t)	[new Mac/Umts]			;# mac layer
	    set mac_bw		[lindex $args 1]
	    set mac_tti		[lindex $args 2]
	    set phy_tx_($t)	[new Phy/Umts]			;# interface
	    set chan_tx_($t)	[lindex $args 3]

	    #NIST: add link to node
	    $mac_($t) set-node $self

	    #
	    # Local Variables
	    #

	    set connector $connector_($t)
	    set drophead $drophead_($t)
	    set mac $mac_($t)
	    set phy_tx $phy_tx_($t)
	    set outchan $chan_tx_($t)

	    #
	    # Stack Head
	    #
	    
	    $connector target $mac

	    
	    #
	    # Mac Layer
	    #
	    
	    $drophead target [[Simulator instance] set nullAgent_]
	    $mac drop-target $drophead

	    $mac down-target $phy_tx
	    $mac set bandwidth_ $mac_bw; 
	    $mac set TTI_ $mac_tti; 
	    $mac start_TTI
	    $mac set_access_delay

	    #
	    # Physical Layer
	    #
	    
	    $phy_tx node $self		;# Bind node <---> interface
	    $phy_tx channel $outchan

	} elseif {$linktype == "fach"} {
	   
	    set mac_($t)	[new Mac/Umts]		;# mac layer
	    set phy_rx_($t)	[new Phy/Umts]		;# interface
	    set chan_rx_($t)	[lindex $args 1]
	    
	    #NIST: add link to node
	    $mac_($t) set-node $self

	    #
	    # Local Variables
	    #
       
	    set mac $mac_($t)
	    set phy_rx $phy_rx_($t)
	    set inchan $chan_rx_($t)		
	    
	    #
	    # Mac Layer
	    #
	    
	    $mac up-target [$self entry]			

	    #
	    # Physical Layer
	    #
	    
	    $phy_rx node $self	;# Bind node <---> interface
	    $phy_rx up-target $mac
	    $inchan addif $phy_rx	;# Add to list of Phys receiving on the channel

	}
	
    } elseif {$type_ == "bs"} {
	set t $nifs_
	incr nifs_
	
	if {$linktype == "rach"} {
	    
	    set mac_($t)	[new Mac/Umts]		;# mac layer
	    set phy_rx_($t)	[new Phy/Umts]		;# interface
	    set chan_rx_($t)	[new Channel]

	    #NIST: add link to node
	    $mac_($t) set-node $self

	    #
	    # Local Variables
	    #

	    set mac $mac_($t)
	    set phy_rx $phy_rx_($t)
	    set inchan $chan_rx_($t)


	    #
	    # Physical Layer
	    #

	    $phy_rx node $self		;# Bind node <---> interface
	    $phy_rx up-target $mac
	    $inchan addif $phy_rx	;# Add to list of Phys receiving on the channel
	    
	} elseif {$linktype == "fach"} {
	    
	    set connector_($t)	[new Connector]
	    set drophead_($t) 	[new Connector]			;# drop target for queue
	    set mac_($t)	[new Mac/Umts]			;# mac layer
	    set mac_bw		[lindex $args 1]
	    set mac_tti		[lindex $args 2]		;# interface queue
	    set phy_tx_($t)	[new Phy/Umts]			;# interface
	    set chan_tx_($t)	[new Channel]

	    #NIST: add link to node
	    $mac_($t) set-node $self

	    #
	    # Local Variables
	    #

	    set connector $connector_($t)
	    set drophead $drophead_($t)
	    set mac $mac_($t)
	    set phy_tx $phy_tx_($t)
	    set outchan $chan_tx_($t)

	    #
	    # Stack Head
	    #
	    
	    $connector target $mac


	    #
	    # Mac Layer
	    #
	    
	    $drophead target [[Simulator instance] set nullAgent_]
	    $mac drop-target $drophead

	    $mac down-target $phy_tx	
	    $mac set bandwidth_ $mac_bw; 
	    $mac set TTI_ $mac_tti; 
	    $mac start_TTI
	    
	    #
	    # Physical Layer
	    #

	    $phy_tx node $self			;# Bind node <---> interface
	    $phy_tx channel $outchan

	} 

    }
}




#===================================================================================================
#
#  The following are functions to set up tracing in the UMTS system.
#
#===================================================================================================


Simulator instproc trace-rlc-queue { n1 n2 {file ""} } {
    $self instvar link_ traceAllFile_
    if {$file == ""} {
	if ![info exists traceAllFile_] return
	set file $traceAllFile_
    }
    $link_([$n1 id]:[$n2 id]) trace-rlc $self $file

    # Added later for queue specific tracing events other than enque, 
    # deque and drop 
    set queue [$link_([$n1 id]:[$n2 id]) queue]
    $queue attach-traces $n1 $n2 $file
}


SimpleLink instproc trace-rlc { ns f {op ""} } {

    $self instvar enqT_ deqT_ drpT_ queue_ link_ fromNode_ toNode_
    $self instvar rcvT_ ttl_ trace_
    $self instvar drophead_		

    set trace_ $f
    set enqT_ [$ns create-trace UMTS/Enque $f $fromNode_ $toNode_ $op]
    set deqT_ [$ns create-trace UMTS/Deque $f $fromNode_ $toNode_ $op]
    set drpT_ [$ns create-trace UMTS/Drop $f $fromNode_ $toNode_ $op]
    set rcvT_ [$ns create-trace UMTS/Recv $f $fromNode_ $toNode_ $op]

    $self instvar drpT_ drophead_
    set nxt [$drophead_ target]
    $drophead_ target $drpT_
    $drpT_ target $nxt

    $queue_ drop-target $drophead_

    $drpT_ target [$queue_ drop-target]
    $queue_ drop-target $drpT_

    $deqT_ target [$queue_ target]
    $queue_ target $deqT_

    # head is, like the drop-head_ a special connector.
    # mess not with it.
    $self add-to-head $enqT_

    # put recv trace after ttl checking, so that only actually 
    # received packets are recorded
    $rcvT_ target [$ttl_ target]
    $ttl_ target $rcvT_

    $self instvar dynamics_
    if [info exists dynamics_] {
	$self trace-dynamics $ns $f $op
    }
}


Node/UmtsNode instproc trace-outlink {f {index_ 0}} {
    $self instvar id_ enqT_ deqT_ drpT_ hopT_ phy_tx_ mac_ nif_ nifs_ drophead_ tcm_ cs_ connector_ type_ nif_classifier_ bs_
    
    set ns [Simulator instance]
    set fromNode_ $id_


    if {$type_ == "ue" } {
	if {[info exists tcm_($index_)] || [info exists connector_($index_)]} {
	    set toNode_ 1
	    set enqT_($index_) [$ns create-trace UMTS/Enque $f $fromNode_ $toNode_]
	    $enqT_($index_) target $mac_($index_)
	    if {[info exists tcm_($index_)]} {
		$tcm_($index_) up-target $enqT_($index_)
	    } else {
		for {set i 0} {$i < $nifs_} {incr i} {

		    if {[info exists tcm_($i)]} {
			$tcm_($i) down-target $enqT_($index_)
		    }
		}

		$connector_($index_) target $enqT_($index_)
	    }

	    set deqT_($index_) [$ns create-trace UMTS/Deque $f $fromNode_ $toNode_]
	    $deqT_($index_) target $phy_tx_($index_)
	    $mac_($index_) down-target $deqT_($index_)
	    
	    set drpT_($index_) [$ns create-trace UMTS/Drop $f $fromNode_ $toNode_]
	    $drpT_($index_) target [$drophead_($index_) target]
	    $drophead_($index_) target $drpT_($index_)
	    $mac_($index_) drop-target $drpT_($index_)
	} else {
	    puts "UE: Stack $index_ at Node ID:$id_ is not outlink."
	}

    } elseif {$type_ == "rnc" } {
	if {[info exists nif_($index_)]} {
	    set toNode_ 1
	    set hopT_($index_) [$ns create-trace UMTS/Hop $f $fromNode_ $toNode_]
	    $hopT_($index_) target [$nif_($index_) target]
	    $nif_($index_) target $hopT_($index_)
	} else {
	    puts "RNC: Stack $index_ at RNC ID:$id_ is not outlink."
	}

    } elseif {$type_ == "bs" } {
	if {[info exists tcm_($index_)] || [info exists connector_($index_)]} {
	    set toNode_ -1
	    set enqT_($index_) [$ns create-trace UMTS/Enque $f $fromNode_ $toNode_]
	    $enqT_($index_) target $mac_($index_)
	    if {[info exists tcm_($index_)]} {
		$tcm_($index_) up-target $enqT_($index_)
	    } else {
		for {set i 0} {$i < $nifs_} {incr i} {
		    if {[info exists tcm_($i)]} {
			$tcm_($i) down-target $enqT_($index_)
		    }
		}
		
		$connector_($index_) target $enqT_($index_)
	    }

	    set deqT_($index_) [$ns create-trace UMTS/Deque $f $fromNode_ $toNode_]
	    $deqT_($index_) target $phy_tx_($index_)
	    $mac_($index_) down-target $deqT_($index_)
	    
	    set drpT_($index_) [$ns create-trace UMTS/Drop $f $fromNode_ $toNode_]
	    $drpT_($index_) target [$drophead_($index_) target]
	    $drophead_($index_) target $drpT_($index_)
	    $mac_($index_) drop-target $drpT_($index_)
	} else {
	    puts "BS: Stack $index_ at BS ID:$id_ is not outlink."
	}
    }
}


#
# Trace element between mac and ll 
#

Node/UmtsNode instproc trace-inlink {f {index_ 0} } {
    $self instvar id_ rcvT_ mac_ ll_ errT_ em_ type_ bs_

    set ns [Simulator instance]
    set toNode_ $id_
    
    if {$type_ == "rnc"} {
	return
    } elseif {$type_ == "ue"} {
	set fromNode_ $bs_
    } elseif {$type_ == "bs"} {
	set fromNode_ -1
    }

    if {[info exists em_($index_)]} {
	# if error model, then chain mac -> em -> rcvT -> ll
	# First, set up an error trace on the ErrorModule
	set errT_($index_) [$ns create-trace UMTS/Error $f $fromNode_ $toNode_]
	$errT_($index_) target [$em_($index_) drop-target]
	$em_($index_) drop-target $errT_($index_)
	set rcvT_($index_) [$ns create-trace UMTS/Recv $f $fromNode_ $toNode_]
	$rcvT_($index_) target [$em_($index_) target]
	$em_($index_) target $rcvT_($index_)
    } else {
	if {[info exists mac_($index_)]} {
	    set rcvT_($index_) [$ns create-trace UMTS/Recv $f $fromNode_ $toNode_]
	    $rcvT_($index_) target [$mac_($index_) up-target]
	    $mac_($index_) up-target $rcvT_($index_)
	} else {
	    puts"Stack $index_ at Node ID:$id_ is not valid."
	}
    }
}


Node/UmtsNode instproc trace-inlink-tcp {f {index_ 0} } {
    $self instvar id_ rcvtcpT_ ll_ type_ bs_

    set ns [Simulator instance]
    set toNode_ $id_

    if {$type_ == "bs"} {
	return
    } elseif {$type_ == "ue"} {
	set fromNode_ $bs_
    } elseif {$type_ == "rnc"} {
	set fromNode_ 1
    }
    if {[info exists ll_($index_)]} {
	set rcvtcpT_($index_) [$ns create-trace Recv $f $fromNode_ $toNode_]
	$rcvtcpT_($index_) target [$ll_($index_) up-target]
	$ll_($index_) up-target $rcvtcpT_($index_)
    } else {
	puts "Stack $index_ at Node ID:$id_ is not valid."
    }
}


#  Attaches error model to interface "index" (by default, the first one)
Node/UmtsNode instproc interface-errormodel { em { index 0 } } {
    $self instvar mac_ em_ id_
    if {[info exists mac_($index)]} {
	$em target [$mac_($index) up-target]
	$mac_($index) up-target $em
	$em drop-target [new Agent/Null]; # otherwise, packet is only marked
	set em_($index) $em
    } else {
	puts "Stack $index at Node ID:$id_ is not valid."
    }
}



Node/UmtsNode instproc status-errormodel { em { index 0 } } {
    $self instvar mac_ em_ id_
    if {[info exists mac_($index)] && [info exists em_($index)]} {
	set tano_ [new Tano]
	$mac_($index) up-target $tano_
	$tano_ up-target $em_($index)
	$tano_ down-target $em 		
	$em target [$em_($index) target]

	$em drop-target [new Agent/Null]; # otherwise, packet is only marked
    } else {
	puts "Stack $index at Node ID:$id_ is not valid."
    }
}


###########
# TRACE MODIFICATIONS
##########

# This creates special UMTS tracing elements
# Support for Enque, Deque, Recv, Drop, Hop and Error

Class Trace/UMTS/Hop -superclass Trace/UMTS
Trace/UMTS/Hop instproc init {} {
    $self next "h"
}

Class Trace/UMTS/Enque -superclass Trace/UMTS
Trace/UMTS/Enque instproc init {} {
    $self next "+"
}

Class Trace/UMTS/Deque -superclass Trace/UMTS
Trace/UMTS/Deque instproc init {} {
    $self next "-"
}

Class Trace/UMTS/Recv -superclass Trace/UMTS
Trace/UMTS/Recv instproc init {} {
    $self next "r"
}

Class Trace/UMTS/Drop -superclass Trace/UMTS
Trace/UMTS/Drop instproc init {} {
    $self next "d"
}

Class Trace/UMTS/Error -superclass Trace/UMTS
Trace/UMTS/Error instproc init {} {
    $self next "e"
}

# HTTP Modification
Http instproc umts-connect { server } {
    Http instvar TRANSPORT_
    $self instvar ns_ slist_ node_ fid_ id_

    lappend slist_ $server
    set tcp [new Agent/TCP/$TRANSPORT_]
    $tcp set fid_ [$self getfid]
    $ns_ attach-agent $node_ $tcp

    set ret [$server alloc-connection $self $fid_]
    set snk [$ret agent]
    $ns_ connect $tcp $snk
    #$tcp set dst_ [$snk set addr_]
    $tcp set window_ 100

    # Use a wrapper to implement application data transfer
    set wrapper [new Application/TcpApp $tcp]
    $self cmd connect $server $wrapper
    $wrapper connect $ret
    #puts "HttpApp $id_ connected to server [$server id]"

    return $tcp
}

