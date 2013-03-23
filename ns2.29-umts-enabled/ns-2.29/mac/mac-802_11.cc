/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*-
 *
 * Copyright (c) 1997 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Header: /var/lib/cvs/ns-2.29/mac/mac-802_11.cc,v 1.18 2007/02/26 16:25:51 rouil Exp $
 *
 * Ported from CMU/Monarch's code, nov'98 -Padma.
 * Contributions by:
 *   - Mike Holland
 *   - Sushmita
 * -----------------------------------------------------------------------
 * 03/10/06 - rouil - ported NIST modification to ns-2.29
 * 03/14/06 - rouil - updated recvBeacon method to discard a beacon if the
 *                    node is a base station. Also changed condition to 
 *                    generate trigger only when not connected.
 */

#include "delay.h"
#include "connector.h"
#include "packet.h"
#include "random.h"
#include "mobilenode.h"

//NIST:add include
#include "wireless-phy.h"
//end NIST

// #define DEBUG 99

#include "arp.h"
#include "ll.h"
#include "mac.h"
#include "mac-timers.h"
#include "mac-802_11.h"
#include "cmu-trace.h"

// Added by Sushmita to support event tracing
#include "agent.h"
#include "basetrace.h"

//NIST: enable the following line for lots of debug info!!!
//#define DEBUG_FULL

#ifdef DEBUG_FULL
#define debug2 printf 
#else
#define debug2(arg1,...) 
#endif

//#define RETX_TRIGGER
//end NIST

/*
 * NIST: Print the status of the collision size average
 */
double Mac802_11::calculate_txprob ()
{
	double pi = txerror_watch_.average();
	double prob = 0;
	double lambda;

	if(first_datatx_ && ((NOW-txerror_watch_.current()) <= 1)) {
		if(qwait_watch_.average() < phymib_.getSlotTime())
			lambda = 1;
		else
			lambda = phymib_.getSlotTime()/qwait_watch_.average();
	
		double denom = 2*(1-2*pi)*(1-pi);
		if(denom != 0) {
			double num = (1-2*pi)*(phymib_.getCWMin()+1) + pi*phymib_.getCWMin()*(1-pow(2*pi,(int)(macmib_.getShortRetryLimit()-1)));
			double lambdaFactor = (1-pi)*(1/lambda-1);
			prob = (1/(num/denom+lambdaFactor));
		}
	}
	return prob;
}

void Mac802_11::ms_retx_handover()
{
	debug ("At %.9f Mac %d, retx %f thresh %f weighted-thresh %f\n", NOW, index_, retx_watch_.average(), RetxThreshold_, (pr_limit_*RetxThreshold_));
	if ( retx_watch_.average() >= RetxThreshold_ ) {
		debug ("At %.9f Mac %d retx average is less then retx limit, so disconnect\n", NOW, index_);
		link_disconnect ();
	}
	else if ( retx_watch_.average() >= (pr_limit_*RetxThreshold_)) {
		double probability = 1;
		Mac::send_link_going_down (addr(), bss_id_, -1, (int)(100*probability), LGD_RC_LINK_PARAM_DEGRADING, eventId_++);
		going_down_ = true;
		if(first_lgd_ == true) {
			debug ("At %.9f Mac %d first-lgd rxp %.9f %e\n", NOW, index_, rxp_watch_.average(), pow(10, rxp_watch_.average()/10)/1e3);
			first_lgd_ = false;
		}
	}
	else {
		if (going_down_) {
			Mac::send_link_rollback (addr(), bss_id_, eventId_-1);
			going_down_ = false;
		}
	}
}

void Mac802_11::ms_rxp_handover()
{
	double avg_w = pow(10,(rxp_watch_.average()/10))/1e3;
	
	debug ("At %.9f Mac %d, rxp %e thresh %e weighted-thresh %e\n", NOW, index_, avg_w, RXThreshold_, pr_limit_*RXThreshold_);
	if ( avg_w < RXThreshold_) {
		debug ("At %.9f Mac %d rxp average is less then RXThreshold, so disconnect\n", NOW, index_);
		link_disconnect ();
	}
	else if ( avg_w < (pr_limit_*RXThreshold_)) {
		double probability = ((pr_limit_*RXThreshold_)-avg_w)/((pr_limit_*RXThreshold_)-RXThreshold_);
		Mac::send_link_going_down (addr(), bss_id_, -1, (int)(100*probability), LGD_RC_LINK_PARAM_DEGRADING, eventId_++);
		going_down_ = true;
		if(first_lgd_ == true) {
			debug ("At %.9f Mac %d first-lgd rxp %.9f %e\n", NOW, index_, rxp_watch_.average(), avg_w);
			first_lgd_ = false;
		}
	}
	else {
		if (going_down_) {
			Mac::send_link_rollback (addr(), bss_id_, eventId_-1);
			going_down_ = false;
		}
	}
}

void Mac802_11::queue_packet_loss()
{
	update_watch(&txloss_watch_, 1.0);
	//txloss_watch_.update(1);
	//debug ("At %.9f Mac %d txloss %f\n", NOW, index_, txloss_watch_.average());
}

void Mac802_11::update_queue_stats(int length)
{
	qlen_watch_.update(length + ((pktTx_==0)?0:1));
	debug ("At %.9f Mac %d qlen %f\n", NOW, index_, qlen_watch_.average());
}

/* our backoff timer doesn't count down in idle times during a
 * frame-exchange sequence as the mac tx state isn't idle; genreally
 * these idle times are less than DIFS and won't contribute to
 * counting down the backoff period, but this could be a real
 * problem if the frame exchange ends up in a timeout! in that case,
 * i.e. if a timeout happens we've not been counting down for the
 * duration of the timeout, and in fact begin counting down only
 * DIFS after the timeout!! we lose the timeout interval - which
 * will is not the REAL case! also, the backoff timer could be NULL
 * and we could have a pending transmission which we could have
 * sent! one could argue this is an implementation artifact which
 * doesn't violate the spec.. and the timeout interval is expected
 * to be less than DIFS .. which means its not a lot of time we
 * lose.. anyway if everyone hears everyone the only reason a ack will
 * be delayed will be due to a collision => the medium won't really be
 * idle for a DIFS for this to really matter!!
 */

inline void
Mac802_11::checkBackoffTimer()
{
	if(is_idle() && mhBackoff_.paused())
		mhBackoff_.resume(phymib_.getDIFS());
	if(! is_idle() && mhBackoff_.busy() && ! mhBackoff_.paused())
		mhBackoff_.pause();
}

inline void
Mac802_11::transmit(Packet *p, double timeout)
{
	tx_active_ = 1;
	
	if (EOTtarget_) {
		assert (eotPacket_ == NULL);
		eotPacket_ = p->copy();
	}

	/*
	 * If I'm transmitting without doing CS, such as when
	 * sending an ACK, any incoming packet will be "missed"
	 * and hence, must be discarded.
	 */
	if(rx_state_ != MAC_IDLE) {
		//struct hdr_mac802_11 *dh = HDR_MAC802_11(p);		
		//assert(dh->dh_fc.fc_type == MAC_Type_Control);
		//assert(dh->dh_fc.fc_subtype == MAC_Subtype_ACK);
		assert(pktRx_);
		struct hdr_cmn *ch = HDR_CMN(pktRx_);
		ch->error() = 1;        /* force packet discard */
	}

	/*
	 * pass the packet on the "interface" which will in turn
	 * place the packet on the channel.
	 *
	 * NOTE: a handler is passed along so that the Network
	 *       Interface can distinguish between incoming and
	 *       outgoing packets.
	 */
	//NIST: no of tx frames stats
	numtx_watch_.update(NOW-numtx_watch_.current());
	numtx_watch_.set_current(NOW);

	downtarget_->recv(p->copy(), this);	
	mhSend_.start(timeout);
	mhIF_.start(txtime(p));
	/*NIST: add help pointer*/
	last_tx_ = p;
}
inline void
Mac802_11::setRxState(MacState newState)
{
	rx_state_ = newState;
	checkBackoffTimer();
}

inline void
Mac802_11::setTxState(MacState newState)
{
	tx_state_ = newState;
	checkBackoffTimer();
}


/* ======================================================================
   TCL Hooks for the simulator
   ====================================================================== */
static class Mac802_11Class : public TclClass {
public:
	Mac802_11Class() : TclClass("Mac/802_11") {}
	TclObject* create(int, const char*const*) {
	return (new Mac802_11());

	}
} class_mac802_11;


/* ======================================================================
   Mac  and Phy MIB Class Functions
   ====================================================================== */

PHY_MIB::PHY_MIB(Mac802_11 *parent)
{
	/*
	 * Bind the phy mib objects.  Note that these will be bound
	 * to Mac/802_11 variables
	 */

	parent->bind("CWMin_", &CWMin);
	parent->bind("CWMax_", &CWMax);
	parent->bind("SlotTime_", &SlotTime);
	parent->bind("SIFS_", &SIFSTime);
	parent->bind("PreambleLength_", &PreambleLength);
	parent->bind("PLCPHeaderLength_", &PLCPHeaderLength);
	parent->bind_bw("PLCPDataRate_", &PLCPDataRate);
}

MAC_MIB::MAC_MIB(Mac802_11 *parent)
{
	/*
	 * Bind the phy mib objects.  Note that these will be bound
	 * to Mac/802_11 variables
	 */
	
	parent->bind("RTSThreshold_", &RTSThreshold);
	parent->bind("ShortRetryLimit_", &ShortRetryLimit);
	parent->bind("LongRetryLimit_", &LongRetryLimit);
}

/* ======================================================================
   Mac Class Functions
   ====================================================================== */
Mac802_11::Mac802_11() : 
	Mac(), phymib_(this), macmib_(this)/*NIST*/,
        print_stats_(0), bssTimer_(this), bwTimer_(this), beaconTimer_(this),
        probeTimer_(this) /*end NIST*/, mhIF_(this), mhNav_(this), 
	mhRecv_(this), mhSend_(this), 
	mhDefer_(this), mhBackoff_(this)
{
	
	nav_ = 0.0;
	tx_state_ = rx_state_ = MAC_IDLE;
	tx_active_ = 0;
	eotPacket_ = NULL;
	pktRTS_ = 0;
	pktCTRL_ = 0;		
	cw_ = phymib_.getCWMin();
	ssrc_ = slrc_ = 0;
	// Added by Sushmita
        et_ = new EventTrace();
	
	sta_seqno_ = 1;
	cache_ = 0;
	cache_node_count_ = 0;
	
	// chk if basic/data rates are set
	// otherwise use bandwidth_ as default;
	
	Tcl& tcl = Tcl::instance();
	tcl.evalf("Mac/802_11 set basicRate_");
	if (strcmp(tcl.result(), "0") != 0) 
		bind_bw("basicRate_", &basicRate_);
	else
		basicRate_ = bandwidth_;

	tcl.evalf("Mac/802_11 set dataRate_");
	if (strcmp(tcl.result(), "0") != 0) 
		bind_bw("dataRate_", &dataRate_);
	else
		dataRate_ = bandwidth_;

        EOTtarget_ = 0;
       	bss_id_ = IBSS_ID;
	//Set default channel
	calFreq(1);

	//printf("bssid in constructor %d\n",bss_id_);

	//NIST: add binding and initialization
	bind_bool ("print_stats_", &print_stats_);
	double tmp;
	bind ("retx_avg_alpha_", &tmp);
	retx_watch_.set_alpha(tmp);
	bind ("rtt_avg_alpha_", &tmp);
	rtt_watch_.set_alpha(tmp);
	bind ("rxp_avg_alpha_", &tmp);
	rxp_watch_.set_alpha(tmp);
	bind ("numtx_avg_alpha_", &tmp);
	numtx_watch_.set_alpha(tmp);
	bind ("numrx_avg_alpha_", &tmp);
	numrx_watch_.set_alpha(tmp);
	bind ("qlen_avg_alpha_", &tmp);
	qlen_watch_.set_alpha(tmp);
	bind ("pktloss_avg_alpha_", &tmp);
	pktloss_watch_.set_alpha(tmp);
	bind ("qwait_avg_alpha_", &tmp);
	qwait_watch_.set_alpha(tmp);
	bind ("collsize_avg_alpha_", &tmp);
	collsize_watch_.set_alpha(tmp);
	bind ("txerror_avg_alpha_", &tmp);
	txerror_watch_.set_alpha(tmp);
	bind ("txloss_avg_alpha_", &tmp);
	txloss_watch_.set_alpha(tmp);
	bind ("txframesize_avg_alpha_", &tmp);
	txframesize_watch_.set_alpha(tmp);
	bind ("occupiedbw_avg_alpha_", &tmp);
	occupiedbw_watch_.set_alpha(tmp);
	bind ("delay_avg_alpha_", &tmp);
	delay_watch_.set_alpha(tmp);
	bind ("jitter_avg_alpha_", &tmp);
	jitter_watch_.set_alpha(tmp);	
	bind ("rxtp_avg_alpha_", &tmp);
	rxtp_watch_.set_alpha(tmp);
	rxtp_watch_.set_pos_gradient (false);
	bind ("txtp_avg_alpha_", &tmp);
	txtp_watch_.set_alpha(tmp);	
	txtp_watch_.set_pos_gradient (false);
	bind ("rxbw_avg_alpha_", &tmp);
	rxbw_watch_.set_alpha(tmp);
	rxbw_watch_.set_pos_gradient (false);
	bind ("txbw_avg_alpha_", &tmp);
	txbw_watch_.set_alpha(tmp);	
	txbw_watch_.set_pos_gradient (false);
	bind ("rxsucc_avg_alpha_", &tmp);
	rxsucc_watch_.set_alpha(tmp);	
	bind ("txsucc_avg_alpha_", &tmp);
	txsucc_watch_.set_alpha(tmp);
	bind ("rxcoll_avg_alpha_", &tmp);
	rxcoll_watch_.set_alpha(tmp);	
	bind ("txcoll_avg_alpha_", &tmp);
	txcoll_watch_.set_alpha(tmp);
	
	bind ("bw_timeout_", &bw_timeout_);
	bind ("pr_limit_", &pr_limit_);
	bind ("max_error_frame_", &max_error_);
	bind ("beacon_interval_", &beacon_interval_);
	bind ("max_beacon_loss_", &max_beacon_loss_);
	bind ("minChannelTime_", &minChannelTime_);
	bind ("maxChannelTime_", &maxChannelTime_);	
	bind ("client_lifetime_", &client_lifetime_);
	bind ("RetxThreshold_", &RetxThreshold_);

	LIST_INIT(&list_client_head_);
	LIST_INIT(&collector_bw_);

	going_down_ = false;
	tcl.evalf ("Phy/WirelessPhy set RXThresh_");
	RXThreshold_ = atof (tcl.result());
	hdv_state_ = MAC_DISCONNECTED;
	pktManagement_ = 0;
	pktRetransmit_ = 0;
	managementQueue_ = new PacketQueue();
	pktBeacon_=0;
	nbAccepted_ = 0;
	nbRefused_ = 0;
	prev_pr = 0;
	tmpPacket_ = 0;
	nb_error_ = 0;
	bs_beacon_interval_ = 0;
	handover_ = 0;
	bss_timeout_ = 0.5;

	scan_status_.step = SCAN_NONE;
	scan_status_.minTimer = new MinScanTimer (this);
	scan_status_.maxTimer = new MaxScanTimer (this);
	
	overhead_duration_ = phymib_.getSIFS()+phymib_.getDIFS()+txtime(phymib_.getACKlen(), basicRate_);
	coll_overhead_duration_ = phymib_.getDIFS();

	first_datatx_ = false;
	first_lgd_ = true;
	prevDelay = 0;

	linkType_ = LINK_802_11;
	eventList_ = 0x1BF;
	commandList_ = 0xF;
	//end NIST
}


int
Mac802_11::command(int argc, const char*const* argv)
{
	//Tcl& tcl = Tcl::instance();
	//NIST: add enable beacon

	if (argc == 2) {
		if (strcmp(argv[1], "enable-beacon") == 0) {
			if(beaconTimer_.status() == TIMER_IDLE){
				//pick a backoff to avoid collision			       
				beaconTimer_.resched (Random::uniform(0, beacon_interval_)+beacon_interval_);
			}
			return TCL_OK;
		}
		if (strcmp(argv[1], "disable-beacon") == 0) {
			if (beaconTimer_.status() == TIMER_PENDING) {
				beaconTimer_.cancel();
			}
			return TCL_OK;
		}
		if (strcmp(argv[1], "autoscan") == 0) {
			start_autoscan ();
			return TCL_OK;
		}
	}

	if (argc == 3) {
		if (strcmp(argv[1], "eot-target") == 0) {
			EOTtarget_ = (NsObject*) TclObject::lookup(argv[2]);
			if (EOTtarget_ == 0)
				return TCL_ERROR;
			return TCL_OK;
		} else if (strcmp(argv[1], "bss_id") == 0) {
			
			//NIST: add link up/down trigger
			if (bss_id_ != (int)IBSS_ID) {
				link_disconnect();
			}		       
			old_bss_id_ = bss_id_;
			//if we set it via script then we don't want dynamic connection
			hdv_state_ = MAC_CONNECTED;
			bss_id_ = atoi(argv[2]);	
			// end NIST
			return TCL_OK;
		} else if (strcmp(argv[1], "log-target") == 0) { 
			logtarget_ = (NsObject*) TclObject::lookup(argv[2]);
			if(logtarget_ == 0)
				return TCL_ERROR;
			return TCL_OK;
		} else if(strcmp(argv[1], "nodes") == 0) {
			if(cache_) return TCL_ERROR;
			cache_node_count_ = atoi(argv[2]);
			cache_ = new Host[cache_node_count_ + 1];
			assert(cache_);
			bzero(cache_, sizeof(Host) * (cache_node_count_+1 ));
			return TCL_OK;
		} else if(strcmp(argv[1], "eventtrace") == 0) {
			// command added to support event tracing by Sushmita
                        et_ = (EventTrace *)TclObject::lookup(argv[2]);
                        return (TCL_OK);
		} else if (strcmp(argv[1], "set-channel") == 0) { 
			Tcl& tcl = Tcl::instance();
			channel_ = atoi(argv[2]);
			tcl.evalf ("%s set-freq %f", netif_->name(), calFreq(channel_));
			printf("%s channelID = %f \n", netif_->name(), calFreq(channel_));
			return TCL_OK;
		}
	}
	return Mac::command(argc, argv);
}
	
//NIST: add 802.11 channel frequency.
double 
Mac802_11::calFreq(int channelId)
{
	const int nbChannel=11;
	assert (channelId > 0 && channelId <= 11);
	double channel_freq_[nbChannel];
	channel_ = channelId;

	for(int i=0; i < nbChannel; i++){
		channel_freq_[i]= 2.412e9 + 0.005e9*i ;
	}
	return channel_freq_[channelId-1];
}	
// end NIST

// Added by Sushmita to support event tracing
void Mac802_11::trace_event(char *eventtype, Packet *p) 
{
        if (et_ == NULL) return;
        char *wrk = et_->buffer();
        char *nwrk = et_->nbuffer();
	
        //hdr_ip *iph = hdr_ip::access(p);
        //char *src_nodeaddr =
	//       Address::instance().print_nodeaddr(iph->saddr());
        //char *dst_nodeaddr =
        //      Address::instance().print_nodeaddr(iph->daddr());
	
        struct hdr_mac802_11* dh = HDR_MAC802_11(p);
	
        //struct hdr_cmn *ch = HDR_CMN(p);
	
	if(wrk != 0) {
		sprintf(wrk, "E -t "TIME_FORMAT" %s %2x ",
			et_->round(Scheduler::instance().clock()),
                        eventtype,
                        //ETHER_ADDR(dh->dh_sa)
                        ETHER_ADDR(dh->dh_ta)
                        );
        }
        if(nwrk != 0) {
                sprintf(nwrk, "E -t "TIME_FORMAT" %s %2x ",
                        et_->round(Scheduler::instance().clock()),
                        eventtype,
                        //ETHER_ADDR(dh->dh_sa)
                        ETHER_ADDR(dh->dh_ta)
                        );
        }
        et_->dump();
}

/* ======================================================================
   Debugging Routines
   ====================================================================== */
void
Mac802_11::trace_pkt(Packet *p) 
{
	struct hdr_cmn *ch = HDR_CMN(p);
	struct hdr_mac802_11* dh = HDR_MAC802_11(p);
	u_int16_t *t = (u_int16_t*) &dh->dh_fc;

	fprintf(stderr, "\t[ %2x %2x %2x %2x ] %x %s %d\n",
		*t, dh->dh_duration,
		 ETHER_ADDR(dh->dh_ra), ETHER_ADDR(dh->dh_ta),
		index_, packet_info.name(ch->ptype()), ch->size());
}

void
Mac802_11::dump(char *fname)
{
	fprintf(stderr,
		"\n%s --- (INDEX: %d, time: %2.9f)\n",
		fname, index_, Scheduler::instance().clock());

	fprintf(stderr,
		"\ttx_state_: %x, rx_state_: %x, nav: %2.9f, idle: %d\n",
		tx_state_, rx_state_, nav_, is_idle());

	fprintf(stderr,
		"\tpktTx_: %lx, pktRx_: %lx, pktRTS_: %lx, pktCTRL_: %lx, callback: %lx\n",
		(long) pktTx_, (long) pktRx_, (long) pktRTS_,
		(long) pktCTRL_, (long) callback_);

	fprintf(stderr,
		"\tDefer: %d, Backoff: %d (%d), Recv: %d, Timer: %d Nav: %d\n",
		mhDefer_.busy(), mhBackoff_.busy(), mhBackoff_.paused(),
		mhRecv_.busy(), mhSend_.busy(), mhNav_.busy());
	fprintf(stderr,
		"\tBackoff Expire: %f\n",
		mhBackoff_.expire());
}


/* ======================================================================
   Packet Headers Routines
   ====================================================================== */
inline int
Mac802_11::hdr_dst(char* hdr, int dst )
{
	struct hdr_mac802_11 *dh = (struct hdr_mac802_11*) hdr;
	
       if (dst > -2) {
               if ((bss_id() == (int)IBSS_ID) || (addr() == bss_id())) {
                       /* if I'm AP (2nd condition above!), the dh_3a
                        * is already set by the MAC whilst fwding; if
                        * locally originated pkt, it might make sense
                        * to set the dh_3a to myself here! don't know
                        * how to distinguish between the two here - and
                        * the info is not critical to the dst station
                        * anyway!
                        */
                       STORE4BYTE(&dst, (dh->dh_ra));
               } else {
                       /* in BSS mode, the AP forwards everything;
                        * therefore, the real dest goes in the 3rd
                        * address, and the AP address goes in the
                        * destination address
                        */
                       STORE4BYTE(&bss_id_, (dh->dh_ra));
                       STORE4BYTE(&dst, (dh->dh_3a));
               }
       }


       return (u_int32_t)ETHER_ADDR(dh->dh_ra);
}

inline int 
Mac802_11::hdr_src(char* hdr, int src )
{
	struct hdr_mac802_11 *dh = (struct hdr_mac802_11*) hdr;
        if(src > -2)
               STORE4BYTE(&src, (dh->dh_ta));
        return ETHER_ADDR(dh->dh_ta);
}

inline int 
Mac802_11::hdr_type(char* hdr, u_int16_t type)
{
	struct hdr_mac802_11 *dh = (struct hdr_mac802_11*) hdr;
	if(type)
		STORE2BYTE(&type,(dh->dh_body));
	return GET2BYTE(dh->dh_body);
}


/* ======================================================================
   Misc Routines
   ====================================================================== */
inline int
Mac802_11::is_idle()
{
	
	if(rx_state_ != MAC_IDLE) 
		return 0;
	
	if(tx_state_ != MAC_IDLE) 
		return 0;
	
	if(nav_ > Scheduler::instance().clock()) 
		return 0;
	return 1;
}

void
Mac802_11::discard(Packet *p, const char* why)
{
	hdr_mac802_11* mh = HDR_MAC802_11(p);
	hdr_cmn *ch = HDR_CMN(p);

	/* if the rcvd pkt contains errors, a real MAC layer couldn't
	   necessarily read any data from it, so we just toss it now */
	if(ch->error() != 0) {
		Packet::free(p);
		return;
	}

	switch(mh->dh_fc.fc_type) {
	case MAC_Type_Management:
		drop(p, why);
		return;
	case MAC_Type_Control:
		switch(mh->dh_fc.fc_subtype) {
		case MAC_Subtype_RTS:
			 if((u_int32_t)ETHER_ADDR(mh->dh_ta) ==  (u_int32_t)index_) {
				drop(p, why);
				return;
			}
			/* fall through - if necessary */
		case MAC_Subtype_CTS:
		case MAC_Subtype_ACK:
			if((u_int32_t)ETHER_ADDR(mh->dh_ra) == (u_int32_t)index_) {
				drop(p, why);
				return;
			}
			break;
		default:
			fprintf(stderr, "invalid MAC Control subtype\n");
			exit(1);
		}
		break;
	case MAC_Type_Data:
		switch(mh->dh_fc.fc_subtype) {
		case MAC_Subtype_Data:
			if((u_int32_t)ETHER_ADDR(mh->dh_ra) == \
                           (u_int32_t)index_ ||
                          (u_int32_t)ETHER_ADDR(mh->dh_ta) == \
                           (u_int32_t)index_ ||
                          (u_int32_t)ETHER_ADDR(mh->dh_ra) == MAC_BROADCAST) {
                                drop(p,why);
                                return;
			}
			break;
		default:
			fprintf(stderr, "invalid MAC Data subtype\n");
			exit(1);
		}
		break;
	default:
		fprintf(stderr, "invalid MAC type (%x)\n", mh->dh_fc.fc_type);
		trace_pkt(p);
		exit(1);
	}
	Packet::free(p);
}

void
Mac802_11::capture(Packet *p)
{
	/*
	 * Update the NAV so that this does not screw
	 * up carrier sense.
	 */	
	set_nav(usec(phymib_.getEIFS() + txtime(p)));
	Packet::free(p);
}

void
Mac802_11::collision(Packet *p)
{
	switch(rx_state_) {
	case MAC_RECV:
		setRxState(MAC_COLL);
		/* fall through */
	case MAC_COLL:
		assert(pktRx_);
		assert(mhRecv_.busy());
		/*
		 *  Since a collision has occurred, figure out
		 *  which packet that caused the collision will
		 *  "last" the longest.  Make this packet,
		 *  pktRx_ and reset the Recv Timer if necessary.
		 */
		if(txtime(p) > mhRecv_.expire()) {
			mhRecv_.stop();
			discard(pktRx_, DROP_MAC_COLLISION);
			pktRx_ = p;
			mhRecv_.start(txtime(pktRx_));
		}
		else {
			discard(p, DROP_MAC_COLLISION);
		}
		break;
	default:
		assert(0);
	}
}

void
Mac802_11::tx_resume()
{
	//double rTime;

	assert(mhSend_.busy() == 0);
	assert(mhDefer_.busy() == 0);

	debug2 ("At %.9f Mac %d tx_resume (backoff=%d)\n", NOW, index_, mhBackoff_.busy());

	if(pktCTRL_) {
		/*
		 *  Need to send a CTS or ACK.
		 */
		mhDefer_.start(phymib_.getSIFS());
		debug2 ("\tdefer SIFS for CTRL=%f\n", phymib_.getSIFS());
		//NIST: add packet management. 
	} else if (pktBeacon_) {
		if (mhBackoff_.busy() == 0) {
			mhDefer_.start(phymib_.getDIFS());
			debug2 ("\tdefer DIFS for beacon=%f\n", phymib_.getDIFS());
		}
		
	} else if(pktManagement_) {
		if (mhBackoff_.busy() == 0) {
			mhDefer_.start( phymib_.getDIFS());
			debug2 ("\tdefer DIFS for Mngt=%f\n", phymib_.getDIFS());
		}
	} else if (scan_status_.step != SCAN_NONE) {
		//this is to forbid sending other types of packets

		//End NIST
	} else if(pktRTS_) {
		if (mhBackoff_.busy() == 0) {
			mhDefer_.start( phymib_.getDIFS());
			debug2 ("\tdefer DIFS for RTS=%f\n", phymib_.getDIFS());
		}
	} else if(pktTx_) {
		if (mhBackoff_.busy() == 0) {
			hdr_cmn *ch = HDR_CMN(pktTx_);
			struct hdr_mac802_11 *mh = HDR_MAC802_11(pktTx_);
			
			if ((u_int32_t) ch->size() < macmib_.getRTSThreshold()
			    || (u_int32_t) ETHER_ADDR(mh->dh_ra) == MAC_BROADCAST) {
				mhDefer_.start(phymib_.getDIFS());
				debug2 ("\tdefer DIFS for data=%f\n", phymib_.getDIFS());
                        } else {
				mhDefer_.start(phymib_.getSIFS());
				debug2 ("\tdefer SIFS for data=%f\n", phymib_.getSIFS() );
                        }
		}
	} else if(callback_) { //NIST: comment, this dequeue packet from upper (IFq)
		Handler *h = callback_;
		//check if we buffered a packet previously
		if (tmpPacket_) {
			send(tmpPacket_, callback_);
			tmpPacket_ = 0;
		}
		else {//notify IFq to get new packet
			qwait_watch_.set_current(NOW);
			callback_ = 0;
			h->handle((Event*) 0);
		}
	}
	setTxState(MAC_IDLE);
}

void
Mac802_11::rx_resume()
{
	assert(pktRx_ == 0);
	assert(mhRecv_.busy() == 0);
	setRxState(MAC_IDLE);
}


/* ======================================================================
   Timer Handler Routines
   ====================================================================== */
void
Mac802_11::backoffHandler()
{
	debug2 ("At %.9f Mac %d backoff expired\n", NOW, index_);
	if(pktCTRL_) {
		assert(mhSend_.busy() || mhDefer_.busy());
		return;
	}
	//NIST: add management packet
	//beacon don't have backoff...but if we backoff after recv ack
	if(check_pktBeacon()==0)
		return;
	//we need to check that the back if for management or not
	if(check_pktManagement() == 0)
		return;
	//end NIST
	if(check_pktRTS() == 0)
		return;

	if(check_pktTx() == 0)
		return;

}

void
Mac802_11::deferHandler()
{
	debug2 ("At %.9f Mac %d defer expired\n", NOW, index_);
	//NIST: add management packet to check list
	assert(pktCTRL_ || pktBeacon_ || pktManagement_ || pktRTS_ || pktTx_);
	//end NIST

	//Now a defer does not contain a backoff
	if(check_pktCTRL() == 0)
		return;
	//NIST: add beacon
	if(check_pktBeacon()==0)
		return;
	//end NIST

	assert(mhBackoff_.busy() == 0);
	
	if (!pktManagement_ && !pktRTS_) {
		//check if we waited for a SIFS
		hdr_cmn *ch = HDR_CMN(pktTx_);
		struct hdr_mac802_11 *mh = HDR_MAC802_11(pktTx_);
		
		if ((u_int32_t) ch->size() > macmib_.getRTSThreshold()
		    && (u_int32_t) ETHER_ADDR(mh->dh_ra) != MAC_BROADCAST) {
			//we only need to wait for SIFS, no backoff
			if(check_pktTx() == 0)
				return;
		}
	}

	//else pick a backoff
	mhBackoff_.start(cw_, is_idle());
	
	/*
	//NIST: add management packet
	if(check_pktManagement() == 0)
		return;
	//end NIST
	if(check_pktRTS() == 0)
		return;
	if(check_pktTx() == 0)
		return;
	*/

}

void
Mac802_11::navHandler()
{
	if(is_idle() && mhBackoff_.paused())
		mhBackoff_.resume(phymib_.getDIFS());
}

void
Mac802_11::recvHandler()
{
	recv_timer();
}

void
Mac802_11::sendHandler()
{
	send_timer();
}


void
Mac802_11::txHandler()
{
	if (EOTtarget_) {
		assert(eotPacket_);
		EOTtarget_->recv(eotPacket_, (Handler *) 0);
		eotPacket_ = NULL;
	}
	tx_active_ = 0;
}


/* ======================================================================
   The "real" Timer Handler Routines
   ====================================================================== */
void
Mac802_11::send_timer()
{
	debug2 ("At %.9f Mac %d send_timer\n", NOW, index_);
	switch(tx_state_) {
	/*
	 * Sent a RTS, but did not receive a CTS.
	 */
	case MAC_RTS:
		RetransmitRTS();
		break;
	/*
	 * Sent a CTS, but did not receive a DATA packet.
	 */
	case MAC_CTS:
		assert(pktCTRL_);
		Packet::free(pktCTRL_); 
		pktCTRL_ = 0;
		break;
	/*
	 * Sent DATA, but did not receive an ACK packet.
	 */
	case MAC_SEND:
		//NIST: added the condition to call RetransmitManagement();
		if(pktBeacon_==last_tx_) {
			debug2 ("At %.9f Mac %d free beacon\n", NOW, index_);
			Packet::free(pktBeacon_); 
			pktBeacon_ = 0;	
			if (pktRetransmit_) {
				last_tx_ = pktRetransmit_;
				pktRetransmit_ = NULL;
			}
			else
				break;
		}
		
		if(pktManagement_==last_tx_) {
			if (pktBeacon_) {
				//send beacon first
				pktRetransmit_ = pktManagement_;
			}
			else {
				RetransmitManagement();
			}
		}
		else if (pktTx_==last_tx_) {
			if (pktBeacon_) {
				pktRetransmit_ = pktTx_;
			}
			else {
				RetransmitDATA();
			}
		}
		// end NIST
		break;
	/*
	 * Sent an ACK, and now ready to resume transmission.
	 */
	case MAC_ACK:
		assert(pktCTRL_);
		Packet::free(pktCTRL_); 
		pktCTRL_ = 0;
		break;
	case MAC_IDLE:
		break;
	default:
		assert(0);
	}
	tx_resume();
}


/* ======================================================================
   Outgoing Packet Routines
   ====================================================================== */
int
Mac802_11::check_pktCTRL()
{
	struct hdr_mac802_11 *mh;
	double timeout;

	if(pktCTRL_ == 0)
		return -1;
	if(tx_state_ == MAC_CTS || tx_state_ == MAC_ACK)
		return -1;

	mh = HDR_MAC802_11(pktCTRL_);
							  
	switch(mh->dh_fc.fc_subtype) {
	/*
	 *  If the medium is not IDLE, don't send the CTS.
	 */
	case MAC_Subtype_CTS:
		if(!is_idle()) {
			discard(pktCTRL_, DROP_MAC_BUSY); pktCTRL_ = 0;
			return 0;
		}
		setTxState(MAC_CTS);
		/*
		 * timeout:  cts + data tx time calculated by
		 *           adding cts tx time to the cts duration
		 *           minus ack tx time -- this timeout is
		 *           a guess since it is unspecified
		 *           (note: mh->dh_duration == cf->cf_duration)
		 */		
		 timeout = txtime(phymib_.getCTSlen(), basicRate_)
                        + DSSS_MaxPropagationDelay                      // XXX
                        + sec(mh->dh_duration)
                        + DSSS_MaxPropagationDelay                      // XXX
                       - phymib_.getSIFS()
                       - txtime(phymib_.getACKlen(), basicRate_);
		break;
		/*
		 * IEEE 802.11 specs, section 9.2.8
		 * Acknowledments are sent after an SIFS, without regard to
		 * the busy/idle state of the medium.
		 */
	case MAC_Subtype_ACK:		
		setTxState(MAC_ACK);
		timeout = txtime(phymib_.getACKlen(), basicRate_);
		break;
	default:
		fprintf(stderr, "check_pktCTRL:Invalid MAC Control subtype\n");
		exit(1);
	}

	debug2 ("At %.9f Mac %d transmit packet control\n", NOW, index_);
	transmit(pktCTRL_, timeout);
	return 0;
}

//NIST: add handling of management packet
int
Mac802_11::check_pktBeacon()
{
	struct hdr_cmn *ch;
	double timeout;

        assert(mhBackoff_.busy() == 0);

	if(pktBeacon_ == 0)
		return -1;

	ch = HDR_CMN(pktBeacon_);

	if(! is_idle()) {
		debug2 ("At %.9f Mac %d defer beacon\n",NOW, index_);
		mhDefer_.start(phymib_.getDIFS());
		return 0;
	}
	setTxState(MAC_SEND);
 
	timeout = txtime(pktBeacon_);
	debug2 ("At %.9f Mac %d transmit beacon\n", NOW, index_);
	
	update_throughput(&txbw_watch_, (ch->txtime()+overhead_duration_)*dataRate_);
	//txbw_watch_.update((ch->txtime()+overhead_duration_)*dataRate_, NOW);
	//debug ("At %.9f Mac %d txbw %f\n", NOW, index_, txbw_watch_.average());
	txsucc_watch_.update((ch->txtime()+overhead_duration_)*dataRate_, NOW);
	debug ("At %.9f Mac %d txsucc %f\n", NOW, index_, txsucc_watch_.average());
	
	transmit(pktBeacon_, timeout);
	return 0;
}

int
Mac802_11::check_pktManagement()
{
	struct hdr_mac802_11 *mh;
	struct hdr_cmn *ch;
	double timeout;
		
	assert(mhBackoff_.busy() == 0);

	if(pktManagement_ == 0)
		return -1;

	mh = HDR_MAC802_11(pktManagement_);
	ch = HDR_CMN(pktManagement_);

 	switch(mh->dh_fc.fc_subtype) {
	case MAC_Subtype_AssocReq:
	case MAC_Subtype_AssocRes:
	case MAC_Subtype_ProbeRes:
		if(! is_idle()) {
			inc_cw();
			mhBackoff_.start(cw_, is_idle());
			return 0;
		}
		setTxState(MAC_SEND);
                  timeout = txtime(pktManagement_)
                                + DSSS_MaxPropagationDelay              // XXX
                               + phymib_.getSIFS()
                               + txtime(phymib_.getACKlen(), basicRate_)
                               + DSSS_MaxPropagationDelay;             // XXX
		break;
	case MAC_Subtype_ProbeReq:
		if(! is_idle()) {
			inc_cw();
			mhBackoff_.start(cw_, is_idle());
			return 0;
		}
		setTxState(MAC_SEND);
		timeout = txtime(pktManagement_)
			+ DSSS_MaxPropagationDelay              // XXX
			+ phymib_.getSIFS()
			+ txtime(phymib_.getACKlen(), basicRate_)
			+ DSSS_MaxPropagationDelay;             // XXX
		break;
 
	default:
		fprintf(stderr, "check_pktManagement:Invalid MAC Control subtype\n");
		exit(1);
	}
	debug2 ("At %.9f Mac %d transmit packet management\n", NOW, index_);
	if((u_int32_t)ETHER_ADDR(mh->dh_ra) != MAC_BROADCAST) {
		rtt_watch_.set_current(NOW);
	}
	else {
		update_throughput(&txbw_watch_, (ch->txtime()+overhead_duration_)*dataRate_);
		//txbw_watch_.update((ch->txtime()+overhead_duration_)*dataRate_, NOW);
		//debug ("At %.9f Mac %d txbw %f\n", NOW, index_, txbw_watch_.average());
		txsucc_watch_.update((ch->txtime()+overhead_duration_)*dataRate_, NOW);
		debug ("At %.9f Mac %d txsucc %f\n", NOW, index_, txsucc_watch_.average());
	}

	transmit(pktManagement_, timeout);
	return 0;
}
//end NIST

int
Mac802_11::check_pktRTS()
{
	struct hdr_mac802_11 *mh;
	double timeout;

	assert(mhBackoff_.busy() == 0);

	if(pktRTS_ == 0)
 		return -1;
	mh = HDR_MAC802_11(pktRTS_);

 	switch(mh->dh_fc.fc_subtype) {
	case MAC_Subtype_RTS:
		if(! is_idle()) {
			inc_cw();
			mhBackoff_.start(cw_, is_idle());
			return 0;
		}
		setTxState(MAC_RTS);
		timeout = txtime(phymib_.getRTSlen(), basicRate_)
			+ DSSS_MaxPropagationDelay                      // XXX
			+ phymib_.getSIFS()
			+ txtime(phymib_.getCTSlen(), basicRate_)
			+ DSSS_MaxPropagationDelay;
		break;
	default:
		fprintf(stderr, "check_pktRTS:Invalid MAC Control subtype\n");
		exit(1);
	}
	debug2 ("At %.9f Mac %d transmit packet RTS\n", NOW, index_);
	transmit(pktRTS_, timeout);
  

	return 0;
}

int
Mac802_11::check_pktTx()
{
	struct hdr_mac802_11 *mh;
	struct hdr_cmn *ch;
	double timeout;
	
	assert(mhBackoff_.busy() == 0);

	if(pktTx_ == 0)
		return -1;

	mh = HDR_MAC802_11(pktTx_);
	ch = HDR_CMN(pktTx_);

	switch(mh->dh_fc.fc_subtype) {
	case MAC_Subtype_Data:
		if(! is_idle()) {
			sendRTS(ETHER_ADDR(mh->dh_ra));
			inc_cw();
			debug2 ("At %.9f Mac %d check_pktTx() starts backoff\n", NOW, index_);
			mhBackoff_.start(cw_, is_idle());
			return 0;
		}
		setTxState(MAC_SEND);
		if((u_int32_t)ETHER_ADDR(mh->dh_ra) != MAC_BROADCAST) {
			timeout = txtime(pktTx_)
                                + DSSS_MaxPropagationDelay              // XXX
				+ phymib_.getSIFS()
				+ txtime(phymib_.getACKlen(), basicRate_)
				+ DSSS_MaxPropagationDelay;             // XXX
		
			rtt_watch_.set_current(NOW);
		}
		else {
			timeout = txtime(pktTx_);

			update_throughput(&txbw_watch_, (ch->txtime()+overhead_duration_)*dataRate_);
			//txbw_watch_.update((ch->txtime()+overhead_duration_)*dataRate_, NOW);
			//debug ("At %.9f Mac %d txbw %f\n", NOW, index_, txbw_watch_.average());
			txsucc_watch_.update((ch->txtime()+overhead_duration_)*dataRate_, NOW);
			debug ("At %.9f Mac %d txsucc %f\n", NOW, index_, txsucc_watch_.average());
		}
		break;
	default:
		fprintf(stderr, "check_pktTx:Invalid MAC Control subtype\n");
		exit(1);
	}
	debug2 ("At %.9f Mac %d transmit packet data\n", NOW, index_);
	transmit(pktTx_, timeout);
	return 0;
}
/*
 * Low-level transmit functions that actually place the packet onto
 * the channel.
 */
void
Mac802_11::sendRTS(int dst)
{	
	assert (pktTx_);

	assert(pktRTS_ == 0);

	/*
	 *  If the size of the packet is larger than the
	 *  RTSThreshold, then perform the RTS/CTS exchange.
	 */
	if( (u_int32_t) HDR_CMN(pktTx_)->size() < macmib_.getRTSThreshold() ||
            (u_int32_t) dst == MAC_BROADCAST) {
		debug2 ("At %.9f Mac %d don't send RTS to %d\n", NOW, index_, dst);
		//Packet::free(p);
		return;
	}
	else {
		debug2 ("At %.9f Mac %d send RTS to %d\n",NOW, index_, dst);
	}

	Packet *p = Packet::alloc();
	hdr_cmn* ch = HDR_CMN(p);
	struct rts_frame *rf = (struct rts_frame*)p->access(hdr_mac::offset_);

	ch->uid() = 0;
	ch->ptype() = PT_MAC;
	ch->size() = phymib_.getRTSlen();
	ch->iface() = -2;
	ch->error() = 0;

	bzero(rf, MAC_HDR_LEN);

	rf->rf_fc.fc_protocol_version = MAC_ProtocolVersion;
 	rf->rf_fc.fc_type	= MAC_Type_Control;
 	rf->rf_fc.fc_subtype	= MAC_Subtype_RTS;
 	rf->rf_fc.fc_to_ds	= 0;
 	rf->rf_fc.fc_from_ds	= 0;
 	rf->rf_fc.fc_more_frag	= 0;
 	rf->rf_fc.fc_retry	= 0;
 	rf->rf_fc.fc_pwr_mgt	= 0;
 	rf->rf_fc.fc_more_data	= 0;
 	rf->rf_fc.fc_wep	= 0;
 	rf->rf_fc.fc_order	= 0;

	//rf->rf_duration = RTS_DURATION(pktTx_);
	STORE4BYTE(&dst, (rf->rf_ra));
	
	/* store rts tx time */
 	ch->txtime() = txtime(ch->size(), basicRate_);
	
	STORE4BYTE(&index_, (rf->rf_ta));

	/* calculate rts duration field */	
	rf->rf_duration = usec(phymib_.getSIFS()
			       + txtime(phymib_.getCTSlen(), basicRate_)
			       + phymib_.getSIFS()
                               + txtime(pktTx_)
			       + phymib_.getSIFS()
			       + txtime(phymib_.getACKlen(), basicRate_));

	pktRTS_ = p;
}

void
Mac802_11::sendCTS(int dst, double rts_duration)
{
	Packet *p = Packet::alloc();
	hdr_cmn* ch = HDR_CMN(p);
	struct cts_frame *cf = (struct cts_frame*)p->access(hdr_mac::offset_);

	assert(pktCTRL_ == 0);

	ch->uid() = 0;
	ch->ptype() = PT_MAC;
	ch->size() = phymib_.getCTSlen();


	ch->iface() = -2;
	ch->error() = 0;
	//ch->direction() = hdr_cmn::DOWN;
	bzero(cf, MAC_HDR_LEN);

	cf->cf_fc.fc_protocol_version = MAC_ProtocolVersion;
	cf->cf_fc.fc_type	= MAC_Type_Control;
	cf->cf_fc.fc_subtype	= MAC_Subtype_CTS;
 	cf->cf_fc.fc_to_ds	= 0;
 	cf->cf_fc.fc_from_ds	= 0;
 	cf->cf_fc.fc_more_frag	= 0;
 	cf->cf_fc.fc_retry	= 0;
 	cf->cf_fc.fc_pwr_mgt	= 0;
 	cf->cf_fc.fc_more_data	= 0;
 	cf->cf_fc.fc_wep	= 0;
 	cf->cf_fc.fc_order	= 0;
	
	//cf->cf_duration = CTS_DURATION(rts_duration);
	STORE4BYTE(&dst, (cf->cf_ra));
	
	/* store cts tx time */
	ch->txtime() = txtime(ch->size(), basicRate_);
	
	/* calculate cts duration */
	cf->cf_duration = usec(sec(rts_duration)
                              - phymib_.getSIFS()
                              - txtime(phymib_.getCTSlen(), basicRate_));


	//printf ("At %.9f in %d send CTS\n", NOW, index_);
	pktCTRL_ = p;
	
}

void
Mac802_11::sendACK(int dst)
{
	Packet *p = Packet::alloc();
	hdr_cmn* ch = HDR_CMN(p);
	struct ack_frame *af = (struct ack_frame*)p->access(hdr_mac::offset_);

	assert(pktCTRL_ == 0);

	ch->uid() = 0;
	ch->ptype() = PT_MAC;
	// CHANGE WRT Mike's code
	ch->size() = phymib_.getACKlen();
	ch->iface() = -2;
	ch->error() = 0;
	
	bzero(af, MAC_HDR_LEN);

	af->af_fc.fc_protocol_version = MAC_ProtocolVersion;
 	af->af_fc.fc_type	= MAC_Type_Control;
 	af->af_fc.fc_subtype	= MAC_Subtype_ACK;
 	af->af_fc.fc_to_ds	= 0;
 	af->af_fc.fc_from_ds	= 0;
 	af->af_fc.fc_more_frag	= 0;
 	af->af_fc.fc_retry	= 0;
 	af->af_fc.fc_pwr_mgt	= 0;
 	af->af_fc.fc_more_data	= 0;
 	af->af_fc.fc_wep	= 0;
 	af->af_fc.fc_order	= 0;

	//af->af_duration = ACK_DURATION();
	STORE4BYTE(&dst, (af->af_ra));

	/* store ack tx time */
 	ch->txtime() = txtime(ch->size(), basicRate_);
	
	/* calculate ack duration */
 	af->af_duration = 0;	
	
	pktCTRL_ = p;
}

void
Mac802_11::sendDATA(Packet *p)
{
	hdr_cmn* ch = HDR_CMN(p);
	struct hdr_mac802_11* dh = HDR_MAC802_11(p);

	assert(pktTx_ == 0);

	/*
	 * Update the MAC header
	 */
	ch->size() += phymib_.getHdrLen11();

	dh->dh_fc.fc_protocol_version = MAC_ProtocolVersion;
	dh->dh_fc.fc_type       = MAC_Type_Data;
	dh->dh_fc.fc_subtype    = MAC_Subtype_Data;
	
	dh->dh_fc.fc_to_ds      = 0;
	dh->dh_fc.fc_from_ds    = 0;
	dh->dh_fc.fc_more_frag  = 0;
	dh->dh_fc.fc_retry      = 0;
	dh->dh_fc.fc_pwr_mgt    = 0;
	dh->dh_fc.fc_more_data  = 0;
	dh->dh_fc.fc_wep        = 0;
	dh->dh_fc.fc_order      = 0;

	/* store data tx time */
 	//ch->txtime() = txtime(ch->size(), dataRate_);
	ch->txtime() = txtime(ch->size(), getTxDatarate(p));

	if((u_int32_t)ETHER_ADDR(dh->dh_ra) != MAC_BROADCAST) {
		/* store data tx time for unicast packets */
		//ch->txtime() = txtime(ch->size(), dataRate_);
		ch->txtime() = txtime(ch->size(), getTxDatarate(p));
				
		dh->dh_duration = usec(txtime(phymib_.getACKlen(), basicRate_)
				       + phymib_.getSIFS());



	} else {
		/* store data tx time for broadcast packets (see 9.6) */
		ch->txtime() = txtime(ch->size(), basicRate_);
		
		dh->dh_duration = 0;
	}
	
	//NIST: add BW exchange information if a MS
	if(addr() != bss_id()) {
		txprob_watch_.update(calculate_txprob());
		dh->dh_txprob = txprob_watch_.instant();
	}
	txframesize_watch_.update(ch->size());
	first_datatx_ = true;
	
	pktTx_ = p;
}

/* ======================================================================
   Retransmission Routines
   ====================================================================== */
void
Mac802_11::RetransmitRTS()
{
	assert(pktTx_);
	assert(pktRTS_);
	assert(mhBackoff_.busy() == 0);
	macmib_.RTSFailureCount++;

	//NIST: Count lost pkt
	//mac_stats_.txerror_update(1);

	ssrc_ += 1;			// STA Short Retry Count
		
	if(ssrc_ >= macmib_.getShortRetryLimit()) {
		discard(pktRTS_, DROP_MAC_RETRY_COUNT_EXCEEDED); pktRTS_ = 0;
		/* tell the callback the send operation failed 
		   before discarding the packet */
		hdr_cmn *ch = HDR_CMN(pktTx_);
		if (ch->xmit_failure_) {
                        /*
                         *  Need to remove the MAC header so that 
                         *  re-cycled packets don't keep getting
                         *  bigger.
                         */
			ch->size() -= phymib_.getHdrLen11();
                        ch->xmit_reason_ = XMIT_REASON_RTS;
                        ch->xmit_failure_(pktTx_->copy(),
                                          ch->xmit_failure_data_);
                }
		discard(pktTx_, DROP_MAC_RETRY_COUNT_EXCEEDED); 
		pktTx_ = 0;
		ssrc_ = 0;
		rst_cw();

		// NIST: collect the number of re-tx and reset counter
		retx_watch_.update(retx_watch_.current());
		retx_watch_.set_current(0);
		debug("At %.9f Mac %d retx count (Retx RTS) is %f \n",NOW, index_, retx_watch_.current());
		
	} else {
		struct rts_frame *rf;
		rf = (struct rts_frame*)pktRTS_->access(hdr_mac::offset_);
		rf->rf_fc.fc_retry = 1;

		inc_cw();
		mhBackoff_.start(cw_, is_idle());

		// NIST: increment re-tx count
		retx_watch_.set_current(retx_watch_.current()+1);
		debug("At %.9f Mac %d retx count (Retx RTS) is %f \n",NOW, index_, retx_watch_.current());
	}
}

void
Mac802_11::RetransmitDATA()
{
	struct hdr_cmn *ch;
	struct hdr_mac802_11 *mh;
	u_int32_t *rcount, thresh;
	assert(mhBackoff_.busy() == 0);

	assert(pktTx_);
	assert(pktRTS_ == 0);

	ch = HDR_CMN(pktTx_);
	mh = HDR_MAC802_11(pktTx_);

	//NIST: Count lost pkt
	txerror_watch_.update(1);
	txerror_watch_.set_current(NOW);
	debug ("At %.9f Mac %d txerror %f\n", NOW, index_, txerror_watch_.average());
	
	//NIST: Keep track of collision size
	if(mh->dh_fc.fc_retry == 1) {
		txcoll_watch_.update((ch->txtime()+coll_overhead_duration_)*dataRate_, NOW);
		debug ("At %.9f Mac %d txcoll %f\n", NOW, index_, txcoll_watch_.average());
		update_throughput(&txbw_watch_, (ch->txtime()+coll_overhead_duration_)*dataRate_);
		//txbw_watch_.update((ch->txtime()+coll_overhead_duration_)*dataRate_, NOW);
		//debug ("At %.9f Mac %d txbw %f\n", NOW, index_, txbw_watch_.average());
	
		if (bss_id() == addr()) {
			collsize_watch_.update(ch->size());
			collsize_watch_.set_current(NOW);
		}
	}

	/*
	 *  Broadcast packets don't get ACKed and therefore
	 *  are never retransmitted.
	 */
	if((u_int32_t)ETHER_ADDR(mh->dh_ra) == MAC_BROADCAST) {
		Packet::free(pktTx_); 
		pktTx_ = 0;

		/*
		 * Backoff at end of TX.
		 */
		rst_cw();
		debug2 ("At %.9f Mac %d Backoff after discarding broadcast data\n", NOW, index_);
		
		debug2 ("Packet beacon = %x \n", pktBeacon_);
		mhBackoff_.start(cw_, is_idle());

		return;
	}

	macmib_.ACKFailureCount++;

	if((u_int32_t) ch->size() <= macmib_.getRTSThreshold()) {
                rcount = &ssrc_;
               thresh = macmib_.getShortRetryLimit();
        } else {
                rcount = &slrc_;
               thresh = macmib_.getLongRetryLimit();
        }

	(*rcount)++;

	if(*rcount >= thresh) {
		/* IEEE Spec section 9.2.3.5 says this should be greater than
		   or equal */
		macmib_.FailedCount++;

		/* tell the callback the send operation failed 
		   before discarding the packet */
		hdr_cmn *ch = HDR_CMN(pktTx_);
		if (ch->xmit_failure_) {
                        ch->size() -= phymib_.getHdrLen11();
			ch->xmit_reason_ = XMIT_REASON_ACK;
                        ch->xmit_failure_(pktTx_->copy(),
                                          ch->xmit_failure_data_);
                }

		discard(pktTx_, DROP_MAC_RETRY_COUNT_EXCEEDED); 
		pktTx_ = 0;
		*rcount = 0;
		rst_cw();

		// NIST: collect the number of re-tx and reset counter
		retx_watch_.update(retx_watch_.current());
		retx_watch_.set_current(0);
		debug("At %.9f Mac %d retx count (Retx Data) is %f \n",NOW, index_, retx_watch_.current());

		update_watch(&txloss_watch_, 1.0);
		//txloss_watch_.update(1);
		//debug ("At %.9f Mac %d txloss %f\n", NOW, index_, txloss_watch_.average());
	
#ifdef RETX_TRIGGER
		ms_retx_handover();
#endif
	}
	else {
		struct hdr_mac802_11 *dh;
		dh = HDR_MAC802_11(pktTx_);
		dh->dh_fc.fc_retry = 1;


		sendRTS(ETHER_ADDR(mh->dh_ra));
		inc_cw();
		debug2 ("At %.9f Mac %d retransmitDATA starts backoff\n", NOW, index_);
		mhBackoff_.start(cw_, is_idle());

		// NIST: increment re-tx count
		retx_watch_.set_current(retx_watch_.current()+1);
		debug("At %.9f Mac %d retx count (Retx Data) is %f \n",NOW, index_, retx_watch_.current());
	}
}

//NIST: add association request/response handling
void Mac802_11::sendAssocRequest(int dst)
{
	debug("At %.9f Mac %d sends an Association Request to Mac %d\n",NOW, index_, dst);

	hdv_state_ = MAC_BEING_CONNECTED;

	Packet *p = Packet::alloc();
	hdr_cmn* ch = HDR_CMN(p);
	struct association_request_frame *assocReq = (struct association_request_frame*)p->access(hdr_mac::offset_);
	
	ch->uid() = 0;
	ch->ptype() = PT_MAC;
	ch->size() = phymib_.getAssocReqlen();
	ch->iface() = -2;
	ch->error() = 0;

	//bzero(assocReq, MAC_HDR_LEN);
	assert(pktManagement_ == 0);
	hdr_src((char*)assocReq, addr());

	//Frame control fields
	assocReq->arf_fc.fc_protocol_version = MAC_ProtocolVersion;
 	assocReq->arf_fc.fc_type	= MAC_Type_Management;
 	assocReq->arf_fc.fc_subtype	= MAC_Subtype_AssocReq;
 	assocReq->arf_fc.fc_to_ds	= 0;
 	assocReq->arf_fc.fc_from_ds	= 0;
 	assocReq->arf_fc.fc_more_frag	= 0;
 	assocReq->arf_fc.fc_retry	= 0;
 	assocReq->arf_fc.fc_pwr_mgt	= 0;
 	assocReq->arf_fc.fc_more_data	= 0;
 	assocReq->arf_fc.fc_wep	= 0;
 	assocReq->arf_fc.fc_order	= 0;

	STORE4BYTE(&dst, (assocReq->arf_ra));
	
	//ch->txtime() = txtime(ch->size(), dataRate_);
	ch->txtime() = txtime(ch->size(), getTxDatarate(p));
		
	assocReq->arf_duration = usec(txtime(phymib_.getACKlen(), basicRate_)
				       + phymib_.getSIFS());

	//add supported datarate (not the real encoding but enough for our purposes)
	//Supported datarate is encoded as follow:
	//Invalid = 0x0
	//1Mbps = 0x1
	//2Mbps = 0x2
	//5.5Mbps = 0x3
	//11Mbps = 0x4
	if (dataRate_ == 1000000) {
		assocReq->supported_rate = 0x1;
	} else if (dataRate_ == 2000000) {
                assocReq->supported_rate = 0x2;
        } else if (dataRate_ == 5500000) {
                assocReq->supported_rate = 0x3;
        } else if (dataRate_ == 11000000) {
                assocReq->supported_rate = 0x4;
        } else {
		assocReq->supported_rate = 0x0; 
	}

	if (pktManagement_) {
		managementQueue_->enque (p);
		return;
	}
	//else we can try to send it now
	pktManagement_ = p;

	/*
	 *  If the medium is IDLE, we must wait for a DIFS
	 *  Space before transmitting.
	 */
	if(mhBackoff_.busy() == 0) {
		if(is_idle()) {
			if (mhDefer_.busy() == 0) {
				/*
				 * If we are already deferring, there is no
				 * need to reset the Defer timer.
				 */
				mhDefer_.start(phymib_.getDIFS());
			}
		} else {
			/*
			 * If the medium is NOT IDLE, then we start
			 * the backoff timer.
			 */
			mhBackoff_.start(cw_, is_idle());
		}
	}
	
}

void Mac802_11::sendAssocResponse(int dst, Client *cl)
{
	debug("At %.9f Mac %d sends an Association Response to Mac %d\n",NOW, index_, dst);

	Packet *p = Packet::alloc();
	hdr_cmn* ch = HDR_CMN(p);
	struct association_response_frame *assocResp = (struct association_response_frame*)p->access(hdr_mac::offset_);
	
	//assert(pktTx_ == 0);	
	//assert(pktManagement_ == 0);
	
	ch->uid() = 0;
	ch->ptype() = PT_MAC;
	ch->size() = phymib_.getAssocResplen();
	ch->iface() = -2;
	ch->error() = 0;

	
	//bzero(assocResp, MAC_HDR_LEN); 

	hdr_src((char*)assocResp, addr());
	hdr_dst((char*)assocResp, dst);
	
	//We configure the stats field:
	//Here one can change the condition to refuse association request
	//at the mac layer. For us, always accept
	if (1){
		assocResp->status_ = 4;
		nbAccepted_++; 
		
		//fprintf(finfocom,"Bw %f\n",base_station_bw);
		//fflush(finfocom);		
	}
	else { 
		assocResp->status_ = 0;
		nbRefused_++;
		
		//fprintf(finfocom,"Bw %f\n", base_station_bw); 
		//fflush(finfocom);
	}
	
	//Frame control fields
	assocResp->arf_fc.fc_protocol_version = MAC_ProtocolVersion;
 	assocResp->arf_fc.fc_type	= MAC_Type_Management;
 	assocResp->arf_fc.fc_subtype	= MAC_Subtype_AssocRes;
 	assocResp->arf_fc.fc_to_ds	= 0;
 	assocResp->arf_fc.fc_from_ds	= 0;
 	assocResp->arf_fc.fc_more_frag	= 0;
 	assocResp->arf_fc.fc_retry	= 0;
 	assocResp->arf_fc.fc_pwr_mgt	= 0;
 	assocResp->arf_fc.fc_more_data	= 0;
 	assocResp->arf_fc.fc_wep	= 0;
 	assocResp->arf_fc.fc_order	= 0;

	STORE4BYTE(&dst, (assocResp->arf_ra)); //Richard:isn't this similar to hdr_dst((char*)assocResp, dst);
	
	//ch->txtime() = txtime(ch->size(), dataRate_);
	ch->txtime() = txtime(ch->size(), getTxDatarate(p));

	assocResp->arf_duration = usec(txtime(phymib_.getACKlen(), basicRate_)
				       + phymib_.getSIFS());

	cl->pktManagement_ = p;
	cl->status() = CLIENT_PENDING;
	//If we were not already sending a management frame
	if(pktManagement_ == 0)	{
		pktManagement_ = p;
	}
	else {
		managementQueue_->enque(p);
	}
	
}

//NIST: Retransmit an Association Request or an Association Response
void
Mac802_11::RetransmitManagement()
{
	struct hdr_cmn *ch;
	struct hdr_mac802_11 *mh;
	u_int32_t *rcount, thresh;
	assert(mhBackoff_.busy() == 0);

	assert(pktManagement_);

	ch = HDR_CMN(pktManagement_);
	mh = HDR_MAC802_11(pktManagement_);

	//NIST: Count lost pkt
	//mac_stats_.txerror_update(1);

	/*
	 *  Broadcast packets don't get ACKed and therefore
	 *  are never retransmitted.
	 */
	if((u_int32_t)ETHER_ADDR(mh->dh_ra) == MAC_BROADCAST) {
		Packet::free(pktManagement_); 
		pktManagement_ = 0;
		pktManagement_ = managementQueue_->deque();

		/*
		 * Backoff at end of TX.
		 */
		rst_cw();
	        debug2 ("At %.9f Mac %d Backoff after discarding management broadcast\n", NOW, index_);
		
		mhBackoff_.start(cw_, is_idle());

		return;
	}

	macmib_.ACKFailureCount++;
	
	rcount = &ssrc_;
	thresh = macmib_.getShortRetryLimit();

	(*rcount)++;

	if(*rcount >= thresh) {
		debug("At %.9f Mac %d : Number of retransmission reached for a management frame\n",\
		      NOW, index_);

		//Update list of client (list is empty for MN)
		for ( Client *n=list_client_head_.lh_first; n ; n=n->next_entry()) {
			if(n->pkt() == pktManagement_) {
				n->cancel_timer ();
				n->remove_entry();
				break;
			}
		}

		//if it is an association request, then disconnect
		u_int8_t  type = mh->dh_fc.fc_type;
		u_int8_t  subtype = mh->dh_fc.fc_subtype;
		if (type==MAC_Type_Management && subtype==MAC_Subtype_AssocReq){
			link_disconnect ();
		} else {
			Packet::free(pktManagement_); 
			pktManagement_ = 0;
		}
		pktManagement_ = managementQueue_->deque();

		// NIST: collect the number of re-tx and reset counter
		retx_watch_.update(retx_watch_.current());
		retx_watch_.set_current(0);
		debug("At %.9f Mac %d retx count (Retx Management) is %f \n",NOW, index_, retx_watch_.current());
#ifdef RETX_TRIGGER
		ms_retx_handover();
#endif
	}
	else {
		struct hdr_mac802_11 *dh;
		dh = HDR_MAC802_11(pktManagement_);
		dh->dh_fc.fc_retry = 1;

		debug ("At %.9f Mac %d : Retransmission of a management frame%d\n", \
		       NOW, index_,dh->dh_fc.fc_subtype);

		inc_cw();
		mhBackoff_.start(cw_, is_idle());

		// NIST: increment re-tx count
		retx_watch_.set_current(retx_watch_.current()+1);
		debug("At %.9f Mac %d retx count (Retx Management) is %f \n",NOW, index_, retx_watch_.current());
	}
}

//end NIST




/* ======================================================================
   Incoming Packet Routines
   ====================================================================== */
void
Mac802_11::send(Packet *p, Handler *h)
{
	struct hdr_mac802_11* dh = HDR_MAC802_11(p);

	//NIST: check if I am an AP and the destination node is associated
	//      or it is a broadcast
	if (index_ == bss_id_) {
		struct hdr_mac802_11 *mh = HDR_MAC802_11(p);
		if ((u_int32_t) ETHER_ADDR(mh->dh_ra) != MAC_BROADCAST ) {
			//check if associated
			bool found = false;
			for ( Client *n=list_client_head_.lh_first; n ; n=n->next_entry()) {
				if (n->status() == CLIENT_ASSOCIATED) {
					found = true;
					break;
				}
			}
			if (!found) {
				//not associated, drop the packet
				debug ("At %.9f Mac %d (BS) drops packet because destination is not associated\n", NOW, index_);
				discard(p, "NAS");
				callback_ = h;
				//Packet::free (p);
				if (mhBackoff_.busy() == 0 && mhDefer_.busy() == 0 && mhSend_.busy() == 0) {
					tx_resume ();
				}
				return;
			}
		}
	}
		

	//NIST: handle management packet
	//check if we already have a management packet to handle
	//if yes, then buffer this packet. It will be taken
	//care after the managment packet is finished.
	//printf ("=====At %.9f MAC %d received data to send %d\n",NOW, index_, HDR_CMN(p)->ptype()==PT_MESSAGE?1:0);
	if (pktManagement_ || pktBeacon_) {
		tmpPacket_ = p;
		callback_ = h;
		return; //we keep for later
	}
	//end NIST

	EnergyModel *em = netif_->node()->energy_model();
	if (em && em->sleep()) {
		em->set_node_sleep(0);
		em->set_node_state(EnergyModel::INROUTE);
	}
	
	callback_ = h;
	sendDATA(p);
	sendRTS(ETHER_ADDR(dh->dh_ra));

	/*
	 * Assign the data packet a sequence number.
	 */
	dh->dh_scontrol = sta_seqno_++;

	/*
	 *  If the medium is IDLE, we must wait for a DIFS
	 *  Space before transmitting.
	 */
	if(mhBackoff_.busy() == 0) {
		if(is_idle()) {
			if (mhDefer_.busy() == 0) {
				/*
				 * If we are already deferring, there is no
				 * need to reset the Defer timer.
				 */
				mhDefer_.start(phymib_.getDIFS());
				//printf ("\tnew defer time=%f\n", phymib_.getDIFS() + rTime);
			}
		} else {
			/*
			 * If the medium is NOT IDLE, then we start
			 * the backoff timer.
			 */
			mhBackoff_.start(cw_, is_idle());
		}
	}
}

void
Mac802_11::recv(Packet *p, Handler *h)
{
	struct hdr_cmn *hdr = HDR_CMN(p);
	/*
	 * Sanity Check
	 */
	assert(initialized());

	/*
	 *  Handle outgoing packets.
	 */
	if(hdr->direction() == hdr_cmn::DOWN) {
		// Add 1 for the frame just received from the queue.
		qwait_watch_.update(NOW-qwait_watch_.current());
                send(p, h);
                return;
        }

	/*
	 *  Handle incoming packets.
	 *
	 *  We just received the 1st bit of a packet on the network
	 *  interface.
	 *
	 */

	/*
	 *  If the interface is currently in transmit mode, then
	 *  it probably won't even see this packet.  However, the
	 *  "air" around me is BUSY so I need to let the packet
	 *  proceed.  Just set the error flag in the common header
	 *  to that the packet gets thrown away.
	 */
	if(tx_active_ && hdr->error() == 0) {
		hdr->error() = 1;
	}

	if(rx_state_ == MAC_IDLE) {
		setRxState(MAC_RECV);
		pktRx_ = p;
		/*
		 * Schedule the reception of this packet, in
		 * txtime seconds.
		 */
		mhRecv_.start(txtime(p));
	} else {
		/*
		 *  If the power of the incoming packet is smaller than the
		 *  power of the packet currently being received by at least
                 *  the capture threshold, then we ignore the new packet.
		 */
		if(pktRx_->txinfo_.RxPr / p->txinfo_.RxPr >= p->txinfo_.CPThresh) {
			/*printf ("At %.9f in %d capture RX=%d/%d P=%d/%d\n", NOW, index_,\
				HDR_MAC802_11(p)->dh_fc.fc_subtype, \
				HDR_MAC802_11(p)->dh_fc.fc_type, \
				HDR_MAC802_11(pktRx_)->dh_fc.fc_subtype, \
				HDR_MAC802_11(pktRx_)->dh_fc.fc_type);
			*/
			capture(p);
		} else {
			/*printf ("At %.9f in %d collision RX=%d/%d P=%d/%d\n", NOW, index_, \
				HDR_MAC802_11(p)->dh_fc.fc_subtype, \
				HDR_MAC802_11(p)->dh_fc.fc_type, \
				HDR_MAC802_11(pktRx_)->dh_fc.fc_subtype, \
				HDR_MAC802_11(pktRx_)->dh_fc.fc_type);
			*/
			collision(p);
		}
	}
}

void
Mac802_11::recv_timer()
{
	u_int32_t src; 
	hdr_cmn *ch = HDR_CMN(pktRx_);
	hdr_mac802_11 *mh = HDR_MAC802_11(pktRx_);
	u_int32_t dst = ETHER_ADDR(mh->dh_ra);
	
	u_int8_t  type = mh->dh_fc.fc_type;
	u_int8_t  subtype = mh->dh_fc.fc_subtype;

	assert(pktRx_);
	assert(rx_state_ == MAC_RECV || rx_state_ == MAC_COLL);

	//NIST: collect number of rx packet stats
	numrx_watch_.update(NOW-numrx_watch_.current());
	numrx_watch_.set_current(NOW);
	
        /*
         *  If the interface is in TRANSMIT mode when this packet
         *  "arrives", then I would never have seen it and should
         *  do a silent discard without adjusting the NAV.
         */
        if(tx_active_) {
                Packet::free(pktRx_);
                goto done;
        }

	/*
	 * Handle collisions.
	 */
	if(rx_state_ == MAC_COLL) {
		//NIST: collect packet loss stats
		pktloss_watch_.update(1);

		rxcoll_watch_.update((ch->txtime()+coll_overhead_duration_)*dataRate_, NOW);
		debug ("At %.9f Mac %d rxcoll %f\n", NOW, index_, rxcoll_watch_.average());
		update_throughput(&rxbw_watch_, (ch->txtime()+coll_overhead_duration_)*dataRate_);
		//rxbw_watch_.update((ch->txtime()+coll_overhead_duration_)*dataRate_, NOW);
		//debug ("At %.9f Mac %d rxbw %f\n", NOW, index_, rxbw_watch_.average());
		
		discard(pktRx_, DROP_MAC_COLLISION);		
		set_nav(usec(phymib_.getEIFS()));
		goto done;
	}
	
	//NIST: collect Rx power stats
	rxp_watch_.update(10*log10(pktRx_->txinfo_.RxPr*1e3));

	//NIST: add collection of stats at Base station.
	//Note1: Should this go after checking for error??
	//Note2: why only at base station?
	if((u_int32_t)index_ == (u_int32_t)bss_id_){
		if(bwTimer_.status() == TIMER_IDLE){
			bwTimer_.resched (bw_timeout_);
		}
		bool found = false;
		src = ETHER_ADDR(mh->dh_ta);
		
		for ( BandwidthInfo *n=collector_bw_.lh_first; n ; n=n->next_entry()) {
			if((u_int32_t)n->id() == src) {
				n->nb_pkts() ++;
				n->size_pkts() += ch->size();
				found = true;
				break;
			}
		}
		if( !found){
			BandwidthInfo *new_info = new BandwidthInfo (src);
			new_info->nb_pkts()++;
			new_info->size_pkts() =  ch->size();
			new_info->insert_entry (&collector_bw_);
		}
	}

	//end NIST


	/*
	 * Check to see if this packet was received with enough
	 * bit errors that the current level of FEC still could not
	 * fix all of the problems - ie; after FEC, the checksum still
	 * failed.
	 */
	if( ch->error() ) {
		if (!((type == MAC_Type_Control && subtype == MAC_Subtype_CTS)
		      || (type == MAC_Type_Control && subtype == MAC_Subtype_ACK))
		    && hdv_state_==MAC_CONNECTED) {
			src = ETHER_ADDR(mh->dh_ta);
			if (src == (u_int32_t) bss_id_ 
			    && (index_ != bss_id_)) {
				nb_error_++;
				/*if (nb_error_ > max_error_) {
					debug ("At %.9f Mac %d, received too many packet with errors\n", NOW, index_);
					link_disconnect ();
					set_nav(usec(phymib_.getEIFS()));
					return;
					}*/
			}
		}

		Packet::free(pktRx_);
		set_nav(usec(phymib_.getEIFS()));

		//NIST: collect packet loss stats
		pktloss_watch_.update(1);

		goto done;
	}

	//NIST: collect packet loss stats
	pktloss_watch_.update(0);

	//NIST: add link going down and rollback trigger
	/* Note: CTS and ACK messages do not contain source information
	 *       we cannot use src with these types of packets.
	 */
	if (!((type == MAC_Type_Control && subtype == MAC_Subtype_CTS)
	      || (type == MAC_Type_Control && subtype == MAC_Subtype_ACK))) {
		src = ETHER_ADDR(mh->dh_ta);
		if (src == (u_int32_t) bss_id_ 
		    && (index_ != bss_id_)) {
			nb_error_ = 0;
#ifndef RETX_TRIGGER
			ms_rxp_handover();
#endif
			if (!pktRx_) {
				return; //a handler processed the link event
			}
			prev_pr = pktRx_->txinfo_.RxPr;
		}
		else if (index_ == bss_id_) {
			//I am a BS and I check if this packet is from 
			//one of the associated MNs
			for ( Client *n=list_client_head_.lh_first; n ; n=n->next_entry()) {
				if((u_int32_t) n->id() == src) { 
					if (n->status() == CLIENT_ASSOCIATED) {
						if ((pktRx_->txinfo_.RxPr < (pr_limit_*RXThreshold_)) && (pktRx_->txinfo_.RxPr < n->getPacketPr())) {
							double probability = 1-((pktRx_->txinfo_.RxPr-RXThreshold_)/((pr_limit_*RXThreshold_)-RXThreshold_));
							Mac::send_link_going_down (src, addr(), -1, (int)(100*probability), LGD_RC_LINK_PARAM_DEGRADING, eventId_++);
							n->going_down() = true;
						}
						else {
							if (n->going_down()) {
								Mac::send_link_rollback (src, bss_id_, eventId_-1);
								n->going_down() = false;
							}
						}
						if (!pktRx_) {
							return; //a handler processed the link event
						}
						n->getPacketPr() = pktRx_->txinfo_.RxPr;
					}
				}
			}
		}
	}

	//end NIST

	/*
	 * IEEE 802.11 specs, section 9.2.5.6
	 *	- update the NAV (Network Allocation Vector)
	 */
	if(dst != (u_int32_t)index_) {
		set_nav(mh->dh_duration);
	}

        /* tap out - */
        if (tap_ && type == MAC_Type_Data &&
            MAC_Subtype_Data == subtype ) 
		tap_->tap(pktRx_);
	/*
	 * Adaptive Fidelity Algorithm Support - neighborhood infomation 
	 * collection
	 *
	 * Hacking: Before filter the packet, log the neighbor node
	 * I can hear the packet, the src is my neighbor
	 */
	if (netif_->node()->energy_model() && 
	    netif_->node()->energy_model()->adaptivefidelity()) {
		src = ETHER_ADDR(mh->dh_ta);
		netif_->node()->energy_model()->add_neighbor(src);
	}

	/*
	 * Address Filtering
	 */
	if(dst != (u_int32_t)index_ && dst != MAC_BROADCAST) {
		/*
		 *  We don't want to log this event, so we just free
		 *  the packet instead of ing the drop routine.
		 */
		discard(pktRx_, "---");
		goto done;
	}

	//NIST: collect receive throughput stats
	if (type == MAC_Type_Data && MAC_Subtype_Data == subtype ) {
		update_throughput(&rxtp_watch_, ch->size()*8);
		//rxtp_watch_.update(ch->size()*8, NOW);
		//debug ("At %.9f Mac %d rxtp %f\n", NOW, index_, rxtp_watch_.average());
	}
	if(type != MAC_Type_Control ) {
		if(dst != MAC_BROADCAST){
			update_throughput(&rxbw_watch_, (ch->txtime()+overhead_duration_)*dataRate_);
			//rxbw_watch_.update((ch->txtime()+overhead_duration_)*dataRate_, NOW);
			rxsucc_watch_.update((ch->txtime()+overhead_duration_)*dataRate_, NOW);
		} else {
			update_throughput(&rxbw_watch_, (ch->txtime()+phymib_.getDIFS())*dataRate_);
			//rxbw_watch_.update((ch->txtime()+phymib_.getDIFS())*dataRate_, NOW);
			rxsucc_watch_.update((ch->txtime()+phymib_.getDIFS())*dataRate_, NOW);
		}
		//debug ("At %.9f Mac %d rxbw %f\n", NOW, index_, rxbw_watch_.average());
		debug ("At %.9f Mac %d rxsucc %f\n", NOW, index_, rxsucc_watch_.average());
	}

	//NIST: Keep track of collision size
	if ((bss_id() == addr()) && (mh->dh_fc.fc_retry == 1)) {
		collsize_watch_.update(ch->size());
		collsize_watch_.set_current(NOW);
	}

	switch(type) {

	case MAC_Type_Management:
		//NIST: add association handling
		switch(subtype) {
		case MAC_Subtype_AssocReq:
			recvAssocRequest(pktRx_);
			break;
		case MAC_Subtype_AssocRes:
			recvAssocResponse(pktRx_);
			break;
		case MAC_Subtype_Beacon2:
			recvBeacon(pktRx_);
			break;
		case MAC_Subtype_ProbeReq:
			recvProbeRequest(pktRx_);
			break;
		case MAC_Subtype_ProbeRes:
			recvProbeResponse(pktRx_);			
			break;
		default:
			discard(pktRx_, DROP_MAC_PACKET_ERROR);
			goto done;
		}
		break;
		//end NIST
	case MAC_Type_Control:
		switch(subtype) {
		case MAC_Subtype_RTS:
			recvRTS(pktRx_);
			break;
		case MAC_Subtype_CTS:
			recvCTS(pktRx_);
			break;
		case MAC_Subtype_ACK:
			recvACK(pktRx_);
			break;
		default:
			fprintf(stderr,"recvTimer1:Invalid MAC Control Subtype %x\n",
				subtype);
			exit(1);
		}
		break;
	case MAC_Type_Data:
		//NIST: add L2 connection 
		/* if we are not attached to bss_id, we cannot receive data */
		//check if I am connected to my base station
		//src = ETHER_ADDR(mh->dh_ta);
		//Note: Only let routing algorithm messages pass through
		if ( ch->ptype()!=PT_MESSAGE && (index_ != bss_id_) && ((src != (u_int32_t) bss_id_) || hdv_state_ != MAC_CONNECTED) ) {
			//printf ("at %f src = %d and type=%d, subtype=%x...", NOW,src,type,subtype);
			debug ("At %.9f Mac %d (Node %s) drop packet because does not belong to bss_id %d (hdv_state_=%d)\n", \
				NOW, index_, Address::instance().print_nodeaddr(netif_->node()->address()), bss_id_, hdv_state_);
			discard(pktRx_, "NCO");
			goto done;
		}
		//end NIST
		switch(subtype) {
		case MAC_Subtype_Data:
			recvDATA(pktRx_);
			break;
		default:
			fprintf(stderr, "recv_timer2:Invalid MAC Data Subtype %x\n",
				subtype);
			exit(1);
		}
		break;
	default:
		fprintf(stderr, "recv_timer3:Invalid MAC Type %x\n", subtype);
		exit(1);
	}
 done:
	pktRx_ = 0;
	rx_resume();
}


void
Mac802_11::recvRTS(Packet *p)
{
	struct rts_frame *rf = (struct rts_frame*)p->access(hdr_mac::offset_);

	if(tx_state_ != MAC_IDLE) {
		discard(p, DROP_MAC_BUSY);
		return;
	}

	/*
	 *  If I'm responding to someone else, discard this RTS.
	 */
	if(pktCTRL_) {
		discard(p, DROP_MAC_BUSY);
		return;
	}

	sendCTS(ETHER_ADDR(rf->rf_ta), rf->rf_duration);

	/*
	 *  Stop deferring - will be reset in tx_resume().
	 */
	if(mhDefer_.busy()) mhDefer_.stop();

	tx_resume();

	mac_log(p);
}

/*
 * txtime()	- pluck the precomputed tx time from the packet header
 */
double
Mac802_11::txtime(Packet *p)
{
	struct hdr_cmn *ch = HDR_CMN(p);
	double t = ch->txtime();
	if (t < 0.0) {
		drop(p, "XXX");
 		exit(1);
	}
	return t;
}

 
/*
 * txtime()	- calculate tx time for packet of size "psz" bytes 
 *		  at rate "drt" bps
 */
double
Mac802_11::txtime(double psz, double drt)
{
	double dsz = psz - phymib_.getPLCPhdrLen();
        int plcp_hdr = phymib_.getPLCPhdrLen() << 3;	
	int datalen = (int)dsz << 3;
	double t = (((double)plcp_hdr)/phymib_.getPLCPDataRate())
                                       + (((double)datalen)/drt);
	//debug("PLCP: %f DAT: %f\n", ((double)plcp_hdr)/phymib_.getPLCPDataRate(), ((double)datalen)/drt);
	return(t);
}



void
Mac802_11::recvCTS(Packet *p)
{
	if(tx_state_ != MAC_RTS) {
		discard(p, DROP_MAC_INVALID_STATE);
		return;
	}

	assert(pktRTS_);
	Packet::free(pktRTS_); pktRTS_ = 0;

	assert(pktTx_);	
	mhSend_.stop();

	/*
	 * The successful reception of this CTS packet implies
	 * that our RTS was successful. 
	 * According to the IEEE spec 9.2.5.3, you must 
	 * reset the ssrc_, but not the congestion window.
	 */
	ssrc_ = 0;
	tx_resume();

	mac_log(p);
}

void
Mac802_11::recvDATA(Packet *p)
{
	struct hdr_mac802_11 *dh = HDR_MAC802_11(p);
	u_int32_t dst, src, size;
	struct hdr_cmn *ch = HDR_CMN(p);

	dst = ETHER_ADDR(dh->dh_ra);
	src = ETHER_ADDR(dh->dh_ta);
	size = ch->size();
	/*
	 * Adjust the MAC packet size - ie; strip
	 * off the mac header
	 */
	ch->size() -= phymib_.getHdrLen11();
	ch->num_forwards() += 1;

	/*
	 *  If we sent a CTS, clean up...
	 */
	if(dst != MAC_BROADCAST) {
		if(size >= macmib_.getRTSThreshold()) {
			if (tx_state_ == MAC_CTS) {
				assert(pktCTRL_);
				Packet::free(pktCTRL_); pktCTRL_ = 0;
				mhSend_.stop();
				/*
				 * Our CTS got through.
				 */
			} else {
				discard(p, DROP_MAC_BUSY);
				return;
			}
			sendACK(src);
			tx_resume();
		} else {
			/*
			 *  We did not send a CTS and there's no
			 *  room to buffer an ACK.
			 */
			if(pktCTRL_) {
				discard(p, DROP_MAC_BUSY);
				return;
			}
			sendACK(src);
			if(mhSend_.busy() == 0){
				/*NIST*/
				//case where we need to send an ack and a defer is already scheduled (beacon)
				if(mhDefer_.busy()) mhDefer_.stop();
				/*end NIST*/
				tx_resume();
			}
		}
	}
	
	//NIST: update the timer for the list of client if I am an AP
	if (bss_id() == addr()) {
		for ( Client *n=list_client_head_.lh_first; n ; n=n->next_entry()) {
			if((u_int32_t)n->id() == src) { 
				n->update_timer ();
				n->rxp_watch_.update(10*log10(pktRx_->txinfo_.RxPr*1e3));
				n->txprob_.update(dh->dh_txprob);
				n->framesize_.update(size);
				n->lastupdate_ = NOW;
				break;
			}
		}
		predict_bw_used();
	
	}	      
	//end NIST
	    
	/* ============================================================
	   Make/update an entry in our sequence number cache.
	   ============================================================ */

	/* Changed by Debojyoti Dutta. This upper loop of if{}else was 
	   suggested by Joerg Diederich <dieder@ibr.cs.tu-bs.de>. 
	   Changed on 19th Oct'2000 */

        if(dst != MAC_BROADCAST) {
                if (src < (u_int32_t) cache_node_count_) {
                        Host *h = &cache_[src];

                        if(h->seqno && h->seqno == dh->dh_scontrol) {
                                discard(p, DROP_MAC_DUPLICATE);
                                return;
                        }
                        h->seqno = dh->dh_scontrol;
                } else {
			static int count = 0;
			if (++count <= 10) {
				debug ("MAC_802_11: accessing MAC cache_ array out of range (src %u, dst %u, size %d)!\n", src, dst, cache_node_count_);
				if (count == 10)
					debug ("[suppressing additional MAC cache_ warnings]\n");
			};
		};
	}

	/*
	 *  Pass the packet up to the link-layer.
	 *  XXX - we could schedule an event to account
	 *  for this processing delay.
	 */
	
	/* in BSS mode, if a station receives a packet via
	 * the AP, and higher layers are interested in looking
	 * at the src address, we might need to put it at
	 * the right place - lest the higher layers end up
	 * believing the AP address to be the src addr! a quick
	 * grep didn't turn up any higher layers interested in
	 * the src addr though!
	 * anyway, here if I'm the AP and the destination
	 * address (in dh_3a) isn't me, then we have to fwd
	 * the packet; we pick the real destination and set
	 * set it up for the LL; we save the real src into
	 * the dh_3a field for the 'interested in the info'
	 * receiver; we finally push the packet towards the
	 * LL to be added back to my queue - accomplish this
	 * by reversing the direction!*/

	if ((bss_id() == addr()) 
	    && ((u_int32_t)ETHER_ADDR(dh->dh_ra)!= MAC_BROADCAST)
	    && ((u_int32_t)ETHER_ADDR(dh->dh_3a) != (u_int32_t)addr())) {
		struct hdr_cmn *ch = HDR_CMN(p);
		u_int32_t dst = ETHER_ADDR(dh->dh_3a);
		u_int32_t src = ETHER_ADDR(dh->dh_ta);
		/* if it is a broadcast pkt then send a copy up
		 * my stack also
		 */
		if (dst == MAC_BROADCAST) {
			uptarget_->recv(p->copy(), (Handler*) 0);
		}

		ch->next_hop() = dst;
		STORE4BYTE(&src, (dh->dh_3a));
		ch->addr_type() = NS_AF_ILINK;
		ch->direction() = hdr_cmn::DOWN;
	}

	uptarget_->recv(p, (Handler*) 0);
}


void
Mac802_11::recvACK(Packet *p)
{	
	//struct hdr_cmn *ch = HDR_CMN(p);

	if(tx_state_ != MAC_SEND) {
		discard(p, DROP_MAC_INVALID_STATE);
		return;
	}

	//NIST: add pktManagement check
	assert(pktTx_ || pktManagement_);

	mhSend_.stop();

	//NIST: find RTT
	rtt_watch_.update(NOW - rtt_watch_.current());
	// NIST: collect the number of re-tx and reset counter
	retx_watch_.update(retx_watch_.current());
	retx_watch_.set_current(0);
	debug("At %.9f Mac %d retx count (Retx recvACK) is %f \n",NOW, index_, retx_watch_.current());		
	
	txerror_watch_.update(0);
	txerror_watch_.set_current(NOW);
	debug ("At %.9f Mac %d txerror %f\n", NOW, index_, txerror_watch_.average());
	update_watch(&txloss_watch_, 0.0);
	//txloss_watch_.update(0);
	//debug ("At %.9f Mac %d txloss %f\n", NOW, index_, txloss_watch_.average());

	struct hdr_cmn *ch;
	if(pktTx_ == last_tx_)
		ch = HDR_CMN(pktTx_);
	else
		ch = HDR_CMN(pktManagement_);
	update_throughput(&txbw_watch_, (ch->txtime()+overhead_duration_)*dataRate_);
	//txbw_watch_.update((ch->txtime()+overhead_duration_)*dataRate_, NOW);
	//debug ("At %.9f Mac %d txbw %f\n", NOW, index_, txbw_watch_.average());
	txsucc_watch_.update((ch->txtime()+overhead_duration_)*dataRate_, NOW);
	debug ("At %.9f Mac %d txsucc %f\n", NOW, index_, txsucc_watch_.average());
	
	if (pktTx_==last_tx_) {
	/*
	 * The successful reception of this ACK packet implies
	 * that our DATA transmission was successful.  Hence,
	 * we can reset the Short/Long Retry Count and the CW.
	 *
	 * need to check the size of the packet we sent that's being
	 * ACK'd, not the size of the ACK packet.
	 */
	if((u_int32_t) HDR_CMN(pktTx_)->size() <= macmib_.getRTSThreshold())
		ssrc_ = 0;
	else
		slrc_ = 0;

	//NIST: update the timer for the list of client if I am an AP
	if (bss_id() == addr()) {
		//get the destination node from which I received the ACK
		u_int32_t dst = ETHER_ADDR(HDR_MAC802_11(pktTx_)->dh_ra);
		for ( Client *n=list_client_head_.lh_first; n ; n=n->next_entry()) {
			if((u_int32_t)n->id() == dst) { 
				n->update_timer ();
				n->rxp_watch_.update(10*log10(pktRx_->txinfo_.RxPr*1e3));
				break;
			}
		}
	}	      
	//end NIST

	//NIST: update delay stats for sent packets
	double l2delay = NOW-HDR_CMN(pktTx_)->l2timestamp();
	if(!prevDelay)
		prevDelay = new double(l2delay);
		
	update_watch(&delay_watch_, l2delay);
	//delay_watch_.update(l2delay);
	if((l2delay - *prevDelay) < 0)
		update_watch(&jitter_watch_, *prevDelay-l2delay);
	        //jitter_watch_.update(*prevDelay-l2delay);
	else
		update_watch(&jitter_watch_, l2delay-*prevDelay);
	        //jitter_watch_.update(l2delay-*prevDelay);
	*prevDelay = l2delay;
	//debug ("At %.9f Mac %d delay %f\n", NOW, index_, delay_watch_.average());
	//debug ("At %.9f Mac %d jitter %f\n", NOW, index_, jitter_watch_.average());
	    
	//NIST: collect transmit throughput stats
	update_throughput(&txtp_watch_, ch->size()*8);
	//txtp_watch_.update(ch->size()*8, NOW);
	//debug ("At %.9f Mac %d txtp %f\n", NOW, index_, txtp_watch_.average());
		
	rst_cw();
	Packet::free(pktTx_);
	pktTx_ = 0;	

	//NIST: add close bracket
	}	
	//NIST: add management packet processing
	else {
		assert (pktManagement_==last_tx_);

		//Set the right step if this we sent an Association Response
		if( HDR_MAC802_11(pktManagement_)->dh_fc.fc_subtype == MAC_Subtype_AssocRes){
			bool found =false;
			for ( Client *n=list_client_head_.lh_first; n ; n=n->next_entry()) {
				if(n->pkt() == pktManagement_) { 
					if (n->status() != CLIENT_ASSOCIATED) {
						//we send link up if it is the first time
						send_link_up (ETHER_ADDR(HDR_MAC802_11(pktManagement_)->dh_ra), addr(), -1);
					}
					n->status() = CLIENT_ASSOCIATED;
					//update timer
					n->update_timer ();
					n->rxp_watch_.update(10*log10(pktRx_->txinfo_.RxPr*1e3));
					found = true;
					break;
				}
			}
			assert (found);
		}

		//NIST: update the timer for the list of client if I am an AP
		if (bss_id() == addr()) {
			//get the destination node from which I received the ACK
			u_int32_t dst = ETHER_ADDR(HDR_MAC802_11(pktManagement_)->dh_ra);
			for ( Client *n=list_client_head_.lh_first; n ; n=n->next_entry()) {
				if((u_int32_t)n->id() == dst) { 
					n->update_timer ();
					n->rxp_watch_.update(10*log10(pktRx_->txinfo_.RxPr*1e3));
					break;
				}
			}
		}	      
		//end NIST



		/* Richard
		 * The successful reception of this ACK packet implies
		 * that our DATA transmission was successful.  Hence,
		 * we can reset the Short/Long Retry Count and the CW.
		 *
		 * need to check the size of the packet we sent that's being
		 * ACK'd, not the size of the ACK packet.
		 */
		if((u_int32_t) HDR_CMN(pktManagement_)->size() <= macmib_.getRTSThreshold())
			ssrc_ = 0;
		else
			slrc_ = 0;
       
		rst_cw();

		Packet::free(pktManagement_); 
		pktManagement_ = managementQueue_->deque();
	}

	/*
	 * Backoff before sending again.
	 */
	assert(mhBackoff_.busy() == 0);
	if (!pktBeacon_) {
		debug2 ("At %.9f Mac %d recvACK starts backoff\n", NOW, index_);
		mhBackoff_.start(cw_, is_idle());
	}
	else {
		debug2 ("At %.9f Mac %d recvACK, do not backoff because a beacon needs to be sent\n");
	}

	tx_resume();

	mac_log(p);

}

//NIST: add reception of assiciation request/response
void Mac802_11::recvAssocRequest(Packet *p)
{
	u_int32_t src; 
	Client *cl;
	hdr_mac802_11 *mh = HDR_MAC802_11(p);
	src = ETHER_ADDR(mh->dh_ta);

	assert (index_ == bss_id_); //I need to be an AP

	debug ("At %.9f Mac %d Recv ASSOCIATION REQUEST from %d \n", NOW, index_, src);

	/*
	 *  We did not send a CTS and there's no
	 *  room to buffer an ACK.
	 */
	if(pktCTRL_) {
		discard(p, DROP_MAC_BUSY);
		return;
	}
	sendACK(src);

	//stop deferring
	if (mhDefer_.busy()) mhDefer_.stop();

	if(mhSend_.busy() == 0) //check if currently sending
		tx_resume();

	for ( cl=list_client_head_.lh_first; cl ; cl=cl->next_entry()) {
		if((u_int32_t)cl->id() == src) {
			break;
		}
	}
	if (!cl) {
		debug ("New client, adding entry in client list\n");
		cl = new Client (src, client_lifetime_, this);
		//look up datarate 
		struct association_request_frame *assocReq = (struct association_request_frame*)p->access(hdr_mac::offset_);
		if (assocReq->supported_rate == 0x1) {
			cl->dataRate_ = 1000000; 
		} else if (assocReq->supported_rate == 0x2) {
                        cl->dataRate_ = 2000000;
                } else if (assocReq->supported_rate == 0x3) {
                        cl->dataRate_ = 5500000;
                } else if (assocReq->supported_rate == 0x4) {
                        cl->dataRate_ = 11000000;
                } else {
			assert (assocReq->supported_rate == 0);
			cl->dataRate_ = dataRate_;
		}
		//debug to remove
		debug ("Node datarate is %f\n", cl->dataRate_);
		sendAssocResponse(src, cl);
		cl->insert_entry (&list_client_head_);
	}
	else if(cl->status() == CLIENT_ASSOCIATED){
		debug("\tMN already associated with me but send a reply\n");    
		sendAssocResponse(src, cl);
	}
	else {
		debug("\tA response is already pending\n");
	}

	mac_log(p);
}

void Mac802_11::recvAssocResponse(Packet *p)
{
	u_int32_t src; 
	
	hdr_mac802_11 *mh = HDR_MAC802_11(p);
	src = ETHER_ADDR(mh->dh_ta);
	debug ("At %.9f Mac %d Recv ASSOCIATION RESPONSE from %d \n", NOW, index_, src);

	/*
	 *  We did not send a CTS and there's no
	 *  room to buffer an ACK.
	 */
	if(pktCTRL_) {
		discard(p, DROP_MAC_BUSY);
		return;
	}
	sendACK(src);
	if(mhSend_.busy() == 0) //check if currently sending
		tx_resume();

	if (src == (u_int32_t)bss_id_ && hdv_state_ == MAC_CONNECTED) {
		discard (p, DROP_MAC_DUPLICATE);
		debug ("\tReceived response while already associated (duplicate?)\n");
		return;
	}

	struct association_response_frame *assocResp = (struct association_response_frame*)p->access(hdr_mac::offset_);

	if(assocResp->status_) { //Association accepted
			bss_id_ = src;
			debug("\tOur new base station is : %i\n", src);
			hdv_state_ = MAC_CONNECTED;	
			if (handover_) {
				send_link_handover_complete (addr(), old_bss_id_, bss_id_);
			}
			handover_=0;
			send_link_up (addr(), src, old_bss_id_);
		}
	else { //Association Rejected
		debug("\tAssociation refused by the AP\n");
		/** Used in previous implementation
		if (old_bss_id_ != (int)IBSS_ID) {
			send_link_handoff_failure (addr(), old_bss_id_, bss_id_);
		}
		*/
		//clean up
		bss_id_ = IBSS_ID; //to avoid sending a link down
		link_disconnect (); 
		//do we have to put the line below
		//send_link_parameter_change(false);
	}
	mac_log(p);
}

void Mac802_11::recvBeacon(Packet *p)
{
	u_int32_t src; 
	hdr_mac802_11 *mh = HDR_MAC802_11(p);
	src = ETHER_ADDR(mh->dh_ta);
	debug ("At %.9f Mac %d Recv Beacon from %d \n", NOW, index_, src);

	//if I am a BS, discard message
	if (index_ == bss_id_) {
		debug ("\t Overlapping cells (BS=%d and BS=%d)\n", index_, src);
		return;
	}

	if (src == (u_int32_t)bss_id_) {
		//extract beacon interval
		unsigned char *body = p->accessdata()+8;
		unsigned int bint = 0;
		bint = (*body++)<<8;
		bint += (*body++);
		debug ("\tbeacon interval=%dms\n",bint);
		
		bss_timeout_ = max_beacon_loss_ * ((double)bint/1000);

		//printf ("\tbss_timeout=%f/%d\n",bss_timeout_,max_beacon_loss_);
		//reschedule 
		bssTimer_.resched (bss_timeout_);
	} else if (scan_status_.step != SCAN_NONE) {
		BSSDescription *n;
		for ( n=scan_status_.resp->bssDesc_head.lh_first; n ; n=n->next_entry()) {
			if((u_int32_t)n->bss_id() == src) {
				break;
			}
		}
		if( !n){
			//Add mih capability after added to beacon
			BSSDescription *new_info = new BSSDescription (src,0.1,scan_status_.req->pref_ch[scan_status_.scan_index],0);
			new_info->insert_entry (&scan_status_.resp->bssDesc_head);
		}
		scan_status_.recvd_response=true;
	}
	else {
		mac_log(p); //we need to do before notifying handover 
		//if Mobile node, then link detected for base station only
		if ( index_ != bss_id_ && (src != (u_int32_t)bss_id_) 
		     && hdv_state_ == MAC_DISCONNECTED) {
			//this is not my base station send trigger
			//printf ("at %f in %d, Link detected triggered PoA=%d\n", NOW, index_, src);
			channelIDAfterScan_[src] = channel_;
			if (mih_) {
#ifdef RETX_TRIGGER
				if((pktRx_->txinfo_.RxPr > (10*pr_limit_*RXThreshold_)) && ( retx_watch_.average() < (pr_limit_* RetxThreshold_)))
#else
				if(pktRx_->txinfo_.RxPr > (pr_limit_*RXThreshold_))
#endif
					send_link_detected (addr(), src, 1);
			}
			else {//if we don't have MIH we automatically connect
				link_connect (src);
			}
		}
		return;
	}

	mac_log(p);
}

/*
 * Handling function when BSSTimer expired
 */
void Mac802_11::bss_expired ()
{
	debug ("At %.9f Mac %d bss timer expired\n", NOW, index_);
	link_disconnect ();
}

/*
 * BSSTimer expiration 
 * @param unused
 */
void BSSTimer::expire(Event *)
{
	if (m_->bss_id_ != (int)(Mac802_11::IBSS_ID)) //we don't want to send trigger when not attached
		m_->bss_expired ();
}

/*
 * BWTimer expiration 
 * @param unused
 */
void BWTimer::expire(Event *)
{
	m_->bw_update ();
}

/*
 * Compute the bandwidth received since last time
 */
void Mac802_11::bw_update ()
{
	base_station_bw = 0;
	
	for ( BandwidthInfo *n=collector_bw_.lh_first; n ; n=n->next_entry()) {
		n->bw() = n->size_pkts() / bw_timeout_;
		//printf(" pk size %d \n", collector_bw.at(i)->size_pks);
		base_station_bw += n->bw();
		n->size_pkts() = 0;
		n->nb_pkts() = 0;
	}

	bwTimer_.resched (bw_timeout_);
	debug("At %.9f Mac %d bandwidth is %f \n",NOW, index_, base_station_bw);
}

/*
 * Predict the bandwidth used.
 */
void Mac802_11::predict_bw_used ()
{
	double prob_slot_idle=1, prob_oneormore, prob_collision, prob_success=0, prob_success_ap=0, success_duration=0;
	double ptx=0, duration, coll_duration, occupied_bw=0;

	txprob_watch_.update(calculate_txprob());

	// Calculate the probability of a slot being idle
	// Start by including the prob for associated MSs
	for(Client *m=list_client_head_.lh_first; m ; m=m->next_entry()) {
		if( (NOW-m->lastupdate_) <= 1)
			ptx = m->txprob_.average();
		else
			ptx = 0;
		prob_slot_idle *= (1-ptx);
	}

	// Finally include AP queues
	prob_slot_idle *= (1-txprob_watch_.average());
		
	// Calculate probability of having one or more tx in a slot
	prob_oneormore= 1-prob_slot_idle;

	// Calculate the probability of successfulTx for each queue and finding the successful tx duration.
	// Start with MS queues
	for(Client *m=list_client_head_.lh_first; m ; m=m->next_entry()) {
		if( (NOW-m->lastupdate_) <= 1)
			ptx = m->txprob_.average();
		else
			ptx = 0;
		m->txprob_success_ = ptx*prob_slot_idle/(1-ptx);
		prob_success += m->txprob_success_;
		//assumes everyone uses same datarate and DCF
		duration = txtime(m->framesize_.average(), dataRate_) + overhead_duration_;
		success_duration += m->txprob_success_*duration;
	}
	// Finally include AP queues
	prob_success_ap = txprob_watch_.average()*prob_slot_idle/(1-txprob_watch_.average());
	prob_success += prob_success_ap;
	
	duration = txtime(txframesize_watch_.average(), dataRate_) + overhead_duration_;
        //Collision duration
	coll_duration = txtime(collsize_watch_.average(), dataRate_) + coll_overhead_duration_;
	
	success_duration += prob_success_ap*duration;
	
	//Calculate probability of collision in a slot
	prob_collision = (prob_oneormore - prob_success);
		
	double denom = prob_collision*coll_duration + prob_slot_idle*phymib_.getSlotTime() + success_duration;
	
	debug("At %.9f Mac %d collision is %f\n",NOW, index_,(dataRate_/1000000)*prob_collision*coll_duration/denom);
	debug("At %.9f Mac %d success is %f\n",NOW, index_,(dataRate_/1000000)*success_duration/denom);
	debug("At %.9f Mac %d idle is %f\n",NOW, index_, (dataRate_/1000000)*prob_slot_idle*phymib_.getSlotTime()/denom);

	if(denom > 0)
		occupied_bw = ((success_duration + prob_collision*coll_duration)/denom)*dataRate_;
       
	occupiedbw_watch_.update(occupied_bw/1000000);
}

/*
 * Connect to the new PoA that is on the given channel
 * @param macPoA The Mac address of the new PoA
 * @param channel The channel where the AP is
 */
void Mac802_11::link_connect (int macPoA, int channel)
{
	Tcl& tcl = Tcl::instance();
	//change channel
	tcl.evalf ("%s set-freq %f", netif_->name(), calFreq(channel));
	//tcl.evalf ("%s getRXThresh", netif_->name());
	//RXThreshold_ = atof (tcl.result());
	RXThreshold_ = ((WirelessPhy*)netif_)->getRXThresh ();
	clear_tx_rx();
	//connect
	//link_connect (macPoA);
}


/*
 * Connect to a new PoA
 * Following a connect, the first packet that will
 * be received from the new PoA will trigger a 
 * link up event.
 * @param macPoA The MAC address of the new PoA
 */
void Mac802_11::link_connect (int macPoA)
{
	
	//if we were previously connected, disconnect
	if (bss_id_ != (int)IBSS_ID && macPoA != (int)bss_id_) {
		link_disconnect ();
		//we are doing a handoff
		handover_=1;
		send_link_handover_imminent (addr(), bss_id_, macPoA);
		//I know it may look strange to send proceeding now
		//but there is no delay between them.
		//send_link_handoff_proceeding (addr(), bss_id_, macPoA);
	}		 
	else {
		clear_tx_rx();
		old_bss_id_ = bss_id_;
	}

	printf("Link Connect to AP %d on Channel %d\n",macPoA, channelIDAfterScan_[macPoA]);
	Tcl& tcl = Tcl::instance();
	tcl.evalf ("%s set-freq %f", netif_->name(), calFreq(channelIDAfterScan_[macPoA]));
	RXThreshold_ = ((WirelessPhy*)netif_)->getRXThresh ();

	//stop scanning
	scan_status_.step = SCAN_NONE;
	assert (scan_status_.minTimer->status()!=TIMER_PENDING);
	if (scan_status_.maxTimer->status()==TIMER_PENDING) {
		scan_status_.maxTimer->cancel();
	}

	bss_id_ = macPoA;
	debug ("At %.9f Mac %d Trigger Association with AP %i on channel %d\n", NOW, index_, macPoA, channelIDAfterScan_[macPoA]);
	sendAssocRequest(macPoA);
}

/*
 * Clear timers and other variable
 */
void Mac802_11::clear_tx_rx ()
{
	nav_ = 0.0;
	tx_state_ = rx_state_ = MAC_IDLE;
	tx_active_ = 0;
	if (pktRTS_)
		Packet::free(pktRTS_);
	pktRTS_ = 0;
	if (pktCTRL_)
		Packet::free(pktCTRL_);
	pktCTRL_ = 0;

	debug2("At %.9f Mac %d everything is flushed\n", NOW, index_);
	if (pktRx_)
		Packet::free(pktRx_);
	pktRx_ = 0;
	if (pktTx_)
		Packet::free(pktTx_);
	pktTx_ = 0;
	if (pktManagement_)
		Packet::free(pktManagement_);
	pktManagement_ = 0;
	//flush management packets
	Packet *tmp = managementQueue_->deque();
	while (tmp) {
		Packet::free(tmp);
		tmp = managementQueue_->deque();
	}

	cw_ = phymib_.getCWMin();
	ssrc_ = slrc_ = 0;
	//clean up possible transmission
	if(mhRecv_.busy()) mhRecv_.stop();
	if(mhDefer_.busy()) mhDefer_.stop();
	if(mhSend_.busy()) mhSend_.stop();
	if(mhBackoff_.busy()) mhBackoff_.stop();
	if(mhIF_.busy()) mhIF_.stop();
	if(mhNav_.busy()) mhNav_.stop();
	
	//other timers in case they were not cleared
	if (scan_status_.maxTimer->status()==TIMER_PENDING) {
		scan_status_.maxTimer->cancel();
	}
	if (scan_status_.minTimer->status()==TIMER_PENDING) {
		scan_status_.minTimer->cancel();
	}
}


/*
 * Disconnect from the PoA
 */
void Mac802_11::link_disconnect (link_down_reason_t reason)
{
	debug ("At %.9f Mac %d received link_disconnect command\n", NOW, index_);

	//cancel timeout timer
	if (bssTimer_.status()==TIMER_PENDING) {	
		bssTimer_.cancel();
	}
	//set bssid to not connected value
	old_bss_id_ = bss_id_;
	bss_id_ = IBSS_ID;
	going_down_=false;
	hdv_state_= MAC_DISCONNECTED;

	clear_tx_rx();

	//if we are connected, send a link down event
	if (old_bss_id_ != (int)IBSS_ID) {
		if(first_lgd_ == true) {
			debug ("At %.9f Mac %d first-lgd rxp %f %e\n", NOW, index_, rxp_watch_.average(), pow(10,rxp_watch_.average()/10)/1e3);
			first_lgd_ = false;
		}
		send_link_down ( addr(), old_bss_id_,  reason );
		if (handover_) {
			//send_link_handoff_failure (addr(), old_bss_id_, bss_id_);
			handover_=0;
		}
	}

}

/*
 * Disconnect from the PoA
 */
void Mac802_11::link_disconnect ()
{
	debug ("At %.9f Mac %d received link_disconnect command\n", NOW, index_);

	//cancel timeout timer
	if (bssTimer_.status()==TIMER_PENDING) {	
		bssTimer_.cancel();
	}
	//set bssid to not connected value
	old_bss_id_ = bss_id_;
	bss_id_ = IBSS_ID;
	going_down_=false;
	hdv_state_= MAC_DISCONNECTED;

	clear_tx_rx();

	//if we are connected, send a link down event
	if (old_bss_id_ != (int)IBSS_ID) {
		if(first_lgd_ == true) {
			debug ("At %.9f Mac %d first-lgd rxp %f %e\n", NOW, index_, rxp_watch_.average(), pow(10,rxp_watch_.average()/10)/1e3);
			first_lgd_ = false;
		}
		send_link_down ( addr(), old_bss_id_, LD_RC_EXPLICIT_DISCONNECT );
		if (handover_) {
			//send_link_handoff_failure (addr(), old_bss_id_, bss_id_);
			handover_=0;
		}
	}

}


/* 
 * Configure/Request configuration
 * The upper layer sends a config object with the required 
 * new values for the parameters (or PARAMETER_UNKNOWN_VALUE).
 * The MAC tries to set the values and return the new setting.
 * For examples if a MAC does not support a parameter it will
 * return  PARAMETER_UNKNOWN_VALUE
 * @param config The configuration object
 */ 
link_get_param_s* Mac802_11::link_get_parameters (link_parameter_type_s* param)
{
	/*
	assert (config);
	//we only support datarate for now
	if (config->bandwidth != PARAMETER_UNKNOWN_VALUE) {
		if (dataRate_ != config->bandwidth) {
			//Mac::send_link_parameter_change (LINK_BANDWIDTH, dataRate_,config->bandwidth);
		}
		dataRate_ = config->bandwidth;
	}
	config->bandwidth = dataRate_;
	config->type = LINK_802_11;
	//we set the rest to PARAMETER_UNKNOWN_VALUE
	config->ber = PARAMETER_UNKNOWN_VALUE;
	config->delay = PARAMETER_UNKNOWN_VALUE;
	config->macPoA = PARAMETER_UNKNOWN_VALUE;
	*/
	return NULL;
}

/* 
 * Configure the threshold values for the given parameters
 * @param numLinkParameter number of parameter configured
 * @param linkThresholds list of parameters and thresholds
 */
struct link_param_th_status * Mac802_11::link_configure_thresholds (int numLinkParameter, struct link_param_th *linkThresholds)
{
	struct link_param_th_status *result = (struct link_param_th_status *) malloc(numLinkParameter * sizeof (struct link_param_th_status));
	StatWatch *watch=NULL;
	for (int i=0 ; i < numLinkParameter ; i++) {
		result[i].parameter = linkThresholds[i].parameter;
		result[i].status = 1; //accepted..default
		if (linkThresholds[i].parameter.link_type != LINK_GENERIC && linkThresholds[i].parameter.link_type != LINK_802_11) {
			//not a valid link type
			result[i].status = 0;
			continue;
		}
		switch (linkThresholds[i].parameter.parameter_type){
		case LINK_GEN_FRAME_LOSS: 
			watch = &txloss_watch_;
			break;
		case LINK_GEN_PACKET_DELAY:
			watch = &delay_watch_;
			break;
		case LINK_GEN_PACKET_JITTER:
			watch = &jitter_watch_;
			break;
		case LINK_GEN_RX_DATA_THROUGHPUT:
			watch = &rxtp_watch_;
			break;
		case LINK_GEN_RX_TRAFFIC_THROUGHPUT:
			watch = &rxbw_watch_;
			break;
		case LINK_GEN_TX_DATA_THROUGHPUT:
			watch = &txtp_watch_;
			break;
		case LINK_GEN_TX_TRAFFIC_THROUGHPUT:
			watch = &txbw_watch_;
			break;
		default:
			fprintf (stderr, "Parameter type not supported %d\n", linkThresholds[i].parameter.parameter_type);
			result[i].status = 0; //rejected
			continue;
		}
		watch->set_thresholds (linkThresholds[i].initActionTh.data_d, 
				       linkThresholds[i].rollbackActionTh.data_d ,
				       linkThresholds[i].exectActionTh.data_d);
	}
	return result;
}
 
/**
 * Update the given timer and check if thresholds are crossed
 * @param watch the stat watch to update
 * @param value the stat value
 */
void Mac802_11::update_watch (StatWatch *watch, double value)
{
	threshold_action_t action = watch->update (value);
	link_parameter_type_s param;
	union param_value old_value, new_value;
	char *name;
	if (action != NO_ACTION_TH) {
		if (watch == &txloss_watch_) {
			param.link_type = LINK_GENERIC;
			param.parameter_type = LINK_GEN_FRAME_LOSS;
		} else if (watch == &delay_watch_) {
			param.link_type = LINK_GENERIC;
			param.parameter_type = LINK_GEN_PACKET_DELAY;
		} else if (watch == &jitter_watch_) {
			param.link_type = LINK_GENERIC;				
			param.parameter_type = LINK_GEN_PACKET_JITTER;
		}
		old_value.data_d = watch->old_average();
		new_value.data_d = watch->average();
		send_link_parameters_report (addr(), bss_id_, param, old_value, new_value);      
	}
	if (watch == &txloss_watch_) {
		name = "txloss";
	} else if (watch == &delay_watch_) {
		name = "delay";
	} else if (watch == &jitter_watch_) {
		name = "jitter";
	} else {
		name = "other";
	}
	if (print_stats_)
		printf ("At %f in Mac %d, updating stats %s: %f\n", NOW, addr(), name, watch->average());
}

/**
 * Update the given timer and check if thresholds are crossed
 * @param watch the stat watch to update
 * @param value the stat value
 */
void Mac802_11::update_throughput (ThroughputWatch *watch, double size)
{
	threshold_action_t action = watch->update (size, NOW);
	link_parameter_type_s param;
	union param_value old_value, new_value;
	char *name;
	if (action != NO_ACTION_TH) {
		if (watch == &rxtp_watch_) {
			param.link_type = LINK_GENERIC;
			param.parameter_type = LINK_GEN_RX_DATA_THROUGHPUT;
		} else if (watch == &rxbw_watch_) {
			param.link_type = LINK_GENERIC;			
			param.parameter_type = LINK_GEN_RX_TRAFFIC_THROUGHPUT;
		} else if (watch == &txtp_watch_) {
			param.link_type = LINK_GENERIC;
			param.parameter_type = LINK_GEN_TX_DATA_THROUGHPUT;
		} else if (watch == &txbw_watch_) {
			param.link_type = LINK_GENERIC;
			param.parameter_type = LINK_GEN_TX_TRAFFIC_THROUGHPUT;
		}
		old_value.data_d = watch->old_average();
		new_value.data_d = watch->average();
		send_link_parameters_report (addr(), bss_id_, param, old_value, new_value);      
	}
	
	if (watch == &rxtp_watch_) {
		name = "rxtp";
	} else if (watch == &rxbw_watch_) {
		name = "rxbw";
	} else if (watch == &txtp_watch_) {
		name = "txtp";
	} else if (watch == &txbw_watch_) {
		name = "txbw";
	}
	if (print_stats_)
		printf ("At %f in Mac %d, updating stats %s: %f\n", NOW, addr(), name, watch->average());
}

/*
 * Prepare and queue a beacon to send
 */
void Mac802_11::sendBeacon ()
{
        debug("At %.9f Mac %d sends a Beacon\n",NOW, index_);

        Packet *p = Packet::alloc();
	hdr_cmn* ch = HDR_CMN(p);
        struct beacon_frame *bf = (struct beacon_frame*)p->access(hdr_mac::offset_);
        unsigned char *body;

	//NIST: predict the bandwidth used
	//predict_bw_used();

        assert(pktBeacon_ == 0);

	//we need to allocate space for information
	p->allocdata(BEACON_FRAME_BODY_SIZE);
	body = p->accessdata();

	//write timestamp (us)
	//unsigned int tstamp = (unsigned int) (NOW*1000000);
	/*
	(*body++) = (tstamp>>56) & 0xFF;
	(*body++) = (tstamp>>48) & 0xFF;
	(*body++) = (tstamp>>40) & 0xFF;
	(*body++) = (tstamp>>32) & 0xFF;
	(*body++) = (tstamp>>24) & 0xFF;
	(*body++) = (tstamp>>16) & 0xFF;
	(*body++) = (tstamp>>8) & 0xFF;
	(*body++) = tstamp & 0xFF;
	*/
	body+=8; //we skip timestamp and go to beacon interval
	//write beacon interval
	unsigned int bint = (unsigned int) (beacon_interval_ * 1000);
	(*body++) = (bint>>8) & 0xFF;
	(*body++) = bint & 0xFF;


        ch->uid() = 0;
        ch->ptype() = PT_MAC;
        ch->size() = phymib_.getBeaconlen();
        ch->iface() = -2;
        ch->error() = 0;

        //bzero(assocReq, MAC_HDR_LEN);

        hdr_src((char*)bf, addr());

        //Frame control fields
        bf->bh_fc.fc_protocol_version = MAC_ProtocolVersion;
        bf->bh_fc.fc_type        = MAC_Type_Management;
        bf->bh_fc.fc_subtype     = MAC_Subtype_Beacon2;
        bf->bh_fc.fc_to_ds       = 0;
        bf->bh_fc.fc_from_ds     = 0;
        bf->bh_fc.fc_more_frag   = 0;
        bf->bh_fc.fc_retry       = 0;
        bf->bh_fc.fc_pwr_mgt     = 0;
        bf->bh_fc.fc_more_data   = 0;
        bf->bh_fc.fc_wep = 0;
        bf->bh_fc.fc_order       = 0;

        hdr_dst((char*)bf, MAC_BROADCAST);

        /* store data tx time for broadcast packets (see 9.6) */
	ch->txtime() = txtime(ch->size(), basicRate_);

	//bf->bh_duration = 0;
	bf->bh_duration = usec(txtime(phymib_.getBeaconlen(), basicRate_)
			       + phymib_.getSIFS());

	beaconTimer_.resched (beacon_interval_);
        pktBeacon_ = p;

	/* Since beacon have higher priority than data or management, we stop defering
	 * Stop deferring - will be reset in tx_resume().
	 */
	if (mhBackoff_.busy()) {
		//we need to stop the timer for any RTS/DATA packet since
		//beacon has higher priority
		//printf ("We stop backoff\n");
		mhBackoff_.stop();
	}
	

	if (pktCTRL_) {
		//printf ("There is a packet control..don't do anything\n");
		return; //we let the pktCTRL being sent first (ACK/CTS)
	}

	/* If we already have a defer, then we don't */
	if(mhDefer_.busy()) {
		//printf ("Stop defer\n");
		mhDefer_.stop();
		tx_resume();
	}
	

	//if we are sending, then we have to wait
	else if(mhSend_.busy() == 0) {
		//printf ("Currently sending don't don anything\n");
		tx_resume(); //Otherwise the event is not schedule
	}
}

/*
 * BeaconTimer expiration
 * @param unused
 */
void BeaconTimer::expire(Event *)
{
	m_->sendBeacon();
}

/*
 * Execute a scan
 * @param r The request 
 */
void Mac802_11::link_scan (void *r)
{
	Tcl& tcl = Tcl::instance();

	debug("At %.9f Mac %d received link_scan command\n",NOW, index_);

	assert (r);
	Mac802_11_scan_request_s *req = (Mac802_11_scan_request_s *)r;

	if (scan_status_.step!=SCAN_NONE)
		return; //only one scan at a time

	scan_status_.req = req;
	//we start the first channel
	scan_status_.scan_index=0;
	scan_status_.step = SCAN_WAITING;
	scan_status_.resp = (Mac802_11_scan_response_s*) malloc (sizeof (Mac802_11_scan_response_s));
	LIST_INIT(&scan_status_.resp->bssDesc_head);
	//tcl.evalf ("%s get-channel",netif_->name());
	scan_status_.orig_ch = channel_;

	scan_status_.recvd_response=false;

	//switch channel at phy layer, and stop all emission/reception
	if (scan_status_.orig_ch != scan_status_.req->pref_ch[scan_status_.scan_index]) {
		tcl.evalf ("%s set-freq %f", netif_->name(), calFreq(scan_status_.req->pref_ch[scan_status_.scan_index]));
		//tcl.evalf ("%s getRXThresh", netif_->name());
		//RXThreshold_ = atof (tcl.result());
		RXThreshold_ = ((WirelessPhy*)netif_)->getRXThresh ();
		clear_tx_rx();
	}
	
	scan_status_.minTimer->resched (scan_status_.req->minChannelTime);
	//if (!scan_status_.recvd_response) {
	scan_status_.maxTimer->resched (scan_status_.req->maxChannelTime);
	//}

	//create packet to send
	probeTimer_.resched (scan_status_.req->probeDelay);
	//sendProbeRequest ();

	hdv_state_=MAC_DISCONNECTED;
	old_bss_id_ = bss_id_;
	bss_id_ = IBSS_ID;

	//cancel timeout timer
	if (bssTimer_.status()==TIMER_PENDING) {	
		bssTimer_.cancel();
	}
}

/*
 * Prepare and queue a beacon to send
 */
void Mac802_11::sendProbeRequest ()
{
        debug("At %.9f Mac %d sends a Probe request\n",NOW, index_);

        Packet *p = Packet::alloc();
	hdr_cmn* ch = HDR_CMN(p);
        struct probe_request_frame *pf = (struct probe_request_frame *)p->access(hdr_mac::offset_);

	//if I send a probe request, it is after switching channel
	//it must be the first packet to be sent.
        //assert(pktManagement_ == 0);

	if (pktManagement_) {
		//we were not able to send the previous probe request
		//to do: check that previous is effectively a probe request
		//we wait for the current packet to be sent
		//printf ("there is another packet in queue\n");
		return;
	}

        ch->uid() = 0;
        ch->ptype() = PT_MAC;
        ch->size() = phymib_.getProbeReqlen();
        ch->iface() = -2;
        ch->error() = 0;

        //bzero(assocReq, MAC_HDR_LEN);

        hdr_src((char*)pf, addr());

        //Frame control fields
        pf->prf_fc.fc_protocol_version = MAC_ProtocolVersion;
        pf->prf_fc.fc_type        = MAC_Type_Management;
        pf->prf_fc.fc_subtype     = MAC_Subtype_ProbeReq;
        pf->prf_fc.fc_to_ds       = 0;
        pf->prf_fc.fc_from_ds     = 0;
        pf->prf_fc.fc_more_frag   = 0;
        pf->prf_fc.fc_retry       = 0;
        pf->prf_fc.fc_pwr_mgt     = 0;
        pf->prf_fc.fc_more_data   = 0;
        pf->prf_fc.fc_wep = 0;
        pf->prf_fc.fc_order       = 0;

        hdr_dst((char*)pf, MAC_BROADCAST);
	
        /* store data tx time for broadcast packets (see 9.6) */
	ch->txtime() = txtime(ch->size(), basicRate_);

	//pf->prf_duration = 0;
	pf->prf_duration = usec(txtime(phymib_.getBeaconlen(), basicRate_)
			       + phymib_.getSIFS());

        pktManagement_ = p;

	/*
	 *  If the medium is IDLE, we must wait for a DIFS
	 *  Space before transmitting.
	 */
	if(mhBackoff_.busy() == 0) {
		if(is_idle()) {
			if (mhDefer_.busy() == 0) {
				/*
				 * If we are already deferring, there is no
				 * need to reset the Defer timer.
				 */
				mhDefer_.start(phymib_.getDIFS());
			}
		} else {
			/*
			 * If the medium is NOT IDLE, then we start
			 * the backoff timer.
			 */
			mhBackoff_.start(cw_, is_idle());
		}
	}

}

/*
 * Prepare and queue a probe response to send
 */
void Mac802_11::sendProbeResponse (int dst)
{
	debug("At %.9f Mac %d sends a Probe Response to %d\n",NOW, index_, dst);

        Packet *p = Packet::alloc();
	hdr_cmn* ch = HDR_CMN(p);
        struct probe_response_frame *pf = (struct probe_response_frame*)p->access(hdr_mac::offset_);

        //assert(pktManagement_ == 0);

        ch->uid() = 0;
        ch->ptype() = PT_MAC;
        ch->size() = phymib_.getProbeReqlen();
        ch->iface() = -2;
        ch->error() = 0;

        //bzero(assocReq, MAC_HDR_LEN);

        hdr_src((char*)pf, addr());

        //Frame control fields
        pf->prf_fc.fc_protocol_version = MAC_ProtocolVersion;
        pf->prf_fc.fc_type        = MAC_Type_Management;
        pf->prf_fc.fc_subtype     = MAC_Subtype_ProbeRes;
        pf->prf_fc.fc_to_ds       = 0;
        pf->prf_fc.fc_from_ds     = 0;
        pf->prf_fc.fc_more_frag   = 0;
        pf->prf_fc.fc_retry       = 0;
        pf->prf_fc.fc_pwr_mgt     = 0;
        pf->prf_fc.fc_more_data   = 0;
        pf->prf_fc.fc_wep = 0;
        pf->prf_fc.fc_order       = 0;

        hdr_dst((char*)pf, dst);

        /* store data tx time for broadcast packets (see 9.6) */
	ch->txtime() = txtime(ch->size(), basicRate_);

	//pf->prf_duration = 0;
	pf->prf_duration = usec(txtime(phymib_.getBeaconlen(), basicRate_)
			       + phymib_.getSIFS());

	//debug ("in send probe response, pktManagement_=0x%x, ctrl=0x%x, data=0x%x beacon=0x%x\n", pktManagement_, pktCTRL_, pktTx_, pktBeacon_);
	if (pktManagement_) {
		managementQueue_->enque(p);
	} else {
		pktManagement_ = p;
	}
}

//NIST: add reception of assiciation request/response
void Mac802_11::recvProbeRequest(Packet *p)
{
	u_int32_t src; 
	hdr_mac802_11 *mh = HDR_MAC802_11(p);
	src = ETHER_ADDR(mh->dh_ta);
	
	if (index_ != bss_id_) {
		Packet::free(p); 
		return; //no AP
	}

	debug ("At %.9f Mac %d Recv PROBE REQUEST from %d/%d \n", NOW, index_, src, mhSend_.busy() == 0);

	sendProbeResponse(src);

	//stop deferring
	if (mhDefer_.busy()) mhDefer_.stop();

	if(mhSend_.busy() == 0) //check if currently sending
		tx_resume();
	
	Packet::free(p); 
}

//NIST: add reception of assiciation request/response
void Mac802_11::recvProbeResponse(Packet *p)
{
	u_int32_t src; 
	hdr_mac802_11 *mh = HDR_MAC802_11(p);
	src = ETHER_ADDR(mh->dh_ta);
	debug ("At %.9f Mac %d Recv PROBE RESPONSE from %d \n", NOW, index_, src);

	scan_status_.recvd_response=true;

	/*
	 *  We did not send a CTS and there's no
	 *  room to buffer an ACK.
	 */
	if(pktCTRL_) {
		discard(p, DROP_MAC_BUSY);
		return;
	}
	sendACK(src);
	if(mhSend_.busy() == 0) //check if currently sending
		tx_resume();
	
	if (scan_status_.step == SCAN_NONE) {
		//I sent ACK to confirm packet but I do not need to process it
		Packet::free(p); 
		return;
	}

	assert (scan_status_.resp);

	//if (src != bss_id_)
	//	send_link_detected (addr(), src,1);
	//add information
	BSSDescription *n;
	for ( n=scan_status_.resp->bssDesc_head.lh_first; n ; n=n->next_entry()) {
		if((u_int32_t)n->bss_id() == src) {
			break;
		}
	}
	if( !n){
		//add mih capability after added to probe
		BSSDescription *new_info = new BSSDescription (src,0.1,scan_status_.req->pref_ch[scan_status_.scan_index],0);
		new_info->insert_entry (&scan_status_.resp->bssDesc_head);
	}

	Packet::free(p); 
}


/*
 * Scan timed out
 */
void Mac802_11::nextScan ()
{	
	Tcl& tcl = Tcl::instance();
	//check if we have more channel to scan
	if (scan_status_.req->action == STOP_FIRST && scan_status_.recvd_response){
		debug ("At %.9f Mac %d scanning is over case 1\n", NOW, index_);
		//no need to clear rx/tx since we stay on the same channel
		scan_status_.resp->result = SCAN_SUCCESS;
		if (scan_status_.maxTimer->status()==TIMER_PENDING) {
			scan_status_.maxTimer->cancel();
		}
		if (scan_status_.minTimer->status()==TIMER_PENDING) {
			scan_status_.minTimer->cancel();
		}
		//inform mih
		send_scan_result (scan_status_.resp, sizeof (struct Mac802_11_scan_response_s));
		free (scan_status_.req);
	}
	else if (++scan_status_.scan_index < scan_status_.req->nb_channels) {
		debug ("At %.9f Mac %d scanning channel %d\n", NOW, index_,scan_status_.req->pref_ch[scan_status_.scan_index]);
		//scan_status_.scan_index++;
		if (scan_status_.maxTimer->status() == TIMER_PENDING) {
			scan_status_.maxTimer->cancel();
		}
		/*switch channel*/
		tcl.evalf ("%s set-freq %f", netif_->name(), calFreq(scan_status_.req->pref_ch[scan_status_.scan_index]));
		//tcl.evalf ("%s getRXThresh",netif_->name());
		//RXThreshold_ = atof (tcl.result());
		RXThreshold_ = ((WirelessPhy*)netif_)->getRXThresh ();

		clear_tx_rx();

		scan_status_.minTimer->resched (scan_status_.req->minChannelTime);
		//if (!scan_status_.recvd_response) {
		scan_status_.maxTimer->resched (scan_status_.req->maxChannelTime);
		//}
		
		probeTimer_.resched (scan_status_.req->probeDelay);
		//sendProbeRequest ();
	}
	else {
		debug ("At %.9f Mac %d scanning is over\n", NOW, index_);
		//go back to original channel
		tcl.evalf ("%s set-freq %f", netif_->name(), calFreq(scan_status_.orig_ch));
		//RXThreshold_ = atof (tcl.result());
		RXThreshold_ = ((WirelessPhy*)netif_)->getRXThresh ();
	
		scan_status_.step = SCAN_NONE;
		bss_id_ = old_bss_id_;
		scan_status_.resp->result = SCAN_SUCCESS;
		if (scan_status_.maxTimer->status()==TIMER_PENDING) {
			scan_status_.maxTimer->cancel();
		}
		if (scan_status_.minTimer->status()==TIMER_PENDING) {
			scan_status_.minTimer->cancel();
		}		

		clear_tx_rx();
		hdv_state_ = MAC_CONNECTED;
		bssTimer_.resched (bss_timeout_); //restart timer

		//store structure scan bssid/channel Vincent Gauthier
		BSSDescription *n;
		for ( n=(scan_status_.resp->bssDesc_head).lh_first; n ; n=n->next_entry()) {
			//store
			channelIDAfterScan_[n->bss_id()] = n->channel();
			debug("At %.9f Mac %d Scan Detect AP %d on channel %d\n", NOW, index_, n->bss_id(),channelIDAfterScan_[n->bss_id()]);
		}


		//inform mih
		send_scan_result (scan_status_.resp, sizeof (struct Mac802_11_scan_response_s));
		
		scan_status_.resp = NULL;
		free (scan_status_.req);
		scan_status_.req = NULL;
	}
	scan_status_.recvd_response=false;
}

void Mac802_11::start_autoscan ()
{
	debug ("At %.9f Mac %d start autoscan\n", NOW, index_);
	//create a scan request
	Mac802_11_scan_request_s *req = (Mac802_11_scan_request_s*) malloc (sizeof(Mac802_11_scan_request_s));
	req->scanType = SCAN_TYPE_ACTIVE;
	req->action   = STOP_FIRST;
	req->probeDelay = 5e-6;
	req->nb_channels = 11;
	int *tmp = (int*) malloc (11*sizeof (int));
	for (int i=0 ; i <11 ; i++) {
		tmp[i] = i+1;
	}
	req->pref_ch = tmp;
	req->minChannelTime = minChannelTime_;
	req->maxChannelTime = maxChannelTime_;
	req->probeDelay = 0.002;

	link_scan (req);
}

/*
 * Remove the information about the client and send trigger
 * @param addr The client id
 */
void Mac802_11::process_client_timeout (int addr)
{
	Client *cl;

	for ( cl=list_client_head_.lh_first; cl ; cl=cl->next_entry()) {
		if(cl->id() == addr) {
			send_link_down (addr, index_, LD_RC_PACKET_TIMEOUT);			
			cl->remove_entry();
			break;
		}
	}
}

/*
 * Return the datarate for the given packet
 * This function helps to support multiple datarate
 * @param p The packet to send
 * @return the datarate to use
 */
double Mac802_11::getTxDatarate (Packet *p)
{
	
	if (index_ == bss_id_ && ((u_int32_t) ETHER_ADDR(HDR_MAC802_11(p)->dh_ra) != MAC_BROADCAST )) {
		//This is an AP and not a broadcast, use client information
		for ( Client *n=list_client_head_.lh_first; n ; n=n->next_entry()) {
			if((u_int32_t) n->id() == (u_int32_t) ETHER_ADDR(HDR_MAC802_11(p)->dh_ra)){
				//printf ("AP sending packet with datarate %f\n", n->dataRate_);
				return n->dataRate_;				
			}
		}
	}
	//This is an MN or broadcast, use dataRate_ variable
	return dataRate_;
}


/*
 * ProbeDelayTimer expiration
 * @param unused
 */
void ProbeDelayTimer::expire(Event *)
{
	m_->debug ("At %.9f Mac %d Probe Delay expired\n", NOW, m_->index_);

	//go to next channel
	m_->sendProbeRequest ();
}

/*
 * MinScanTimer expiration
 * @param unused
 */
void MinScanTimer::expire(Event *)
{
	m_->debug ("At %.9f Mac %d MinScanTimer expired\n", NOW, m_->index_);

	if (!(m_->scan_status_.recvd_response)) {
		//go to next channel
		m_->nextScan();
	}
	else {
		//we will have to wait max, but resend probe
		m_->scan_status_.minTimer->resched(m_->scan_status_.req->minChannelTime);
		m_->sendProbeRequest ();
	}
}

/*
 * MaxScanTimer expiration
 * @param unused
 */
void MaxScanTimer::expire(Event *)
{
	m_->debug ("At %.9f Mac %d MaxScanTimer expired\n", NOW, m_->index_);

	//go to next channel
	m_->nextScan ();
}

/* Client timer expiration
 * @param unused
 */
void ClientTimer::expire(Event *)
{
	m_->debug ("At %.9f Mac %d ClientTimer expired for client %d\n", NOW, m_->index_, addr_);

	m_->process_client_timeout (addr_);
}
