
/*
 * tcp-qs.cc
 * Copyright (C) 2001 by the University of Southern California
 * $Id: tcp-qs.cc,v 1.1.1.1 2006/03/08 13:53:01 rouil Exp $
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

/*
 * Quick Start for TCP and IP.
 * A scheme for transport protocols to dynamically determine initial 
 * congestion window size.
 *
 * http://www.ietf.org/internet-drafts/draft-amit-quick-start-02.ps
 *
 * This implements the TCP Quick Start Source and Sink Agents.
 * TCP Quick Start Source Agent is based on the Rate Based 
 * implementation of TCP. "Agent/TCP/Newreno/QS", "Agent/TCPSink/QS"
 *
 * tcp-qs.cc
 *
 * Srikanth Sundarrajan, 2002
 * sundarra@usc.edu
 *
 * Modifications: Pasi Sarolahti <pasi.sarolahti@iki.fi>, Sept. 2004
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "ip.h"
#include "tcp.h"
#include "flags.h"
#include "hdr_qs.h"
#include "random.h"
#include "tcp-sink.h"

#ifndef MIN
#define MIN(x, y) ((x)<(y) ? (x) : (y))
#endif /* ! MIN */

#if 0
#define RBP_DEBUG_PRINTF(x) printf x
#else /* ! 0 */
#define RBP_DEBUG_PRINTF(x)
#endif /* 0 */


#define RBP_MIN_SEGMENTS 2

/***********************************************************************
 *
 * The New reno-based version
 *
 */

class QSNewRenoTcpAgent;

class QSNewRenoPaceTimer : public TimerHandler {
public:
	QSNewRenoPaceTimer(QSNewRenoTcpAgent *a) : TimerHandler() { a_ = a; }
protected:
	virtual void expire(Event *e);
	QSNewRenoTcpAgent *a_;
};
// Hmmm... ``a is a'' in the construction of the QSNewRenoPaceTimer edifice :->

class QSNewRenoTcpAgent : public virtual NewRenoTcpAgent {
	friend class QSNewRenoPaceTimer;
 public:
	QSNewRenoTcpAgent();
	virtual void recv(Packet *pkt, Handler *);
	virtual void timeout(int tno);
	virtual void send_much(int force, int reason, int maxburst);
	virtual void output(int force, int reason);

	double rbp_scale_;   // conversion from actual -> rbp send rates
	// enum rbp_rate_algorithms { RBP_NO_ALGORITHM, RBP_VEGAS_RATE_ALGORITHM, RBP_CWND_ALGORITHM };
	// int rbp_rate_algorithm_;
protected:
	void paced_send_one();
	int able_to_rbp_send_one();

	// stats on what we did
	int rbp_segs_actually_paced_;

	int ttl_diff_;
	int qs_approved_;
	int rate_request_;

	int session_id_;

	static int next_flow_;

	enum rbp_modes { RBP_GOING, RBP_POSSIBLE, RBP_OFF };
	enum rbp_modes rbp_mode_;
	double rbp_inter_pace_delay_;
	QSNewRenoPaceTimer pace_timer_;
};

int QSNewRenoTcpAgent::next_flow_ = 0;

static class QSNewRenoTcpClass : public TclClass {
public:
	QSNewRenoTcpClass() : TclClass("Agent/TCP/Newreno/QS") {}
	TclObject* create(int, const char*const*) {
		return (new QSNewRenoTcpAgent());
	}
} class_newreno_qs;


void QSNewRenoPaceTimer::expire(Event *) { a_->paced_send_one(); }

QSNewRenoTcpAgent::QSNewRenoTcpAgent() : TcpAgent(),
	ttl_diff_(0), qs_approved_(0), rbp_mode_(RBP_OFF), pace_timer_(this)
{
	bind("rbp_scale_", &rbp_scale_);
	// algorithm is not used in New Reno
	// bind("rbp_rate_algorithm_", &rbp_rate_algorithm_);
	bind("rbp_segs_actually_paced_", &rbp_segs_actually_paced_);
	bind("rbp_inter_pace_delay_", &rbp_inter_pace_delay_);
	bind("rate_request_", &rate_request_);

	session_id_ = next_flow_ % 32;
	next_flow_ = session_id_ + 1;
}

void
QSNewRenoTcpAgent::recv(Packet *pkt, Handler *hand)
{
	double now = Scheduler::instance().clock();
	int app_rate;

	hdr_tcp *tcph = hdr_tcp::access(pkt);
	if (rbp_mode_ != RBP_OFF) {
		// reciept of anything disables rbp
		rbp_mode_ = RBP_OFF;

		// reset cwnd such that we're now ack clocked.
		if (tcph->seqno() > last_ack_) {
			cwnd_ = maxseq_ - last_ack_; //this is what we need for QS.
			RBP_DEBUG_PRINTF(("\ncwnd-after-first-ack=%g\n", (double)cwnd_));
		};

	};
	if (acked_ == 0) {
		hdr_qs *qsh = hdr_qs::access(pkt);

		if (qsh->flag() == QS_RESPONSE && qsh->ttl() == ttl_diff_ && qsh->rate() > 0) {
			printf("Quick Start approved\t");
                        // PS: Convert rate to initial window in pkts
                        app_rate = (int) (hdr_qs::rate_to_Bps(qsh->rate()) *
                            (now - tcph->ts_echo()) / (size_ + headersize()));
			if (app_rate > initial_window()) {
				rbp_mode_ = RBP_POSSIBLE;
				wnd_init_option_ = 1;
				wnd_init_ = app_rate;
				printf("%d: rate= %d, rtt = %f\n", addr(), app_rate, (now - tcph->ts_echo()));
				qs_approved_ = 1;
			}
			else {
				printf("%d: quick start approved, but rate too low %d, fall-back to slow start\n", addr(), app_rate);
				rbp_mode_ = RBP_OFF;
				qsh->flag() = QS_DISABLE;
				qs_approved_ = 0;
			}
		} else { // Quick Start rejected
			printf("Quick Start rejected\n");
			rbp_mode_ = RBP_OFF;
			qsh->flag() = QS_DISABLE;
			qs_approved_ = 0;
		}
	} else if (acked_ == 1 && qs_approved_ == 1) {
		//don't have to do anything here, RBP is doing exactly what we need
	}
	NewRenoTcpAgent::recv(pkt, hand);
}

void
QSNewRenoTcpAgent::timeout(int tno)
{
	if (tno == TCP_TIMER_RTX) {
		if (highest_ack_ == maxseq_) {
			// Idle for a while => RBP next time.
			//rbp_mode_ = RBP_POSSIBLE;
			rbp_mode_ = RBP_OFF; //this is not an RBP implementation
			return;
		}
		else {
			rbp_mode_ = RBP_OFF; //turn off RBP
			cwnd_ = initial_window();
		}
	}
	NewRenoTcpAgent::timeout(tno);
}

void
QSNewRenoTcpAgent::send_much(int force, int reason, int maxburst)
{
	if (rbp_mode_ == RBP_POSSIBLE && able_to_rbp_send_one()) {
		// start paced mode
		rbp_mode_ = RBP_GOING; 
		rbp_segs_actually_paced_ = 0;

		// Pace out scaled cwnd.
		double rbwin_reno;
		rbwin_reno = cwnd_ * rbp_scale_;

		rbwin_reno = int(rbwin_reno + 0.5);   // round
		// Always pace at least RBP_MIN_SEGMENTS
		if (rbwin_reno <= RBP_MIN_SEGMENTS) {
			rbwin_reno = RBP_MIN_SEGMENTS;
		};

		// Conservatively set the congestion window to min of
		// congestion window and the smoothed rbwin_reno
		RBP_DEBUG_PRINTF(("cwnd before check = %g\n", double(cwnd_)));
		cwnd_ = MIN(cwnd_,(TracedDouble) rbwin_reno);
		RBP_DEBUG_PRINTF(("cwnd after check = %g\n", double(cwnd_)));
		RBP_DEBUG_PRINTF(("recv win = %g\n", wnd_));
		// RBP timer calculations must be based on the actual
		// window which is the min of the receiver's
		// advertised window and the congestion window.
		// TcpAgent::window() does this job.
		// What this means is we expect to send window() pkts
		// in v_srtt_ time.
		static double srtt_scale = 0.0;
		if (srtt_scale == 0.0) {  // yuck yuck yuck!
			srtt_scale = 1.0; // why are we doing fixed point?
			int i;
			for (i = T_SRTT_BITS; i > 0; i--) {
				srtt_scale /= 2.0;
			};
		}
		rbp_inter_pace_delay_ = (t_srtt_ * srtt_scale * tcp_tick_) / (window() * 1.0);
		RBP_DEBUG_PRINTF(("window is %d\n", window()));
		RBP_DEBUG_PRINTF(("ipt = %g\n", rbp_inter_pace_delay_));
		paced_send_one();
	} else {
		NewRenoTcpAgent::send_much(force,reason, maxburst);
	};
}

void
QSNewRenoTcpAgent::paced_send_one()
{
	if (rbp_mode_ == RBP_GOING && able_to_rbp_send_one()) {
		RBP_DEBUG_PRINTF(("Sending one rbp packet\n"));
		// send one packet
		output(t_seqno_++, TCP_REASON_RBP);
		rbp_segs_actually_paced_++;
		// schedule next pkt
		pace_timer_.resched(rbp_inter_pace_delay_);
	};
}

int
QSNewRenoTcpAgent::able_to_rbp_send_one()
{
	return t_seqno_ < curseq_ && t_seqno_ <= highest_ack_ + window();
}

void QSNewRenoTcpAgent::output(int seqno, int reason)
{
	int force_set_rtx_timer = 0;
	Packet* p = allocpkt();
	hdr_tcp *tcph = hdr_tcp::access(p);
	hdr_ip *iph = hdr_ip::access(p);
	hdr_qs *qsh = hdr_qs::access(p);
	hdr_flags* hf = hdr_flags::access(p);
	int databytes = hdr_cmn::access(p)->size();
	tcph->seqno() = seqno;
	tcph->ts() = Scheduler::instance().clock();
	tcph->ts_echo() = ts_peer_;
	tcph->reason() = reason;
	tcph->last_rtt() = int(int(t_rtt_)*tcp_tick_*1000);
	//iph->flowid() = session_id_;

	if (seqno == 0) {
		qsh->flag() = QS_REQUEST;
		qsh->ttl() = Random::integer(256);
		ttl_diff_ = (iph->ttl() - qsh->ttl()) % 256;
                // PS: rate_request_ parameter is in KB/sec
                qsh->rate() = hdr_qs::Bps_to_rate(rate_request_ * 1024);
	}
	else {
		qsh->flag() = QS_DISABLE;
	}

	if (ecn_) {
		hf->ect() = 1;	// ECN-capable transport
	}
	if (cong_action_) {
		hf->cong_action() = TRUE;  // Congestion action.
		cong_action_ = FALSE;
		}
	/* Check if this is the initial SYN packet. */
	if (seqno == 0) {
		if (syn_) {
			databytes = 0;
			curseq_ += 1;
			hdr_cmn::access(p)->size() = tcpip_base_hdr_size_;
			//printf("inside initial syn packet\n");
		}
		if (ecn_) {
			hf->ecnecho() = 1;
//			hf->cong_action() = 1;
			hf->ect() = 0;
		}
	}
	else if (useHeaders_ == true) {
		hdr_cmn::access(p)->size() += headersize();
	}
		hdr_cmn::access(p)->size();

	/* if no outstanding data, be sure to set rtx timer again */
	if (highest_ack_ == maxseq_)
		force_set_rtx_timer = 1;
	/* call helper function to fill in additional fields */
	output_helper(p);

		++ndatapack_;
		ndatabytes_ += databytes;
	send(p, 0);
	//printf("wnd_ %f, cwnd_ %f, ssthresh_ %f\n", wnd_+0, cwnd_+0, ssthresh_+0);
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
		nrexmitbytes_ += databytes;
	}
	if (!(rtx_timer_.status() == TIMER_PENDING) || force_set_rtx_timer)
		/* No timer pending.  Schedule one. */
		set_rtx_timer();
}

class QSTcpSink;

class QSTcpSink : public TcpSink {
public:
	QSTcpSink(Acker *);
	virtual void ack(Packet * oPacket);
	void recv(Packet* pkt, Handler*);
};

static class QSTcpSinkClass : public TclClass {
public:
	QSTcpSinkClass() : TclClass("Agent/TCPSink/QS") {}
	TclObject* create(int, const char*const*) {
		return (new QSTcpSink(new Acker));
	}
} class_sink_qs;

QSTcpSink::QSTcpSink(Acker * acker) : TcpSink(acker) {
}

void QSTcpSink::recv(Packet* pkt, Handler*)
{
	int numToDeliver;
	int numBytes = hdr_cmn::access(pkt)->size();
	// number of bytes in the packet just received
	hdr_tcp *th = hdr_tcp::access(pkt);
	/* W.N. Check if packet is from previous incarnation */
	if (th->ts() < lastreset_) {
		// Remove packet and do nothing
		Packet::free(pkt);
		return;
	}
	acker_->update_ts(th->seqno(),th->ts(),ts_echo_rfc1323_);
	// update the timestamp to echo
	
	numToDeliver = acker_->update(th->seqno(), numBytes);
	// update the recv window; figure out how many in-order-bytes
	// (if any) can be removed from the window and handed to the
	// application
	if (numToDeliver)
		recvBytes(numToDeliver);
	// send any packets to the application
		  ack(pkt);
	// ACK the packet
	Packet::free(pkt);
	// remove it from the system
}

void QSTcpSink::ack(Packet* opkt)
{
	Packet* npkt = allocpkt();
	// opkt is the "old" packet that was received
	// npkt is the "new" packet being constructed (for the ACK)
	double now = Scheduler::instance().clock();

	hdr_tcp *otcp = hdr_tcp::access(opkt);
	hdr_tcp *ntcp = hdr_tcp::access(npkt);

	hdr_ip *oiph = hdr_ip::access(opkt);

	hdr_qs *oqsh = hdr_qs::access(opkt);
	hdr_qs *nqsh = hdr_qs::access(npkt);

	if (otcp->seqno() == 0 && oqsh->flag() == QS_REQUEST) {
		nqsh->flag() = QS_RESPONSE;
		nqsh->ttl() = (oiph->ttl() - oqsh->ttl()) % 256;
		nqsh->rate() = oqsh->rate();
	}
	else {
		nqsh->flag() = QS_DISABLE;
	}
	
	// get the tcp headers
	ntcp->seqno() = acker_->Seqno();
	// get the cumulative sequence number to put in the ACK; this
	// is just the left edge of the receive window - 1
	ntcp->ts() = now;
	// timestamp the packet

	if (ts_echo_bugfix_)  /* TCP/IP Illustrated, Vol. 2, pg. 870 */
		ntcp->ts_echo() = acker_->ts_to_echo();
	else
		ntcp->ts_echo() = otcp->ts();
	// echo the original's time stamp

	// hdr_ip* oip = hdr_ip::access(opkt);
	// hdr_ip* nip = hdr_ip::access(npkt);
	// get the ip headers
	//nip->flowid() = oip->flowid();
	// copy the flow id
	
	hdr_flags* of = hdr_flags::access(opkt);
	hdr_flags* nf = hdr_flags::access(npkt);
	hdr_flags* sf;
	if (save_ != NULL) {
		sf = hdr_flags::access(save_);
	} else {
	        sf = 0;
	}
		// Look at delayed packet being acked. 
	if ( (sf != 0 && sf->cong_action()) || of->cong_action() ) 
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
	acker_->append_ack(hdr_cmn::access(npkt),
			   ntcp, otcp->seqno());
	add_to_ack(npkt);
	// the above function is used in TcpAsymSink
	
	send(npkt, 0);
	// send it
}

