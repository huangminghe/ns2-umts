// -*-  Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*-

/*
 * Copyright (C) 2004 by the University of Southern California
 * Copyright (C) 2004 by USC/ISI
 *               2002 by Dina Katabi
 * $Id: xcpq.h,v 1.1.1.1 2006/03/08 13:52:49 rouil Exp $
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
 * @(#) $Header: /var/lib/cvs/ns-2.29/xcp/xcpq.h,v 1.1.1.1 2006/03/08 13:52:49 rouil Exp $
 */


#ifndef NS_XCPQ_H
#define NS_XCPQ_H

#include "drop-tail.h"
#include "packet.h"
#include "xcp-end-sys.h"

#define  INITIAL_Te_VALUE   0.05       // Was 0.3 Be conservative when 
                                       // we don't kow the RTT

#define TRACE  1                       // when 0, we don't race or write 
                                       // var to disk

class XCPWrapQ;
class XCPQueue;

class XCPTimer : public TimerHandler { 
public:
	XCPTimer(XCPQueue *a, void (XCPQueue::*call_back)() ) 
		: a_(a), call_back_(call_back) {};
protected:
	virtual void expire (Event *e);
	XCPQueue *a_; 
	void (XCPQueue::*call_back_)();
}; 


class XCPQueue : public DropTail {
	friend class XCPTimer;
public:
	XCPQueue();
	virtual ~XCPQueue() {}
	void Tq_timeout ();  // timeout every propagation delay 
	void Te_timeout ();  // timeout every avg. rtt
	void everyRTT();     // timeout every highest rtt seen by rtr or some
	                     // preset rtt value
	void setupTimers();  // setup timers for xcp queue only
	void setEffectiveRtt(double rtt) ;
	void routerId(XCPWrapQ* queue, int i);
	int routerId(int id = -1); 
  
	int limit(int len = 0);
	void setBW(double bw);
	void setChannel(Tcl_Channel queue_trace_file);
	double totalDrops() { return total_drops_; }
  
	void spread_bytes(bool b) { 
		spread_bytes_ = b; 
		if (b) 
			Te_ = BWIDTH;
	}
	
        // Overloaded functions
	void enque(Packet* pkt);
	Packet* deque();
	virtual void drop(Packet* p);
  
	// tracing var
	void setNumMice(int mice) {num_mice_ = mice;}

protected:

	// Utility Functions
	double max(double d1, double d2) { return (d1 > d2) ? d1 : d2; }
	double min(double d1, double d2) { return (d1 < d2) ? d1 : d2; }
        int max(int i1, int i2) { return (i1 > i2) ? i1 : i2; }
	int min(int i1, int i2) { return (i1 < i2) ? i1 : i2; }
	double abs(double d) { return (d < 0) ? -d : d; }

	virtual void trace_var(char * var_name, double var);
  
	// Estimation & Control Helpers
	void init_vars();
	
	// called in enque, but packet may be dropped; used for 
	// updating the estimation helping vars such as
	// counting the offered_load_, sum_rtt_by_cwnd_
	virtual void do_on_packet_arrival(Packet* pkt);

	// called in deque, before packet leaves
	// used for writing the feedback in the packet
	virtual void do_before_packet_departure(Packet* p); 
	
  
	// ---- Variables --------
	unsigned int     routerId_;
	XCPWrapQ*        myQueue_;   //pointer to wrapper queue lying on top
	XCPTimer*        queue_timer_;
	XCPTimer*        estimation_control_timer_;
	XCPTimer*        rtt_timer_;
	double           link_capacity_bps_;

	static const double	ALPHA_		= 0.4;
	static const double	BETA_		= 0.226;
	static const double	GAMMA_		= 0.1;
	static const double	XCP_MAX_INTERVAL= 1.0;
	static const double	XCP_MIN_INTERVAL= .001;

	double          Te_;       // control interval
	double          Tq_;    
	double          Tr_;
	double          avg_rtt_;       // average rtt of flows
	double          high_rtt_;      // highest rtt seen in flows
	double          effective_rtt_; // pre-set rtt value 
	double          Cp_;
	double          Cn_;
	double          residue_pos_fbk_;
	double          residue_neg_fbk_;
	double          queue_bytes_;   // our estimate of the fluid model queue
	double          input_traffic_bytes_;       // traffic in Te 
	double          sum_rtt_by_throughput_;
	double          sum_inv_throughput_;
	double          running_min_queue_bytes_;
	unsigned int    num_cc_packets_in_Te_;
  
	bool			spread_bytes_; 
	static const int	BSIZE = 4096;
	double			b_[BSIZE];
	double			t_[BSIZE];
	int			maxb_;
	static const double	BWIDTH = 0.01;
	int			min_queue_ci_;
	int			max_queue_ci_;
  
	double		thruput_elep_;
	double		thruput_mice_;
	double		total_thruput_;
	int		num_mice_;
	// drops
	int drops_;
	double total_drops_ ;
  
	// ----- For Tracing Vars --------------//
	Tcl_Channel queue_trace_file_;
  
};


#endif //NS_XCPQ_H
