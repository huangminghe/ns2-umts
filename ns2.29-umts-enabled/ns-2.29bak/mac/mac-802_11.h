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
 * $Header: /var/lib/cvs/ns-2.29/mac/mac-802_11.h,v 1.10 2007/02/26 16:25:51 rouil Exp $
 *
 * Ported from CMU/Monarch's code, nov'98 -Padma.
 * wireless-mac-802_11.h
 */

#ifndef ns_mac_80211_h
#define ns_mac_80211_h

// Added by Sushmita to support event tracing (singal@nunki.usc.edu)
#include "address.h"
#include "ip.h"

#include "mac-802_11.h"
#include "mac-timers.h"
#include "mac-stats.h"
#include "marshall.h"
#include <math.h>
#include <map>

class EventTrace;

#define GET_ETHER_TYPE(x)		GET2BYTE((x))
#define SET_ETHER_TYPE(x,y)            {u_int16_t t = (y); STORE2BYTE(x,&t);}

/* ======================================================================
   Frame Formats
   ====================================================================== */

#define	MAC_ProtocolVersion	0x00

#define MAC_Type_Management	0x00
#define MAC_Type_Control	0x01
#define MAC_Type_Data		0x02
#define MAC_Type_Reserved	0x03

#define MAC_Subtype_RTS		0x0B
#define MAC_Subtype_CTS		0x0C
#define MAC_Subtype_ACK		0x0D
#define MAC_Subtype_Data	0x00

/* NIST: add association information */

#define MAC_Subtype_AssocReq	0x00
#define MAC_Subtype_AssocRes	0x01
#define MAC_Subtype_ProbeReq    0x04
#define MAC_Subtype_ProbeRes    0x05
#define MAC_Subtype_Beacon2     0x08 //type MAC_Subtype_Beacon already defined in wpan

#define BEACON_FRAME_BODY_SIZE  16
//State linked to L2 handover
enum MacStateHandover {
	MAC_CONNECTED,
	MAC_DISCONNECTED,
	MAC_BEING_CONNECTED
};

//information used on Network side 
enum ClientStatus {
	CLIENT_PENDING,
	CLIENT_ASSOCIATED
};

/* 
 * Client timer
 * One timer is associated with each entry in the client list.
 */
class Mac802_11;
class ClientTimer : public TimerHandler {
 public:
        ClientTimer(Mac802_11 *m, int addr) : TimerHandler() 
	  { m_ = m; addr_ = addr;}
 protected:
        void expire(Event *);
        Mac802_11 *m_;  //The Mac that will process the timer
	int addr_;      //The client address
};


class Client;
 LIST_HEAD (client, Client);

class Client {
public:
	Client (int id, double lifetime, Mac802_11 *mac): timer_(mac, id) {
		id_ = id;
		pktManagement_ = NULL;
		status_ = CLIENT_PENDING;
		nbFrameError_ = 0;
		lifetime_ = lifetime;
		timer_.resched (lifetime);
		pkt_pr_ = 0; 
		going_down_ = 0;
		
		lastupdate_ = NOW;
	}
	~Client () { ; } //destructor

	// Chain element to the list
	inline void insert_entry(struct client *head) {
		LIST_INSERT_HEAD(head, this, link);
	}

	// Return next element in the chained list
	Client* next_entry(void) const { return link.le_next; }
	// Remove the entry from the list
	inline void remove_entry() { 
		LIST_REMOVE(this, link); 
	}

	// Return the client id
	inline int& id() { return id_ ; }
	inline Packet* pkt() { return pktManagement_ ; }
	inline ClientStatus& status () { return status_ ; }
	inline int& getNbFrameError () { return nbFrameError_; }
	inline double& getPacketPr () { return pkt_pr_; }
	inline int& going_down () { return going_down_; }
	inline void update_timer () { timer_.resched (lifetime_); }
	inline void cancel_timer () { timer_.cancel (); }

	int id_; //index_ of the client
	Packet *pktManagement_; //Management frame to be sent
	ClientStatus status_;   //status of the client
	ClientTimer timer_;     //associated timer
	double lifetime_;       //lifetime before expiration
	double pkt_pr_;         //power level of last received packet
	int going_down_;        //tell if the client is going down
	int nbFrameError_;      //number of frames received with error
	double dataRate_;       //client datarate

	//NIST: BW prediction parameter
	StatWatch  txprob_;
	StatWatch  framesize_;
	double txprob_success_;
	double lastupdate_;

	//NIST: Handover parameters
        StatWatch rxp_watch_;
	StatWatch retx_watch_;

 protected:
  LIST_ENTRY(Client) link;
};
     
class BandwidthInfo;
LIST_HEAD (bandwidthInfo, BandwidthInfo);

class BandwidthInfo {
public:
	BandwidthInfo (u_int32_t node_index) {
		id_ = node_index;
		nb_pkts_ = 0;
		size_pkts_ = 0;
		bw_ = 0;
	}
	~BandwidthInfo () { ; }

	// Chain element to the list
	inline void insert_entry(struct bandwidthInfo *head) {
		LIST_INSERT_HEAD(head, this, link);
	}

	// Return next element in the chained list
	BandwidthInfo* next_entry(void) const { return link.le_next; }
	// Remove the entry from the list
	inline void remove_entry() { 
		LIST_REMOVE(this, link); 
	}

	inline int& id() { return id_ ; }
	inline int& nb_pkts () { return nb_pkts_; }
	inline int& size_pkts () { return size_pkts_; }
	inline double& bw () { return bw_; }

	int id_;
	int nb_pkts_;
	int size_pkts_;
	double bw_;

protected:
  LIST_ENTRY(BandwidthInfo) link;
};	  

// scan information
enum Mac802_11_ScanTypes {
	SCAN_TYPE_ACTIVE,
	SCAN_TYPE_PASSIVE
};

enum Mac802_11_ScanResults {
	SCAN_SUCCESS,
	SCAN_INVALID_PARAMETERS
};

enum Mac802_11_ScanSteps {
	SCAN_NONE, //no scan to do
	SCAN_WAITING, //waiting to start scan
	SCAN_ACTIVE  //we are doing a scan
};

enum Mac802_11_ScanActions {
	SCAN_ALL,   //do all the scans even if an answer is detected
	STOP_FIRST  //stop and return result when an AP is detected
};

struct Mac802_11_scan_request_s {
	Mac802_11_ScanTypes scanType;        //Active or passive scanning
	Mac802_11_ScanActions action;     //the action to take
	double probeDelay;      //Delay (in s) prior to sending Probe frame
	int nb_channels;     //the number of channels to scan
	int *pref_ch;        //the channels to scan
	double minChannelTime;  //Minimum time to spend on each channel when scanning
	double maxChannelTime;  //Maximum time to spend on each channel when scanning
};

class BSSDescription;
LIST_HEAD (bssDescription, BSSDescription);

class BSSDescription {
public:
	BSSDescription (int bssid, double beacon_per, int channel, int mihCapable) {
		bss_id_ = bssid;
		bper_ = beacon_per;
		channel_ = channel;
		mihCapable_ = mihCapable;
	}
	~BSSDescription () { ; }

	// Chain element to the list
	inline void insert_entry(struct bssDescription *head) {
		LIST_INSERT_HEAD(head, this, link);
	}

	// Return next element in the chained list
	BSSDescription* next_entry(void) const { return link.le_next; }
	// Remove the entry from the list
	inline void remove_entry() { 
		LIST_REMOVE(this, link); 
	}

	inline int& bss_id() { return bss_id_ ; }
	inline double& beacon_period () { return bper_; }
	inline int& channel () { return channel_; }
	inline int& mihCapable () { return mihCapable_; }

	int bss_id_;
	double bper_;
	int channel_;
	int mihCapable_;

protected:
  LIST_ENTRY(BSSDescription) link;
};	  
	
struct Mac802_11_scan_response_s {
	struct bssDescription bssDesc_head;
	Mac802_11_ScanResults result;
};

class MinScanTimer;
class MaxScanTimer;
struct scan_status_t {
	Mac802_11_ScanSteps step;          //current scan status
	Mac802_11_scan_request_s *req;     //request information
	MinScanTimer *minTimer;  //for Min time interval
	MaxScanTimer *maxTimer;  //for Max time interval
	Mac802_11_scan_response_s *resp;   //the response being built
	int scan_index;          //index of channel scanned
	bool recvd_response;     //indicate if we received a response for current channel
	int orig_ch;    //the channel we had before start of scan
	double	nav;		// Network Allocation Vector
};

/* end NIST */


struct frame_control {
	u_char		fc_subtype		: 4;
	u_char		fc_type			: 2;
	u_char		fc_protocol_version	: 2;

	u_char		fc_order		: 1;
	u_char		fc_wep			: 1;
	u_char		fc_more_data		: 1;
	u_char		fc_pwr_mgt		: 1;
	u_char		fc_retry		: 1;
	u_char		fc_more_frag		: 1;
	u_char		fc_from_ds		: 1;
	u_char		fc_to_ds		: 1;
};

struct rts_frame {
	struct frame_control	rf_fc;
	u_int16_t		rf_duration;
	u_char			rf_ra[ETHER_ADDR_LEN];
	u_char			rf_ta[ETHER_ADDR_LEN];
	u_char			rf_fcs[ETHER_FCS_LEN];
};

struct cts_frame {
	struct frame_control	cf_fc;
	u_int16_t		cf_duration;
	u_char			cf_ra[ETHER_ADDR_LEN];
	u_char			cf_fcs[ETHER_FCS_LEN];
};

struct ack_frame {
	struct frame_control	af_fc;
	u_int16_t		af_duration;
	u_char			af_ra[ETHER_ADDR_LEN];
	u_char			af_fcs[ETHER_FCS_LEN];
};

/* NIST: add association request/response frames */
struct association_request_frame {
	struct frame_control    arf_fc;
	u_int16_t		arf_duration;
	u_char			arf_ra[ETHER_ADDR_LEN];
	u_char			arf_sa[ETHER_ADDR_LEN];
	u_char                  capability;
	u_char                  listen_interval;
	u_char                  ssid;
	u_char                  supported_rate;
};

struct association_response_frame {
	struct frame_control    arf_fc;
	u_int16_t		arf_duration;
	u_char			arf_ra[ETHER_ADDR_LEN]; 
	u_char			arf_sa[ETHER_ADDR_LEN];
	bool                    status_;
	u_char                  capability;
	u_char                  aid;
	u_char                  supported_rate;
	};

struct beacon_frame {
	struct frame_control    bh_fc;
	u_int16_t               bh_duration;
        u_char                  bh_da[ETHER_ADDR_LEN];
        u_char                  bh_sa[ETHER_ADDR_LEN];
	u_char                  bh_bssid [ETHER_ADDR_LEN];
	u_int16_t               bh_scontrol;
};

struct probe_request_frame {
	struct frame_control    prf_fc;
	u_int16_t               prf_duration;
        u_char                  prf_da[ETHER_ADDR_LEN];
        u_char                  prf_sa[ETHER_ADDR_LEN];
	u_char                  prf_bssid [ETHER_ADDR_LEN];
	u_int16_t               prf_scontrol;
};

struct probe_response_frame {
	struct frame_control    prf_fc;
	u_int16_t               prf_duration;
        u_char                  prf_da[ETHER_ADDR_LEN];
        u_char                  prf_sa[ETHER_ADDR_LEN];
	u_char                  prf_bssid [ETHER_ADDR_LEN];
	u_int16_t               prf_scontrol;
};


/* End NIST */

// XXX This header does not have its header access function because it shares
// the same header space with hdr_mac.
struct hdr_mac802_11 {
	struct frame_control	dh_fc;
	u_int16_t		dh_duration;
	u_char                  dh_ra[ETHER_ADDR_LEN];
        u_char                  dh_ta[ETHER_ADDR_LEN];
        u_char                  dh_3a[ETHER_ADDR_LEN];
	u_int16_t		dh_scontrol;
	double                  dh_txprob;
	u_char			dh_body[0]; // XXX Non-ANSI
};


/* ======================================================================
   Definitions
   ====================================================================== */

/* Must account for propagation delays added by the channel model when
 * calculating tx timeouts (as set in tcl/lan/ns-mac.tcl).
 *   -- Gavin Holland, March 2002
 */
#define DSSS_MaxPropagationDelay        0.000002        // 2us   XXXX

class PHY_MIB {
public:
	PHY_MIB(Mac802_11 *parent);

	inline u_int32_t getCWMin() { return(CWMin); }
        inline u_int32_t getCWMax() { return(CWMax); }
	inline double getSlotTime() { return(SlotTime); }
	inline double getSIFS() { return(SIFSTime); }
	inline double getPIFS() { return(SIFSTime + SlotTime); }
	inline double getDIFS() { return(SIFSTime + 2 * SlotTime); }
	inline double getEIFS() {
		// see (802.11-1999, 9.2.10)
		return(SIFSTime + getDIFS()
                       + (8 *  getACKlen())/PLCPDataRate);
	}
	inline u_int32_t getPreambleLength() { return(PreambleLength); }
	inline double getPLCPDataRate() { return(PLCPDataRate); }
	
	inline u_int32_t getPLCPhdrLen() {
		return((PreambleLength + PLCPHeaderLength) >> 3);
	}

	inline u_int32_t getHdrLen11() {
		return(getPLCPhdrLen() + sizeof(struct hdr_mac802_11)
                       + ETHER_FCS_LEN);
	}
	
	inline u_int32_t getRTSlen() {
		return(getPLCPhdrLen() + sizeof(struct rts_frame));
	}
	
	inline u_int32_t getCTSlen() {
		return(getPLCPhdrLen() + sizeof(struct cts_frame));
	}
	
	inline u_int32_t getACKlen() {
		return(getPLCPhdrLen() + sizeof(struct ack_frame));
	}
	//NIST: add computation for associatino request/response
	inline u_int32_t getAssocResplen() {
		return(getPLCPhdrLen() + sizeof(struct association_response_frame));
	}
	inline u_int32_t getAssocReqlen() {
		return(getPLCPhdrLen() + sizeof(struct association_request_frame));
	}
	inline u_int32_t getBeaconlen () {
		return(getPLCPhdrLen() + sizeof(struct beacon_frame)+BEACON_FRAME_BODY_SIZE);
	}
	inline u_int32_t getProbeReqlen() {
		return(getPLCPhdrLen() + sizeof(struct probe_request_frame));
	}
	inline u_int32_t getProbeReslen() {
		return(getPLCPhdrLen() + sizeof(struct probe_response_frame));
	}
	//end NIST 

 private:




	u_int32_t	CWMin;
	u_int32_t	CWMax;
	double		SlotTime;
	double		SIFSTime;
	u_int32_t	PreambleLength;
	u_int32_t	PLCPHeaderLength;
	double		PLCPDataRate;
};


/*
 * IEEE 802.11 Spec, section 11.4.4.2
 *      - default values for the MAC Attributes
 */
#define MAC_FragmentationThreshold	2346		// bytes
#define MAC_MaxTransmitMSDULifetime	512		// time units
#define MAC_MaxReceiveLifetime		512		// time units

class MAC_MIB {
public:

	MAC_MIB(Mac802_11 *parent);

private:
	u_int32_t	RTSThreshold;
	u_int32_t	ShortRetryLimit;
	u_int32_t	LongRetryLimit;
public:
	u_int32_t	FailedCount;	
	u_int32_t	RTSFailureCount;
	u_int32_t	ACKFailureCount;
 public:
       inline u_int32_t getRTSThreshold() { return(RTSThreshold);}
       inline u_int32_t getShortRetryLimit() { return(ShortRetryLimit);}
       inline u_int32_t getLongRetryLimit() { return(LongRetryLimit);}
};


/* ======================================================================
   The following destination class is used for duplicate detection.
   ====================================================================== */
class Host {
public:
	LIST_ENTRY(Host) link;
	u_int32_t	index;
	u_int32_t	seqno;
};

/* ======================================================================
   NIST: add timer to handle layer2 connectivity and link down trigger.
   ====================================================================== */ 

class BSSTimer : public TimerHandler {
 public:
        BSSTimer(Mac802_11* m) : TimerHandler() { m_ = m;}
        void expire(Event *);
 protected:
        Mac802_11* m_;
};

class BeaconTimer : public TimerHandler {
public:
        BeaconTimer(Mac802_11* m) : TimerHandler() { m_ = m;}
        void expire(Event *);
protected:
        Mac802_11* m_;
};

class ProbeDelayTimer : public TimerHandler {
public:
        ProbeDelayTimer(Mac802_11* m) : TimerHandler() { m_ = m;}
        void expire(Event *);
protected:
        Mac802_11* m_;
};

class MinScanTimer : public TimerHandler {
public:
        MinScanTimer(Mac802_11* m) : TimerHandler() { m_ = m;}
        void expire(Event *);
protected:
        Mac802_11* m_;
};

class MaxScanTimer : public TimerHandler {
public:
        MaxScanTimer(Mac802_11* m) : TimerHandler() { m_ = m;}
        void expire(Event *);
protected:
        Mac802_11* m_;
};

//NIST: add timer for bandwidth computation 
class BWTimer : public TimerHandler {
 public:
        BWTimer(Mac802_11* m) : TimerHandler() { m_ = m;}
        void expire(Event *);
 protected:
        Mac802_11* m_;
};


/* ======================================================================
   The actual 802.11 MAC class.
   ====================================================================== */
class Mac802_11 : public Mac {
	friend class DeferTimer;


	friend class BackoffTimer;
	friend class IFTimer;
	friend class NavTimer;
	friend class RxTimer;
	friend class TxTimer;
	
	/*NIST: add BSSTimer and BWTimer*/
	friend class BSSTimer;
	friend class BWTimer;
	friend class Statistics;
	friend class BeaconTimer;
	friend class ProbeDelayTimer;
	friend class MinScanTimer;
	friend class MaxScanTimer;
	friend class ClientTimer;
	/* end NIST */
public:
	Mac802_11();
	void		recv(Packet *p, Handler *h);
	inline int	hdr_dst(char* hdr, int dst = -2);
	inline int	hdr_src(char* hdr, int src = -2);
	inline int	hdr_type(char* hdr, u_int16_t type = 0);
	
	inline int bss_id() { return bss_id_; }
	
	//NIST: add 802.11 channel frequency.
	double calFreq(int channelId);
	// Added by Sushmita to support event tracing
        void trace_event(char *, Packet *);
        EventTrace *et_;

	//NIST
	void start_autoscan ();


protected:
	void	backoffHandler(void);
	void	deferHandler(void);
	void	navHandler(void);
	void	recvHandler(void);
	void	sendHandler(void);
	void	txHandler(void);
	/*NIST*/
	void nextScan ();
	/*end NIST*/

private:
	int		command(int argc, const char*const* argv);

	/*
	 * Called by the timers.
	 */
	void		recv_timer(void);
	void		send_timer(void);
	int		check_pktCTRL();
	int		check_pktRTS();
	int		check_pktTx();
	/*NIST: add pktManagement function*/
	int		check_pktManagement();
	int             check_pktBeacon();
	/*End NIST*/

	/*
	 * Packet Transmission Functions.
	 */
	void	send(Packet *p, Handler *h);
	void 	sendRTS(int dst);
	void	sendCTS(int dst, double duration);
	void	sendACK(int dst);
	void	sendDATA(Packet *p);
	void	RetransmitRTS();
	void	RetransmitDATA();

	//NIST: association management
	void RetransmitManagement();
	void sendAssocRequest(int dest);
	void sendAssocResponse(int dest, Client *cl);
	void sendProbeRequest ();
	void sendProbeResponse (int dst);
	void sendBeacon ();
	//end NIST

	/*
	 * Packet Reception Functions.
	 */
	void	recvRTS(Packet *p);
	void	recvCTS(Packet *p);
	void	recvACK(Packet *p);
	void	recvDATA(Packet *p);

	//NIST: association management
	void recvAssocRequest(Packet *p);
	void recvAssocResponse(Packet *p);
	void recvProbeRequest(Packet *p);
	void recvProbeResponse(Packet *p);
	void recvBeacon(Packet *p);
	//end NIST

	void		capture(Packet *p);
	void		collision(Packet *p);
	void		discard(Packet *p, const char* why);
	void		rx_resume(void);
	void		tx_resume(void);

	inline int	is_idle(void);

	/*
	 * Debugging Functions.
	 */
	void		trace_pkt(Packet *p);
	void		dump(char* fname);

	inline int initialized() {	
		return (cache_ && logtarget_
                        && Mac::initialized());
	}

	inline void mac_log(Packet *p) {
                logtarget_->recv(p, (Handler*) 0);
        }

	double txtime(Packet *p);
	double txtime(double psz, double drt);
	double txtime(int bytes) { /* clobber inherited txtime() */ abort(); return 0;}

	inline void transmit(Packet *p, double timeout);
	inline void checkBackoffTimer(void);
	inline void postBackoff(int pri);
	inline void setRxState(MacState newState);
	inline void setTxState(MacState newState);


	inline void inc_cw() {
		cw_ = (cw_ << 1) + 1;
		if(cw_ > phymib_.getCWMax())
			cw_ = phymib_.getCWMax();
	}
	inline void rst_cw() { cw_ = phymib_.getCWMin(); }

	inline double sec(double t) { return(t *= 1.0e-6); }
	inline u_int16_t usec(double t) {
		u_int16_t us = (u_int16_t)floor((t *= 1e6) + 0.5);
		return us;
	}
	inline void set_nav(u_int16_t us) {
		double now = Scheduler::instance().clock();
		double t = us * 1e-6;
		if((now + t) > nav_) {
			nav_ = now + t;
			if(mhNav_.busy())
				mhNav_.stop();
			mhNav_.start(t);
		}
	}

	//NIST: add link commands and others
	void link_connect (int);
	void link_connect (int, int);
	void link_disconnect ();
	void link_disconnect (link_down_reason_t reason);
	struct link_get_param_s* link_get_parameters (link_parameter_type_s *parameter);
	struct link_param_th_status * link_configure_thresholds (int numLinkParameter, struct link_param_th *linkThresholds); //configure threshold
	void link_scan (void *);
	void clear_tx_rx();

	void bss_expired ();
	void bw_update();
	void process_client_timeout (int);
	double calculate_txprob();
	void predict_bw_used ();
	
	void ms_rxp_handover();
	void ms_retx_handover();
	
	virtual void queue_packet_loss();
	virtual void update_queue_stats(int);

	/** Return the datarate for the given packet
	 * This function helps to support multiple datarate
	 * @param p The packet to send
	 * @return the datarate to use
	 */
	double getTxDatarate (Packet *p);

	//end NIST

protected:
	PHY_MIB         phymib_;
        MAC_MIB         macmib_;

       /* the macaddr of my AP in BSS mode; for IBSS mode
        * this is set to a reserved value IBSS_ID - the
        * MAC_BROADCAST reserved value can be used for this
        * purpose
        */
       int              bss_id_;
       enum             {IBSS_ID=MAC_BROADCAST};

	//NIST: add frame error for link going down
	int             print_stats_;
	bool            going_down_;    //indicate if we send a link going down before
	int             channel_;       //802.11b channel id
	double          bss_timeout_;   //value of timeout for bss timer.
	double          bw_timeout_;
	BSSTimer        bssTimer_;    //timer for bss_id expiration
	BWTimer         bwTimer_;
	BeaconTimer     beaconTimer_;
	ProbeDelayTimer probeTimer_;
	struct bandwidthInfo bwList_head_;
	double          RXThreshold_;   //taken from PHY layer
	double          RetxThreshold_; //Re-transmission threshold (for re-tx triggering)
	double          pr_limit_;      //Coefficient of received power before sending 
                                   	//link going down event. Must be >=1.0
	double          prev_pr;        //received power of previous packet
	bool            first_lgd_;     //first link going down
	double*         prevDelay;      //Save previous delay for jitter calc
	
	Queue*          ifq_;           //to keep track of the queue size
	//Statistics
	StatWatch retx_watch_;	
	StatWatch rtt_watch_;
	StatWatch rxp_watch_;
	StatWatch numtx_watch_;
	StatWatch numrx_watch_;
	StatWatch qlen_watch_;
	StatWatch pktloss_watch_;
       
	StatWatch qwait_watch_;
	StatWatch collsize_watch_;
	StatWatch txerror_watch_;
	StatWatch txloss_watch_;
	StatWatch txprob_watch_;
	StatWatch occupiedbw_watch_;
	StatWatch txframesize_watch_;
	StatWatch delay_watch_;
	StatWatch jitter_watch_;

	ThroughputWatch rxtp_watch_;
	ThroughputWatch txtp_watch_;
	ThroughputWatch rxbw_watch_;
	ThroughputWatch txbw_watch_;
	ThroughputWatch rxsucc_watch_;
	ThroughputWatch txsucc_watch_;
	ThroughputWatch rxcoll_watch_;
	ThroughputWatch txcoll_watch_;
	
	/**
	 * Update the given timer and check if thresholds are crossed
	 * @param watch the stat watch to update
	 * @param value the stat value
	 */
	void update_watch (StatWatch *watch, double value);
	void update_throughput (ThroughputWatch *watch, double size);
	//end NIST


private:
	double		basicRate_;
 	double		dataRate_;

	/*
	 * Mac Timers
	 */
	IFTimer		mhIF_;		// interface timer
	NavTimer	mhNav_;		// NAV timer
	RxTimer		mhRecv_;	// incoming packets
	TxTimer		mhSend_;	// outgoing packets

	DeferTimer	mhDefer_;	// defer timer
	BackoffTimer	mhBackoff_;	// backoff timer

	/* ============================================================
	   Internal MAC State
	   ============================================================ */
	double		nav_;		// Network Allocation Vector

	MacState	rx_state_;	// incoming state (MAC_RECV or MAC_IDLE)
	MacState	tx_state_;	// outgoint state
	int		tx_active_;	// transmitter is ACTIVE
	map<int,int>    channelIDAfterScan_;

	//NIST: add handover state
	MacStateHandover hdv_state_;
	int             handover_;         //1 if I am doing a handover (switching AP)
	Packet          *pktRetransmit_;
	Packet		*pktManagement_;   // outgoing non-RTS packet, management frame
	PacketQueue     *managementQueue_; //queue for management packet
	Packet          *last_tx_;         // last packet transmitted

	//NIST: add bandwidth prediction info
	double          overhead_duration_;
	double          coll_overhead_duration_;
	bool            first_datatx_;

	struct bandwidthInfo collector_bw_;
	double          base_station_bw;
	int             old_bss_id_;
	Packet          *tmpPacket_;
	int             nb_error_;  //current number of consecutive packet error
	int             max_error_; //number of consecutive errors we accept before disconnecting mac.
	int             max_beacon_loss_; //number of beacon we tolerate to loose
	double          bs_beacon_interval_; //current beacon interval from basestation
	
	double minChannelTime_;         //Minimum time to spend during scanning
	double maxChannelTime_;         //Maximum time to spend during scanning
	struct scan_status_t scan_status_;

	//Attributes for AP only
	Packet          *pktBeacon_;
	int             nbAccepted_;       //nb of accepted connection
	int             nbRefused_;        //nb of refused connection
	double          beacon_interval_;  //time in ms between 2 beacons
	double          client_lifetime_;  //timeout for each client
	struct client   list_client_head_;	

	//End NIST

	Packet          *eotPacket_;    // copy for eot callback

	Packet		*pktRTS_;	// outgoing RTS packet
	Packet		*pktCTRL_;	// outgoing non-RTS packet

	u_int32_t	cw_;		// Contention Window
	u_int32_t	ssrc_;		// STA Short Retry Count
	u_int32_t	slrc_;		// STA Long Retry Count

	int		min_frame_len_;

	NsObject*	logtarget_;
	NsObject*       EOTtarget_;     // given a copy of packet at TX end




	/* ============================================================
	   Duplicate Detection state
	   ============================================================ */
	u_int16_t	sta_seqno_;	// next seqno that I'll use
	int		cache_node_count_;
	Host		*cache_;
};

#endif /* __mac_80211_h__ */

