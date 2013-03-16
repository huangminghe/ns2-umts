/*
 * Copyright (c) 2004,2005, University of Cincinnati, Ohio.
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
 *	This product includes software developed by the OBR Center for 
 *      Distributed and Mobile Computing lab at the University of Cincinnati.
 * 4. Neither the name of the University nor of the lab may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef __ns_baseband_h__
#define __ns_baseband_h__

#include "packet.h"
#include "queue.h"
#include "bi-connector.h"
#include "bt.h"
#include "hdr-bt.h"
#include "math.h"
#include "bt-lossmod.h"
#include "bt-linksched.h"

using namespace std;

class Baseband;
class BTNode;

class BTCLKHandler:public Handler {
  public:
    BTCLKHandler(Baseband * bb):bb_(bb) {} 
    void handle(Event * e);
  private:
    Baseband * bb_;
};

class BTCLKSCOHandler:public Handler {
  public:
    BTCLKSCOHandler(Baseband * bb):bb_(bb) {} 
    void handle(Event * e);
  private:
    Baseband * bb_;
};

class BTCLKNHandler:public Handler {
  public:
    BTCLKNHandler(Baseband * bb):bb_(bb) {} 
    void handle(Event * e);
  private:
    Baseband * bb_;
};

class BTRESYNCHandler:public Handler {
  public:
    BTRESYNCHandler(Baseband * bb):bb_(bb) {} 
    void handle(Event * e);
  private:
    Baseband * bb_;
};

class BTTRXTOTimer:public Handler {
  public:
    BTTRXTOTimer(Baseband * bb):bb_(bb) {} 
    void handle(Event * e);
  private:
    Baseband * bb_;
};

class BTTXHandler:public Handler {
  public:
    BTTXHandler(Baseband * bb):bb_(bb) {} 
    void handle(Event * e);
  private:
    Baseband * bb_;
};

class BTRECVHandler:public Handler {
  public:
    BTRECVHandler(Baseband * bb):bb_(bb) {} 
    void handle(Event * e);
  private:
    Baseband * bb_;
};

class BTRECVlastBitHandler:public Handler {
  public:
    BTRECVlastBitHandler(Baseband * bb):bb_(bb) {} 
    void handle(Event * e);
  private:
    Baseband * bb_;
};

class BTslaveSendnonConnHandler:public Handler {
  public:
    BTslaveSendnonConnHandler(class Baseband * bb):bb_(bb) {} 
    void handle(Event * ev);
  private:
    Baseband * bb_;
};


class BTTRXoffEvent : public Event {
  public:
    BTTRXoffEvent():st(0) {}
    // setTX() { act = TXOff;}
    // setRX() { act = RXOff;}
    void clearSt() { st = 0;}
    int st;
};

struct Bd_info {
  public:
    Bd_info * next;
    Bd_info *lnext;
    bd_addr_t bd_addr;
    uchar sr;
    uchar sp;
    int bt_class;
    uchar page_scan_mode;

    clk_t clkn;
    int lt_addr;
    int pm;
    int offset;
    double drift;
    double last_seen_time;
    double dist;
    int active;
    hdr_bt::packet_type packetType;
    hdr_bt::packet_type recvPacketType;

    Bd_info(bd_addr_t ad, clk_t c, int offs = 0)
    :next(0), lnext(0), bd_addr(ad), clkn(c), lt_addr(0), pm(0), offset(offs),
	drift(0), last_seen_time(-1), dist(-1), packetType(hdr_bt::DH1),
	recvPacketType(hdr_bt::DH1) {} 

    Bd_info(bd_addr_t ad, uchar r, uchar p, uchar ps, int off)
    :next(0), lnext(0), bd_addr(ad), sr(r), sp(p), page_scan_mode(ps),
	offset(off), last_seen_time(-1), dist(-1) {}

    Bd_info(Bd_info &a, Bd_info* n)
    :next(n), lnext(0), bd_addr(a.bd_addr), clkn(a.clkn), lt_addr(a.lt_addr),
	pm(a.pm), offset(a.offset), drift(a.drift), 
	last_seen_time(a.last_seen_time), dist(a.dist), 
	packetType(a.packetType), recvPacketType(a.recvPacketType) {}

    void dump(FILE *out = 0) {
	if (!out) { out = stdout; }
	fprintf(out, "Bd:ad:%d clk:%d off:%d lt_addr:%d\n",
	       bd_addr, clkn, offset, lt_addr);
    }
};

class LMPLink;
class Piconet;
class ConnectionHandle;
class LMP;

struct BTSchedWord {
    uint8_t *word;
    uint8_t in_use;
    int len;

    BTSchedWord(bool b);
    BTSchedWord(BTSchedWord & b);
    BTSchedWord(BTSchedWord & b, int l);
    BTSchedWord(int l);
    BTSchedWord(int l, bool b);
    BTSchedWord(int l, int master, int dummy);
    ~BTSchedWord() { delete word; } 
    void dump(FILE *out = 0);
    void expand(int n) {
	if (len >= n) {
	    return;
	}
	if (n % len) {
	    return;
	}
	uint8_t *nword = new uint8_t[n];
	for(int i = 0; i < n / len; i++) {
	    for (int j = 0; j < len; j++) {
		nword[i * len + j] = word[j];
	    }
	}
	delete [] word;
	word = nword;
	len = n;
    }
};


class TxBuffer {
    friend class LMPLink;
    friend class Baseband;
    friend class BTFCFS;
  public:
    // enum Type {ACL, SCO, BBSIG};	// BBSIG is not used currently.
    TxBuffer(class Baseband * bb, class LMPLink * l, int s);
    inline int slot() { return _slot; } 
    inline int dstTxSlot() { return _dstTxSlot; }
    inline void dstTxSlot(int s) { _dstTxSlot = s; }
    inline int type();
    inline LMPLink *link() { return _link; }
    Packet *transmit();
    void update_T_poll();
    int handle_recv(Packet *);
    inline void ack(int a);
    int push(Packet * p);
    void flush();
    inline void switch_reg();
    inline void mark_ack() { _send_ack = 1; }
    inline int available() { return _next == NULL || _current == NULL; }
    // inline int available() { return _next == NULL; }
    void reset_seqn();
    inline void suspend() { _suspended = 1; }
    // inline void resume() { _suspended = 0; T_poll = T_poll_default; }
    inline void resume() { _suspended = 0; }
    inline void session_reset() {
	_suspended = 0;
	T_poll = T_poll_default;
	_fails = 0;
	_nullCntr = 0;
	 txType = rxType = hdr_bt::Invalid;
	_hasData = 1;
    }
    int hasDataPkt();
    inline int hasBcast();
    inline int nullCntr() { return _nullCntr; }

    enum SchedPrioClass { Low, High, Tight } prioClass;
    int deficit_;	//Initailized to 0, decreased by the no. of used slots
    int ttlSlots_;	// Total slots used for this link.

  private:
    Packet * _current, *_next;
    class Baseband *bb_;
    class LMPLink *_link;
    short int _slot;
    short int _dstTxSlot;
    uchar _send_ack;
    uchar _seqn_rx;
    uchar _seqn_tx;
    uchar _arqn;
    // hdr_bt::ARQN _arqn;
    bd_addr_t bd_addr_;
    int afhEnabled_;

    // flag try to sync with the master. In case the master starts a
    // SCO link before a master, the slave needs some idea if the
    // SCO link has been started at the master side.  When a slave
    // receives a SCO packet with the correct D_sco, it considers that
    // the master has started the SCO link and set _scoStarted to 1. 
    int _scoStarted;	

    #define TPOLLUPDATE_PKTNUM      8
	// record clk for last TPOLLUPDATE_PKTNUM pkt recv'd
    int32_t _T[TPOLLUPDATE_PKTNUM];		
    int _t_index;
    int _t_cntr;

    int32_t _lastPktRecvT; 	// use for Link supervision or other purpose.
    int32_t _lastDataPktRecvT;
    int _lastSynT;

    int _ttlFails;
    int _fails;
    int _ttlSent;
    int _nullCntr;
    int _suspended;
    int _hasData;

  public:

    // A link scheduler may set T_poll to [T_poll_default, T_poll_max].
    int32_t T_poll_max;
    int32_t T_poll_default;
    int32_t T_poll;

    int32_t lastPollClk;
    int32_t lastPollClk_highclass;	// used by DRR
    int prio;

    bd_addr_t remote_bd_addr;
    hdr_bt::packet_type txType;
    hdr_bt::packet_type rxType;
};

class BTChannelUseSet {
  public:
    BTChannelUseSet() { reset(); }
    BTChannelUseSet(const char *ch) { importMapByHexString(ch); }
    BTChannelUseSet(uint16_t map_h, uint32_t map_m, uint32_t map_l);

    void importMapByHexString(const char *ch);
    void importMap(uchar * map);
    uchar * exportMap(uchar * map = 0);
    inline int notUsed(int ch) { return _notUsedCh[ch]; }
    inline int numUsedCh() { return _numUsedCh; }
    inline int usedCh(int ind) { return _usedCh[ind]; }
    char * toHexString(char *s = NULL);
	
    void dump() { char s[21]; printf("%s\n", toHexString(s)); }
    void reset();

  private:
    union {
	uchar map[10];
	struct {
	    uint32_t map_l;
	    uint32_t map_m;
	    uint16_t map_h;
	} s;
    } _u;

    int _numUsedCh;
    char _usedCh[80];
    char _notUsedCh[80];

    void _expand();
};

struct BTInterfer {
    bd_addr_t sender_;
    double X_, Y_;	// position of the source
    // double Z_;	// position of the source
    double st, et;
/*
    BTInterfer(bd_addr_t s, double x, double y, double z)
    :X_(x), Y_(y), Z_(z), sender_(s) {}
*/
    // BTInterfer():X_(-1), Y_(-1), Z_(-1), sender_(-1) {}
    BTInterfer():sender_(-1), X_(-1), Y_(-1), st(-1), et(-1) {}
};

class BTInterferQue {
  public:
    BTInterferQue():_head(0), _tail(0) {}
    void add(bd_addr_t s, double x, double y, double st, double et) {
	_q[_head].sender_ = s;
	_q[_head].X_ = x;
	_q[_head].Y_ = y;
	_q[_head].st = st;
	_q[_head].et = et;

	if (++_head == BTInterferQueMaxLen) {
	    _head = 0;
	}
	if (_head == _tail) {	// overflow
	    if (++_tail == BTInterferQueMaxLen) {
		_tail = 0;
	    }
	}
    }
    int collide(class Baseband *bb, hdr_bt *bh);
    
  private:
    int _head, _tail;
    BTInterfer _q[BTInterferQueMaxLen];
};

class Baseband:public BiConnector {
    friend class LMPLink;
    friend class LMP;
    friend class BTNode;
    friend class ScatFormPL;
    friend class ScatFormator;
    friend class ScatFormLaw;

    struct PrioLevelReq {
	struct PrioLevelReq *next;
	bd_addr_t addr;
	int prio;

	PrioLevelReq(bd_addr_t b, int p, struct PrioLevelReq *n)
	:next(n), addr(b), prio(p) {}
    } *_prioLevelReq;

    enum TrainType { Train_A, Train_B };

    enum FH_sequence_type { FHChannel, FHPage, FHInquiry,
	FHPageScan, FHInqScan, FHMasterResp, FHSlaveResp,
	FHInqResp, FHAFH
    };

  public:

    enum BBState {
	STANDBY,
	PARK_SLAVE,
	UNPARK_SLAVE,
	CONNECTION,
	NEW_CONNECTION_MASTER,
	NEW_CONNECTION_SLAVE,
	PAGE,
	PAGE_SCAN,
	SLAVE_RESP,
	MASTER_RESP,
	INQUIRY,
	INQUIRY_SCAN,
	INQUIRY_RESP,
	ROLE_SWITCH_MASTER,
	ROLE_SWITCH_SLAVE,
	RS_NEW_CONNECTION_MASTER,
	RS_NEW_CONNECTION_SLAVE,
	RE_SYNC,

	INVALID			// not a real State
    };

    static const char *state_str(BBState st) {
	static const char *const str[] = {	// the order is hard coded.
	    "STANDBY",
	    "PARK_SLAVE",
	    "UNPARK_SLAVE",
	    "CONNECTION",
	    "NEW_CONNECTION_MASTER",
	    "NEW_CONNECTION_SLAVE",
	    "PAGE",
	    "PAGE_SCAN",
	    "SLAVE_RESP",
	    "MASTER_RESP",
	    "INQUIRY",
	    "INQUIRY_SCAN",
	    "INQUIRY_RESP",
	    "ROLE_SWITCH_MASTER",
	    "ROLE_SWITCH_SLAVE",
	    "RS_NEW_CONNECTION_MASTER",
	    "RS_NEW_CONNECTION_SLAVE",
	    "RE_SYNC",

	    "Invalid_state"
	};
	return str[st];
    }

    static const char *state_str_s(BBState st) {
	static const char *const str[] = {	// the order is hard coded.
	    "STB",
	    "PRK",
	    "UPK",
	    "CON",
	    "NCM",
	    "NCS",
	    "PAG",
	    "PSC",
	    "SLR",
	    "MAR",
	    "INQ",
	    "ISC",
	    "IRE",
	    "RSM",
	    "RSS",
	    "RCM",
	    "RCS",
	    "SYN",

	    "Inv"
	};
	return str[st];
    }

    enum SCOTDDState { SCO_IDLE, SCO_SEND, SCO_RECV};

    Baseband();
    void setup(bd_addr_t ad, clk_t c, LMP * lmp, BTNode * n) {
	bd_addr_ = ad;
	clkn_ = c;
	lmp_ = lmp;
	node_ = n;
    }
    void on();
    int command(int, const char *const *);
    inline void sendBBSig(Packet * p);	// sending bb pkt (page/inq/etc)
    inline void sendDown(Packet *, Handler *h = 0);
    void sendUp(Packet *, Handler *);
    inline void polled(int slot);
    Packet *stamp(Packet *, TxBuffer *);
    void tx_handle(Packet*);
    void recv_handle(Packet * p);
    void recv_handle_lastbit(Packet * p);
    int recv_filter(Packet *);
    void slave_send_nonconn(Packet *);

    void setPiconetParam(class Piconet * pico);
    void set_prio(bd_addr_t remote, int prio);
    void _try_to_set_prio(TxBuffer * txBuffer);

    void set_sched_word(BTSchedWord * sw) {
	sw->in_use = 1;
	_sched_word = sw;
    }

    void setScoLTtable(LMPLink **);
    TxBuffer *allocateTxBuffer(LMPLink * link);
    void freeTxBuffer(TxBuffer *);
    inline TxBuffer *txBuffer(int slot) { return _txBuffer[slot]; }
    void putLinkIntoSleep(LMPLink *link);

    inline TxBuffer *lookupTxBuffer(hdr_bt *);
    inline TxBuffer *lookupScoTxBuffer();

    void handle_clkn(Event * e);
    void handle_clk(Event * e);
    void handle_re_synchronize(Event * e);
    void re_sync();
    void enter_re_sync(double);
    int synToChannelByGod(hdr_bt *bh);

    inline int isMaster() { return isMaster_; }
    inline BBState state() { return _state; }
    void change_state(BBState st);
    inline const char *state_str() { return state_str(_state); }
    inline const char *state_str_s() { return state_str_s(_state); }

    void inquiry_scan(BTSchedWord *);
    void page_scan(BTSchedWord *);
    void page(bd_addr_t slave, uint8_t ps_rep_mod, uint8_t psmod,
	      int16_t clock_offset, BTSchedWord * sched_word);
    void page(bd_addr_t slave, uint8_t ps_rep_mod, uint8_t psmod,
	      int16_t clock_offset, BTSchedWord * sched_word, int to);
    void inquiry(int, int, int, BTSchedWord *);
    void inquiry_cancel();
    void inquiryScan_cancel();
    void page_scan_cancel();
    void page_cancel();
    void setNInquiry(int numSco);
    void setNPage(int numSco);

    int isBusy();

    void reset();
    void reset_all();

    void energyReset();
    void dumpEnergy(FILE * f = 0);
    void dumpTtlSlots(FILE * f = 0);
    void turn_on_tx();
    void turn_on_rx();
    void turn_on_rx_to() {
	turn_on_rx();
	// _trxoff_ev.st = _trx_st;
	_trxoff_ev.st = RX_ON;
	Scheduler::instance().schedule(&_trxtoTimer, &_trxoff_ev, 
				MAX_SLOT_DRIFT);
    }
    void turn_off_trx();
    void turn_off_trx(BTTRXoffEvent *e);
    void turn_off_rx_to();
    void off();

    inline double X() { return X_; }
    inline double Y() { return Y_; }
    inline double Z() { return Z_; }
    inline void setPos(double x, double y, double z) { X_ = x; Y_ = y; Z_ = z; }
    inline void setPos(double x, double y) { X_ = x; Y_ = y; Z_ = 0; }
    void setdest(double destx, double desty, double speed);
    inline double distance(double x1, double y1, double x2, double y2) {
	if (toroidal_x_ < 0) {
	    return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
	}
	double xx = (x1 < x2 ? x2 - x1 : x1 - x2);
	double yy = (y1 < y2 ? y2 - y1 : y1 - y2);
	if (toroidal_x_ - xx < xx && toroidal_x_ - xx >= 0) {
		xx = toroidal_x_ - xx;
	}
	if (toroidal_y_ - yy < yy && toroidal_y_ - yy >= 0) {
		yy = toroidal_y_ - yy;
	}
	return sqrt(xx * xx + yy * yy);
    }
    inline double distance(double x, double y) {
	return distance(x, y, X_, Y_);
    }
    inline double distance(Baseband *bb) {
	return distance(bb->X_, bb->Y_, X_, Y_);
    }

    int test_fh(bd_addr_t);
    void test_afh(int clk, int addr, const char* map, const char *s);
    signed char *seq_analysis(bd_addr_t bdaddr, FH_sequence_type fs, int clk, 
			int clkf, int len, int step, signed char *buf = NULL);

    enum Slot {
	BasebandSlot,		// transmitting Baseband message
	RecvSlot,
	ScoRecvSlot,
	ScoPrioSlot,
	NotAllowedSlot,
	ReserveSlot,
	DynamicSlot,
	BcastSlot,
	BeaconSlot,
	MinTxBufferSlot		// make sure it < maxNumTxBuffer set to 16
    };

    enum trx_st_t { TRX_OFF, TX_ON, RX_ON } _trx_st;

    // const static double SlotTime = 625e-6;
    // const static double Tick = 312.5e-6;
    double SlotTime;
    double Tick;

    int isMaster_;		// my role
    bd_addr_t bd_addr_;		// my bd_addr
    int lt_addr;		// my logical transport address
    bd_addr_t master_bd_addr_;
    clk_t clkn_;
    clk_t clk_;

    bd_addr_t _receiver;

    int _afhEnabled;
    uchar _transmit_fs;
    char inRS_;

    int _polling;		// flag for master to do normal polling
    int _polling_clk;

    int _insleep;
    clk_t clkn_suspend_;
    double t_clkn_suspend_;
    double X_suspend_;
    double Y_suspend_;

    int ar_addr_;		// access request address
    int _access_request_slot;

    int _inBeacon;
    double _beacon_instant;	// s.clock() at beacon instant

    double t_clkn_00;		// s.clock() at clkn_10 == 00
    double t_clk_00;		// s.clock() at clk_10 == 00
    int _reset_as;		// reset (the slot to not-allowed) after sending

    // used to adjust clk drift.
    double drift_clk;

    int _need_syn;

    uchar _polled_lt_addr;		// master use it to validate receiving
    double _poll_time;

    int lastSchedSlot;
    int lastSchedSlot_highclass;
    int drrPass;

    int pmEnabled_; 	// enable power management

    double _active_t;
    double energyMin_;
    double energy_;
    double activeEnrgConRate_;
    double activeTime_;
    double startTime_;
    double warmUpTime_;
    double trxTurnarndTime_;
    int numTurnOn_;


    // paging
    bd_addr_t _slave;
    int slot_offset;		// used for role switch
    int clock_offset;
    uint8_t page_sr_mod;
    uint8_t page_ps_mod;
    uint8_t slave_lt_addr;
    int pageTO_;
    int page_timer;
    int N_page_;
    int page_train_switch_timer;

    int ver_;		// bluetooth spec version: 10, 11, 12, etc.

    enum ScanType {Standard, Interlaced, InterlacedSecondPart};

    // page scan
    int Page_Scan_Type_;
    int T_w_page_scan_;
    int T_w_page_scan_timer;
    int T_page_scan_;
    int T_page_scan_timer;
    int SR_mode_;		//R0, R1, R2
    int SP_mode;		//P0, P1, P2
    int connectable;

    int pagerespTO;
    int pagerespTimer;
    int newconnectionTO;
    int newconnectionTimer;

    // inquiry scan
    int Inquiry_Scan_Type_;
    int T_w_inquiry_scan_;	// T_GAP_101 34 slots
    int T_w_inquiry_scan_timer;
    int T_inquiry_scan_;	// T_GAP_102  2.56s
    int T_inquiry_scan_timer;
    // int length;                      // set to T_GAP_103 30.72s
    int discoverable;		// 1 or 0
    int lap;

    int pagescan_after_inqscan;
    int page_after_inq;

    // inquiry
    int N_inquiry_;
    int inquiry_train_switch_timer;	// set to N_inquiry * 32
    int inquiryTO_;
    int inq_timer;
    int inq_max_num_responses_;
    int inq_num_responses_;
    Bd_info *discoved_bd;

    int inqrespTO;		// 128 slots  - 256
    int inqrespTimer;
    int inBackoff;
    int backoffTimer;
    int backoffParam;		// 2046

    double _page_start_time;
    double _inq_start_time;

#define BT_DRIFT_OFF	0
#define BT_DRIFT	1
#define BT_DRIFT_NORMAL	2
#define BT_DRIFT_USER	3
    int driftType_;
    int clkdrift_;

    int _freq;

    int passiveListenFs_;

    double _resyncWind;
    int _resyncCntr;
    int _resyncWindSlotNum;
    int _maxResync;
    double _resyncWindStartT;

    // handlers
    BTCLKNHandler clkn_handler;
    BTCLKHandler clk_handler;
    BTRESYNCHandler resync_handler;
    BTTRXTOTimer _trxtoTimer; 
    BTTXHandler txHandler;
    BTRECVHandler recvHandler;
    BTRECVlastBitHandler recvHandlerLastbit;
    BTslaveSendnonConnHandler slaveSendnonConnHandler;

    // Events 
    Event clk_ev;
    Event clkn_ev;
    BTTRXoffEvent _trxoff_ev;
    Event resync_ev;

    static int trace_all_stat_;
    int trace_me_stat_;
    static int trace_all_tx_;
    int trace_me_tx_;
    static int trace_all_rx_;
    int trace_me_rx_;
    static int trace_all_null_;
    int trace_me_null_;
    static int trace_all_poll_;
    int trace_me_poll_;
    static int trace_all_in_air_;
    int trace_me_in_air_;

    static int trace_all_link_;
    int trace_me_link_;
    static int trace_all_awmmf_;
    int trace_me_awmmf_;

    static double radioRange;
    static double collisionDist;

    // toroidal distance is an artificial distance which removes 
    // border/corner effect.
    // 
    // if they are set to a value > 0, then toroidal distance instead of
    //   Euclidian distance is used.  See, 
    //   Christian Bettstetter, On the Minimum Node Degree and Connectivity
    //   of a Wireless Multihop Network.  MobiHoc'02 June 9-11, 2002,
    //   EPF Lausanne, Switzerland.
    static double toroidal_x_;	// width of the area, or x_max_.
    static double toroidal_y_;	// heighth of the area, or y_max_.

    static BTLossMod *lossmod;

    BTLinkScheduler *linkSched;

    static int check_wifi_;
    static int useSynByGod_;

    static int32_t T_poll_max;
    static int32_t T_poll_default;

    double x1_range;
    double y1_range;
    double x2_range;
    double y2_range;

    class LMP *lmp_;
    class BTNode *node_;

    BTSchedWord *_sched_word;
    TxBuffer **_txBuffer;
    int maxNumTxBuffer;
    int numTxBuffer;
    TxBuffer *activeBuffer;

  private:
    static Baseband *_chain;	// used to chain all BB together
    Baseband *_next;
    int _started;
    int _in_transmit;
    int _in_receiving;
    // int _occup_slots;
    Packet *_rxPkt;
    clk_t clke_;		// for paging
    clk_t clkf_;		// frozed clkn reading
    TrainType train_;

    BTChannelUseSet _usedChSet;

    double X_;
    double Y_;
    double Z_;
    // double speed_;
    double dX_;
    double dY_;
    // double dZ_;

    double _dXperTick;		// det_X_per_Tick  -- update pos every tick
    double _dYperTick;		// det_Y_per_Tick

    double _lastRecvT;
    double _lastclkt;

    int _txSlot;
    int _txClk;

    int _componentId;

    /* mapping lt_addr to TxBuffer and Link and bd_addr */
    TxBuffer **_lt_table;
    int _lt_table_len;
    TxBuffer *_lt_sco_table[6];

    // double _sco_slot_start_time;
    SCOTDDState _sco_state;

    BBState _state;
    BBState _stable_state;	// STANDBY or CONNECTION

    /* used to control the timing to increase 'slave_rsps_count' */
    clk_t slave_rsps_count_incr_slot;

    int master_rsps_count;	// used for fh kernel
    int slave_rsps_count;	// used for fh kernel
    int inquiry_rsps_count;	// used for fh kernel

    int suspendClkn_;

    // static PacketQueue _interfQ[79];
    // static BTInterferQue _interfQ[79];
    static BTInterferQue *_interfQ;

    void _init();
    int sched(int, BTSchedWord *);
    int trySuspendClknSucc();
    int wakeupClkn();

    // Used for checking connectivity.  If there is only a single
    // Componenet of the Graph, all devices are connected.
    void clearConnMark() { _componentId = -1; }
    int getComponentId() { return _componentId; }
    void setComponentId(int c) { _componentId = c; }

    inline void purgeInterferenceQ(int fs);
    inline int tranceiveSlots(hdr_bt *bh);
    inline int collision(hdr_bt * bh);
    void _slave_reply(hdr_bt * bh);
    void handle_slaveresp();
    inline int ltAddrIsValid(uchar ltaddr);
    inline int recvTimingIsOk(double, double, double *);
    int comp_clkoffset(double timi, int *clkoffset, int *sltoffset);

    inline void change_train() {
	// fprintf(stdout, "%d trainchange from %d ", bd_addr_, train_);
	train_ = (train_ == Train_A ? Train_B : Train_A);
	// fprintf(stdout, "to :%d\n", train_);
    }
    inline clk_t _interlaceClk(clk_t);
    void handle_inquiry_response(hdr_bt *);
    Packet *_allocPacket(FH_sequence_type fs, bd_addr_t addr, clk_t clk,
			 bd_addr_t recv, hdr_bt::packet_type);
    inline Packet *genPollPacket(FH_sequence_type fs, bd_addr_t addr, clk_t clk,
			  bd_addr_t recv);
    inline Packet *genNullPacket(FH_sequence_type fs, bd_addr_t addr, clk_t clk,
			  bd_addr_t recv);
    inline Packet *genIdPacket(FH_sequence_type fs, bd_addr_t addr, clk_t clk,
			bd_addr_t recv);
    Packet *genFHSPacket(FH_sequence_type fs, bd_addr_t addr, clk_t clk,
			 bd_addr_t myaddr, int am, bd_addr_t recv);
    void _transmit(Packet *);
    int FH_kernel(clk_t clk, clk_t clkf, FH_sequence_type FH_seq,
		  bd_addr_t addr);
};

#endif				// __ns_baseband_h__
