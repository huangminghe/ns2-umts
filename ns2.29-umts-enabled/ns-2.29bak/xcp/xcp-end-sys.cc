// -*-  Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*-

/*
 * Copyright (C) 2004 by the University of Southern California
 * Copyright (C) 2004 by USC/ISI
 *               2002 by Dina Katabi
 * $Id: xcp-end-sys.cc,v 1.1.1.1 2006/03/08 13:52:49 rouil Exp $
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 * The copyright of this module includes the following
 * linking-with-specific-other-licenses addition:
 *
 * In addition, as a special exception, the copyright holders of
 * this module give you permission to combine (via static or
 * dynamic linking) this module with free software programs or
 * libraries that are released under the GNU LGPL and with code
 * included in the standard release of ns-2 under the Apache 2.0
 * license or under otherwise-compatible licenses with advertising
 * requirements (or modified versions of such code, with unchanged
 * license).  You may copy and distribute such a system following the
 * terms of the GNU GPL for this module and the licenses of the
 * other code concerned, provided that you include the source code of
 * that other code when and as the GNU GPL requires distribution of
 * source code.
 *
 * Note that people who make modified versions of this module
 * are not obligated to grant this special exception for their
 * modified versions; it is their choice whether to do so.  The GNU
 * General Public License gives permission to release a modified
 * version without this exception; this exception also makes it
 * possible to release a modified version which carries forward this
 * exception.
 *
 */

#ifndef lint
static const char rcsid[] =
"@(#) $Header: /var/lib/cvs/ns-2.29/xcp/xcp-end-sys.cc,v 1.1.1.1 2006/03/08 13:52:49 rouil Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "ip.h"
#include "tcp.h"
#include "flags.h"

#include "agent.h"
#include "packet.h"

#include "flags.h"
#include "tcp-sink.h"
#include "xcp-end-sys.h"


#define TRACE 0 // when 0, we don't print any debugging info.

void cwndShrinkingTimer::expire(Event*)
{
	a_->timeout(TCP_TIMER_DELSND);
}

int hdr_xcp::offset_;
static unsigned int next_xcp = 0;

static class XCPHeaderClass : public PacketHeaderClass {
public:
        XCPHeaderClass() : PacketHeaderClass("PacketHeader/XCP",
					     sizeof(hdr_xcp)) {
		bind_offset(&hdr_xcp::offset_);
	}
} class_xcphdr;

static class XcpRenoTcpClass : public TclClass {
public:
	XcpRenoTcpClass() : TclClass("Agent/TCP/Reno/XCP") {}
	TclObject* create(int, const char*const*) {
		return (new XcpAgent());
	}
} class_xcp;

static class XcpSinkClass : public TclClass {
public:
	XcpSinkClass() : TclClass("Agent/XCPSink") {}
	TclObject* create(int, const char*const*) {
		return (new XcpSink(new Acker));
	}
} class_xcpsink;

XcpAgent::XcpAgent(): RenoTcpAgent(), shrink_cwnd_timer_(this)
{
	tcpId_   = next_xcp;
	next_xcp++;
	init_rtt_vars();
	current_positive_feedback_ = 0.0;
	xcp_srtt_ = 0;
	type_ = PT_XCP;

	xcp_sparse_seqno_ = -1;
}

void
XcpAgent::delay_bind_init_all()
{
        delay_bind_init_one("xcp_sparse_");

	RenoTcpAgent::delay_bind_init_all();
}

int
XcpAgent::delay_bind_dispatch(const char *varName, 
			      const char *localName, 
			      TclObject *tracer)
{
        if (delay_bind_bool(varName, localName, 
			    "xcp_sparse_", &xcp_sparse_, 
			    tracer))
		return TCL_OK;
	
        return RenoTcpAgent::delay_bind_dispatch(varName, localName, tracer);
}
		

// standard tcp output except that it fills in the XCP header
void XcpAgent::output(int seqno, int reason)
{
	int force_set_rtx_timer = 0;
	Packet* p = allocpkt();
	hdr_tcp *tcph = hdr_tcp::access(p);
	hdr_flags* hf = hdr_flags::access(p);
	tcph->seqno() = seqno;
	tcph->ts() = Scheduler::instance().clock();
	tcph->ts_echo() = ts_peer_;
	tcph->reason() = reason;
	if (ecn_) {
		hf->ect() = 1;	// ECN-capable transport
	}
	if (cong_action_) {
		hf->cong_action() = TRUE;  // Congestion action.
		cong_action_ = FALSE;
        }

	// Beginning of XCP Changes
	hdr_xcp *xh = hdr_xcp::access(p);

	if ( (xh->xcp_sparse_ = xcp_sparse_) ) {
		if (xcp_sparse_seqno_ < 0)
			xcp_sparse_seqno_ = tcph->seqno();
		else {
			xh->xcp_enabled_ = hdr_xcp::XCP_ACK; 
			xh->delta_throughput_ = 0;
			//XXX hack needed so that other XCP-controlled
			//packets would get into the XCP queue and not
			//TCP queue.  Alternatively, could set rtt_ to
			//0 to stop XCP processing.

			goto xcp_sparse_skip;
		}
	}

	xh->xcp_enabled_ = hdr_xcp::XCP_ENABLED;
	xh->cwnd_ = double(cwnd_);
	xh->rtt_ = srtt_estimate_;
	xh->xcpId_ = tcpId_;
	
#define MAX_THROUGHPUT        1e24
	if (srtt_estimate_ != 0) {
		xh->throughput_ = window() * size_ / srtt_estimate_;
		xh->delta_throughput_ = (MAX_THROUGHPUT 
					 - xh->throughput_);
	} else {
		//XXX can do xh->xcp_enabled_ = hdr_xcp::XCP_DISABLED;
		xh->throughput_ = .1; //XXX
		xh->delta_throughput_ = 0;
	}

	if(channel_) {
		trace_var("throughput", xh->throughput_);
	}
 xcp_sparse_skip:
	// End of XCP Changes

	/* Check if this is the initial SYN packet. */
	if (seqno == 0) {
		if (syn_) {
			hdr_cmn::access(p)->size() = tcpip_base_hdr_size_ + XCP_HDR_LEN;
		}
		if (ecn_) {
			hf->ecnecho() = 1;
//			hf->cong_action() = 1;
			hf->ect() = 0;
		}
	}
        int bytes = hdr_cmn::access(p)->size();

	/* if no outstanding data, be sure to set rtx timer again */
	if (highest_ack_ == maxseq_)
		force_set_rtx_timer = 1;
	/* call helper function to fill in additional fields */
	output_helper(p);

        ++ndatapack_;
        ndatabytes_ += bytes;
	send(p, 0);

	if (seqno == curseq_ && seqno > maxseq_)
		idle();  // Tell application I have sent everything so far
	if (seqno > maxseq_) {
		maxseq_ = seqno;
		if (!rtt_active_) {
			rtt_active_ = 1;
			if (seqno > rtt_seq_) {
				rtt_seq_ = seqno;
				rtt_ts_ = Scheduler::instance().clock();
			}
					
		}
	} else {
        	++nrexmitpack_;
        	nrexmitbytes_ += bytes;
	}
	if (!(rtx_timer_.status() == TIMER_PENDING) || force_set_rtx_timer)
		/* No timer pending.  Schedule one. */
		set_rtx_timer();
}

/*----- opencwnd 
 *
 * Option  2 lets TCP open its window 
 * by the amount indicated by the router
 * Which option to use depends on the header of 
 * received ack and is figured out in recv_newack_helper
 * 
 */
void XcpAgent::opencwnd()
{
	if (maxcwnd_ && (cwnd_ > double(maxcwnd_)))
		cwnd_ = double(maxcwnd_);

	return;
}

void XcpAgent::recv_newack_helper(Packet *pkt) {
	newack(pkt);
	// XCP changes

	if (channel_)
		trace_var("xcp_sparse_seqno_", xcp_sparse_seqno_);
	if (xcp_sparse_) {	
		hdr_tcp *tcph = hdr_tcp::access(pkt);
		if (xcp_sparse_seqno_ == tcph->seqno()) {
			xcp_sparse_seqno_ = -1; //signal to send again
		}
	}
		
	hdr_xcp *xh = hdr_xcp::access(pkt);
	if (xh->xcp_enabled_ != hdr_xcp::XCP_DISABLED) {
		if(channel_) {
			trace_var("reverse_feedback_", xh->reverse_feedback_);
			trace_var("controlling_hop_", xh->controlling_hop_);
		}

		double delta_cwnd = 0;
		
		delta_cwnd = (xh->reverse_feedback_
			      * srtt_estimate_
			      / size_);
// 		delta_cwnd =  xh->reverse_feedback_ * xh->rtt_ / size_;
		
		double newcwnd = (cwnd_ + delta_cwnd);

		if (newcwnd < 1.0)
			newcwnd = 1.0;
		if (maxcwnd_ && (newcwnd > double(maxcwnd_)))
			newcwnd = double(maxcwnd_);
		cwnd_ = newcwnd;
		if (channel_)
			trace_var("newcwnd", newcwnd);
	
	}
	// End of XCP changes

	// code below is old TCP
	//if (!ect_ || !hdr_flags::access(pkt)->ecnecho() ||
	//(old_ecn_ && ecn_burst_)) 
	/* If "old_ecn", this is not the first ACK carrying ECN-Echo
	 * after a period of ACKs without ECN-Echo.
	 * Therefore, open the congestion window. */
	//opencwnd();
	//if (ect_) {
	//if (!hdr_flags::access(pkt)->ecnecho())
	//	ecn_backoff_ = 0;
	//if (!ecn_burst_ && hdr_flags::access(pkt)->ecnecho())
	//	ecn_burst_ = TRUE;
	//else if (ecn_burst_ && ! hdr_flags::access(pkt)->ecnecho())
	//	ecn_burst_ = FALSE;
	//}
	//if (!ect_ && hdr_flags::access(pkt)->ecnecho() &&
	//!hdr_flags::access(pkt)->cong_action())
	//ect_ = 1;
	/* if the connection is done, call finish() */
	//if ((highest_ack_ >= curseq_-1) && !closed_) {
	//	closed_ = 1;
	//finish();
	//}

	// Code below is from the ns2.8 tcp.cc

	if (!ect_ || !hdr_flags::access(pkt)->ecnecho() ||
	    (old_ecn_ && ecn_burst_)) {
		/* If "old_ecn", this is not the first ACK carrying ECN-Echo
		 * after a period of ACKs without ECN-Echo.
		 * Therefore, open the congestion window. */
		/* if control option is set, and the sender is not
		   window limited, then do not increase the window size */
		
		if (!control_increase_ || 
		    (control_increase_ && (network_limited() == 1))) 
	      		opencwnd();
	}
	if (ect_) {
		if (!hdr_flags::access(pkt)->ecnecho())
			ecn_backoff_ = 0;
		if (!ecn_burst_ && hdr_flags::access(pkt)->ecnecho())
			ecn_burst_ = TRUE;
		else if (ecn_burst_ && ! hdr_flags::access(pkt)->ecnecho())
			ecn_burst_ = FALSE;
	}
	if (!ect_ && hdr_flags::access(pkt)->ecnecho() &&
	    !hdr_flags::access(pkt)->cong_action())
		ect_ = 1;
	/* if the connection is done, call finish() */
	if ((highest_ack_ >= curseq_-1) && !closed_) {
		closed_ = 1;
		finish();
	}
	if (QOption_ && curseq_ == highest_ack_ +1) {
		cancel_rtx_timer();
	}
}

void XcpAgent::rtt_update(double tao)
{
#define FIX1 1 /* 1/0 : 1 for experimental XCP changes, works only with timestamps */
	double now = Scheduler::instance().clock();
	double sendtime = now - tao; // XXX instead, better pass send/recv times as args
	if (ts_option_) {
#if FIX1
		int send_tick = int(sendtime/tcp_tick_);
		int recv_tick = int(now/tcp_tick_);
		t_rtt_ = recv_tick - send_tick;
#else
		t_rtt_ = int(tao /tcp_tick_ + 0.5);
#endif /* FIX1 */
	} else {
		// XXX I don't understand this business with
		// boot_time_, and so not quite sure what FIX1 should
		// look like in this case perhaps something like:
		//      t_rtt_ = int(now/tcp_tick_) - int((sendtime - tickoff)/tcp_tick_);
		// for now FIX1 works only with timestamps.
 
		sendtime += boot_time_;
		double tickoff = fmod(sendtime, tcp_tick_);
		t_rtt_ = int((tao + tickoff) / tcp_tick_);
	}
	// XCP changes
	assert(t_rtt_ >= 0);
	if (xcp_srtt_ != 0)
		xcp_srtt_ = XCP_UPDATE_SRTT(xcp_srtt_, t_rtt_);
	else 
		xcp_srtt_ = XCP_INIT_SRTT(t_rtt_);
	if (xcp_srtt_ == 0)
		xcp_srtt_ = 0;
	srtt_estimate_ = double(xcp_srtt_) * tcp_tick_ / double(1 << XCP_RTT_SHIFT);

	if (TRACE) {
		printf("%d:  %g  SRTT %g, RTT %g \n", tcpId_, now, srtt_estimate_, tao);
	}
	// End of XCP changes

	// XXX does the following check make sense?
	if (t_rtt_ < 1)
		t_rtt_ = 1;

	//
	// srtt has 3 bits to the right of the binary point
	// rttvar has 2
	//
        if (t_srtt_ != 0) {
		register short delta;

		delta = t_rtt_ - (t_srtt_ >> T_SRTT_BITS);	// d = (m - a0)

		if ((t_srtt_ += delta) <= 0)	// a1 = 7/8 a0 + 1/8 m
			t_srtt_ = 1;
		if (delta < 0)
			delta = -delta;
		delta -= (t_rttvar_ >> T_RTTVAR_BITS);
		if ((t_rttvar_ += delta) <= 0)	// var1 = 3/4 var0 + 1/4 |d|
			t_rttvar_ = 1;
	} else {
		t_srtt_ = t_rtt_ << T_SRTT_BITS;		// srtt = rtt
		t_rttvar_ = t_rtt_ << (T_RTTVAR_BITS-1);	// rttvar = rtt / 2
	}

	t_rtxcur_ = (((t_rttvar_ << (rttvar_exp_ + (T_SRTT_BITS - T_RTTVAR_BITS))) +
		      t_srtt_)  >> T_SRTT_BITS ) * tcp_tick_;

	return;
}

void XcpAgent::rtt_init()
{
	TcpAgent::rtt_init();
	// XCP Changes 
	init_rtt_vars();
	rtt_active_ = 0;
	rtt_seq_ = -1;
	// End of XCP Changes
}

void XcpAgent::trace_var(char * var_name, double var)
{
	char wrk[500];
	if (channel_) {
		int n;
		sprintf(wrk, "%g x x x x %s %g",time_now(),var_name, var);
		n = strlen(wrk);
		wrk[n] = '\n'; 
		wrk[n+1] = 0;
		(void)Tcl_Write(channel_, wrk, n+1);
	}
}


XcpSink::XcpSink(Acker* acker) : Agent(PT_ACK), acker_(acker), save_(NULL),
				 lastreset_(0.0)
{ 
}

void
XcpSink::delay_bind_init_all()
{
        delay_bind_init_one("packetSize_");
        delay_bind_init_one("ts_echo_bugfix_");
	delay_bind_init_one("ts_echo_rfc1323_");
	delay_bind_init_one("bytes_"); // For throughput measurements in JOBS
	delay_bind_init_one("RFC2581_immediate_ack_");

	Agent::delay_bind_init_all();
}

int
XcpSink::delay_bind_dispatch(const char *varName, 
			     const char *localName, 
			     TclObject *tracer)
{
        if (delay_bind(varName, localName, 
		       "packetSize_", &size_, 
		       tracer)) 
		return TCL_OK;
        if (delay_bind_bool(varName, localName, 
			    "ts_echo_bugfix_", &ts_echo_bugfix_, 
			    tracer)) 
		return TCL_OK;
	if (delay_bind_bool(varName, localName, 
			    "ts_echo_rfc1323_", &ts_echo_rfc1323_,
			    tracer)) 
		return TCL_OK;
        if (delay_bind_bool(varName, localName, 
			    "RFC2581_immediate_ack_", &RFC2581_immediate_ack_,
			    tracer)) 
		return TCL_OK;

        return Agent::delay_bind_dispatch(varName, localName, tracer);
}

int XcpSink::command(int argc, const char*const* argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "reset") == 0) {
			reset();
			return (TCL_OK);
		}
	}
	return (Agent::command(argc, argv));
}

void XcpSink::reset() 
{
	acker_->reset();	
	save_ = NULL;
	lastreset_ = Scheduler::instance().clock(); /* W.N. - for detecting
						     * packets from previous 
						     * incarnations */
}

void XcpSink::ack(Packet* opkt)
{
	Packet* npkt = allocpkt();
	double now = Scheduler::instance().clock();

	hdr_tcp *otcp = hdr_tcp::access(opkt);
	hdr_tcp *ntcp = hdr_tcp::access(npkt);
	ntcp->seqno() = acker_->Seqno();
	ntcp->ts() = now;

	if (ts_echo_bugfix_)  /* TCP/IP Illustrated, Vol. 2, pg. 870 */
		ntcp->ts_echo() = acker_->ts_to_echo();
	else
		ntcp->ts_echo() = otcp->ts();

	hdr_ip* oip = HDR_IP(opkt);
	hdr_ip* nip = HDR_IP(npkt);
	nip->flowid() = oip->flowid();

	hdr_flags* of = hdr_flags::access(opkt);
	hdr_flags* nf = hdr_flags::access(npkt);
	hdr_flags *sf;
	if (save_ != NULL) 
		sf = hdr_flags::access(save_);
	else
		sf = 0;
	// Look at delayed packet being acked. 
	if ( (save_ != NULL && sf->cong_action()) || of->cong_action() ) 
		// Sender has responsed to congestion. 
		acker_->update_ecn_unacked(0);
	if ( (sf != 0 && sf->ect() && sf->ce())  || 
	     (of->ect() && of->ce()) )
		// New report of congestion.  
		acker_->update_ecn_unacked(1);
	if ( (sf != 0 && sf->ect()) || of->ect() )
		// Set EcnEcho bit.  
		nf->ecnecho() = acker_->ecn_unacked();
	if (!of->ect() && of->ecnecho() ||
	    (sf != 0 && !sf->ect() && sf->ecnecho()) ) 
		// This is the negotiation for ECN-capability.
		// We are not checking for of->cong_action() also. 
		// In this respect, this does not conform to the 
		// specifications in the internet draft 
		nf->ecnecho() = 1;

	// XCP Changes

	hdr_xcp *oxcp = hdr_xcp::access(opkt);
	hdr_xcp *nxcp = hdr_xcp::access(npkt);
	if (oxcp->xcp_enabled_ != hdr_xcp::XCP_DISABLED) {
		nxcp->xcp_enabled_ = hdr_xcp::XCP_ACK;
		nxcp->reverse_feedback_ = oxcp->delta_throughput_;
		nxcp->rtt_ = oxcp->rtt_; /* XXX relay back original rtt for debugging */
	} else 
		nxcp->xcp_enabled_ = hdr_xcp::XCP_DISABLED;

	// End of XCP Changes

	acker_->append_ack(hdr_cmn::access(npkt),
			   ntcp, otcp->seqno());
	add_to_ack(npkt);
        // Andrei Gurtov
        acker_->last_ack_sent_ = ntcp->seqno();

	send(npkt, 0);
}

void XcpSink::add_to_ack(Packet*)
{
	return;
}

void XcpSink::recv(Packet* pkt, Handler*)
{
	int numToDeliver;
	// number of bytes in the packet just received
	int numBytes = hdr_cmn::access(pkt)->size();
	hdr_tcp *th = hdr_tcp::access(pkt);

	/* W.N. Check if packet is from previous incarnation */
	if (th->ts() < lastreset_) {
		// Remove packet and do nothing
		Packet::free(pkt);
		return;
	}
	// update the timestamp to echo
	acker_->update_ts(th->seqno(),th->ts(),ts_echo_rfc1323_);

	// update the recv window; figure out how many in-order-bytes
	// (if any) can be removed from the window and handed to the
	// application
      	numToDeliver = acker_->update(th->seqno(), numBytes);

	// send any packets to the application
	if (numToDeliver)
		recvBytes(numToDeliver);

	// ACK the packet
      	ack(pkt);
	
	// remove it from the system
	Packet::free(pkt);
}
