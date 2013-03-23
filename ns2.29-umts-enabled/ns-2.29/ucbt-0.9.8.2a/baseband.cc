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

/*
 *  baseband.cc
 */

/*   --LIMITS: 
 * 1. clock wrap around is not handled.  -- not necessary since clk is small.
 * 2. Park/unpark is not completed.  -- no plan to complete it.
 * 3. RS take over is not completed. -- need to be complete.
 * 4. GO/STOP bit is not used. -- No queue at receiver side to make use of it.
 * 5. eSCO is not implemented. -- need to be complete.
 */

#include "baseband.h"
#include "lmp.h"
#include "lmp-piconet.h"
#include "bt-stat.h"
#include "bt-node.h"
#include "bt-channel.h"
#include "stdio.h"
#include "random.h"
#include "wireless-phy.h"
#include "scat-form.h"

#define DUMPSYNBYGOD
#define PRINTRESYN
// #define SEQ_8BIT
// #define PRINT_RECV_TIMING_MISMATCH
// #define PRINT_TPOLL_BKOFF
// #define PRINT_SUSPEND_TXBUF


// #define DUMPTRX
#ifdef DUMPTRX
#define DUMP_TRX_ST fprintf(stdout, "%d %f %s st:%d\n", bd_addr_, now, \
				__FUNCTION__, _trx_st)
#define DUMP_TRX_ST1 fprintf(stdout, "%d %s st:%d\n", bd_addr_, \
				__FUNCTION__, _trx_st);
#else
#define DUMP_TRX_ST
#define DUMP_TRX_ST1
#endif

static class BTHeaderClass:public PacketHeaderClass {
  public:
    BTHeaderClass():PacketHeaderClass("PacketHeader/BT", sizeof(hdr_bt)) {
	bind_offset(&hdr_bt::offset_);
    }
} class_bthdr;

static class BasebandClass:public TclClass {
  public:
    BasebandClass():TclClass("Baseband") {}
    TclObject *create(int, const char *const *argv) {
	return new Baseband();
    }
} class_baseband;


//////////////////////////////////////////////////////////
//                                                      //
//                  hdr_bt                              //
//                                                      //
//////////////////////////////////////////////////////////
int hdr_bt::offset_;
int hdr_bt::pidcntr = 0;
const char *const hdr_bt::_pktTypeName[] = {
    "NULL", "POLL", "FHS", "DM1", "DH1", "HV1", "HV2", "HV3", "DV",
    "AUX1", "DM3", "DH3", "EV4", "EV5", "DM5", "DH5", "ID", "EV3",
    "2-DH1", "3-DH1",
    "2-DH3", "3-DH3", "2-DH5", "3-DH5",
    "2-EV3", "3-EV3", "2-EV5", "3-EV5",
    "HLO", "NotSpecified", "Invalid"
};
const char *const hdr_bt::_pktTypeName_s[] = {
    "NU", "PO", "FH", "M1", "H1", "V1", "V2", "V3", "DV", "AU",
    "M3", "H3", "E4", "E5", "M5", "H5", "ID", "E3", "D1", "T1",
    "D3", "T3", "D5", "T5", "F3", "G3", "F5", "G5",
    "HL", "NotSpecified", "Invalid"
};

void hdr_bt::dump()
{
    if (receiver == BD_ADDR_BCAST) {
	printf("B %d:%d-*:%d ", sender, srcTxSlot, dstTxSlot);
    } else {
	printf("B %d:%d-%d:%d ", sender, srcTxSlot, receiver, dstTxSlot);
    }

    // pktType lt ac fs seqn transmitCount size timestamp transId pktId comm
    printf("%s %d %d %.02d %d %d cn:%d %.03d %f %d:%d %s\n",
	   packet_type_str_short(type), lt_addr, ac, fs_, arqn, seqn,
	   transmitCount, size, ts(), pid, seqno, comment());
}

void hdr_bt::dump(FILE * f, char stat, int ad, const char *st)
{
    if (receiver == BD_ADDR_BCAST) {
	fprintf(f, BTPREFIX "%c %d %s %d:%d-*:%d ", stat, ad, st, sender,
		srcTxSlot, dstTxSlot);
    } else {
	fprintf(f, BTPREFIX "%c %d %s %d:%d-%d:%d ", stat, ad, st, sender,
		srcTxSlot, receiver, dstTxSlot);
    }

    // pktType lt ac fs seqn transmitCount size timestamp transId pktId comm
    fprintf(f, "%s %d %d %.02d %d %d c:%d %.03d %f %d:%d %s %d %d\n",
	    packet_type_str_short(type), lt_addr, ac, fs_, arqn, seqn,
	    transmitCount, size, ts(), pid, seqno, comment(), clk,
	    extinfo);
}

void hdr_bt::dump_sf()
{
    // printf(" 0x%x t:%d c:%d l:%d d:%d data: %d %d %d %d\n",
	   // (unsigned int) this,
    printf("  --- t:%d c:%d l:%d d:%d data: %d %d %d %d\n",
	   u.sf.type, u.sf.code, u.sf.length, u.sf.target,
	   u.sf.data[0], u.sf.data[1], u.sf.data[2], u.sf.data[3]);
}

//////////////////////////////////////////////////////////
//                                                      //
//                  BTInterferQue                       //
//                                                      //
//////////////////////////////////////////////////////////
int BTInterferQue::collide(Baseband * bb, hdr_bt * bh)
{
    if (Baseband::collisionDist < 0.00001) {	// Turn it off
	return 0;
    }
    if (_head == _tail) {
	return 0;
    }

    double pst = bh->ts_;
    double pet = bh->ts_ + bh->txtime();

    // purge old entries
    for (;;) {
	if (_q[_tail].et + MAX_PKT_TX_TIME < pet) {
	    if (++_tail == BTInterferQueMaxLen) {
		_tail = 0;
	    }
	    if (_head == _tail) {
		return 0;
	    }
	} else {
	    break;
	}
    }

    int ind = _tail;
    while (ind != _head) {
	if (bh->sender != _q[ind].sender_
	    && ((pst >= _q[ind].st && pst < _q[ind].et)
		|| (pet > _q[ind].st && pet <= _q[ind].et))) {
	    double interfsrc = bb->distance(_q[ind].X_, _q[ind].Y_);
	    double pktsrc = bb->distance(bh->X_, bh->Y_);
	    // if (interfsrc < Baseband::radioRange || interfsrc < pktsrc * 2) {
	    if (interfsrc < Baseband::collisionDist) {
		if (bb->trace_all_rx_ || bb->trace_me_rx_) {
		    bh->dump(BtStat::log, BTCOLLISONPREFIX, bb->bd_addr_,
			     bb->state_str_s());
		    fprintf(BtStat::log,
			    "  %d i:%d (%f %f) i-d:%f p-d:%f\n",
			    bb->bd_addr_, _q[ind].sender_, _q[ind].st,
			    _q[ind].et, interfsrc, pktsrc);
		}
		return 1;
	    }
	}

	if (++ind == BTInterferQueMaxLen) {
	    ind = 0;
	}
    }

    return 0;
}

//////////////////////////////////////////////////////////
//                                                      //
//                  BTChannelUseSet                     //
//                                                      //
//////////////////////////////////////////////////////////
BTChannelUseSet::BTChannelUseSet(uint16_t map_h, uint32_t map_m,
				 uint32_t map_l)
{
    _u.s.map_h = ntohs(map_h);
    _u.s.map_m = ntohl(map_m);
    _u.s.map_l = ntohl(map_l);
    _expand();
}

void BTChannelUseSet::reset()
{
    for (int i = 0; i < 9; i++) {
	_u.map[i] = 0xFF;
    }
    _u.map[9] = 0x7F;
    _expand();
}

void BTChannelUseSet::importMapByHexString(const char *ch)
{
    int len = strlen((const char *) ch);
    if (len == 22 && ch[0] == '0' && (ch[1] == 'x' || ch[1] == 'X')) {
	ch += 2;
    } else if (len != 20) {
	abort();
    }

    for (int i = 0; i < 10; i++) {
	if (ch[2 * i] >= '0' && ch[2 * i] <= '9') {
	    _u.map[9 - i] = ch[2 * i] - '0';
	} else if (ch[2 * i] >= 'A' && ch[2 * i] <= 'F') {
	    _u.map[9 - i] = ch[2 * i] - 'A' + 10;
	} else if (ch[2 * i] >= 'a' && ch[2 * i] <= 'f') {
	    _u.map[9 - i] = ch[2 * i] - 'a' + 10;
	} else {
	    abort();
	}
	_u.map[9 - i] <<= 4;

	if (ch[2 * i + 1] >= '0' && ch[2 * i + 1] <= '9') {
	    _u.map[9 - i] += (ch[2 * i + 1] - '0');
	} else if (ch[2 * i + 1] >= 'A' && ch[2 * i + 1] <= 'F') {
	    _u.map[9 - i] += (ch[2 * i + 1] - 'A' + 10);
	} else if (ch[2 * i + 1] >= 'a' && ch[2 * i + 1] <= 'f') {
	    _u.map[9 - i] += (ch[2 * i + 1] - 'a' + 10);
	} else {
	    abort();
	}
    }
    _expand();
}

void BTChannelUseSet::importMap(uchar * map)
{
    memcpy(_u.map, map, 10);
    _expand();
}

uchar *BTChannelUseSet::exportMap(uchar * map)
{
    if (map) {
	memcpy(map, _u.map, 10);
	return map;
    } else {
	return _u.map;
    }
}

char *BTChannelUseSet::toHexString(char *s)
{
    char *ret = s;
    if (!s) {
	ret = new char[21];
    }
    sprintf(ret, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
	    _u.map[9], _u.map[8], _u.map[7], _u.map[6], _u.map[5],
	    _u.map[4], _u.map[3], _u.map[2], _u.map[1], _u.map[0]);
    ret[20] = '\0';
    return ret;
}

void BTChannelUseSet::_expand()
{

    int numOddCh = 0;
    char oddCh[40];
    int i, j;

    _numUsedCh = 0;
    _u.map[9] &= 0x7F;

    for (i = 0; i < 10; i++) {
	for (j = 0; j < 8; j++) {
	    if ((_u.map[i] >> j) & 0x01) {
		if ((j & 0x0001)) {	// odd
		    oddCh[numOddCh++] = i * 8 + j;
		} else {
		    _usedCh[_numUsedCh++] = i * 8 + j;
		}
		_notUsedCh[i * 8 + j] = 0;
	    } else {
		_notUsedCh[i * 8 + j] = 1;
	    }
	}
    }

    memcpy(&_usedCh[_numUsedCh], oddCh, sizeof(char) * numOddCh);
    _numUsedCh += numOddCh;
}


//////////////////////////////////////////////////////////
//                                                      //
//                      TxBuffer                        //
//                                                      //
//////////////////////////////////////////////////////////
TxBuffer::TxBuffer(class Baseband * bb, class LMPLink * l, int s)
:prioClass(Low), _current(NULL), _next(NULL), bb_(bb), _link(l), _slot(s),
_dstTxSlot(0),
_send_ack(0), _seqn_rx(0), _seqn_tx(0), _arqn(0),
afhEnabled_(0), _scoStarted(0),
_t_index(-1), _t_cntr(0),
_lastPktRecvT(0), _lastDataPktRecvT(0), _lastSynT(0),
_ttlFails(0), _fails(0), _ttlSent(0), _nullCntr(0), _suspended(0),
T_poll_max(Baseband::T_poll_max),
T_poll_default(Baseband::T_poll_default),
T_poll(Baseband::T_poll_default),
lastPollClk(0), lastPollClk_highclass(0), prio(PRIO_PANU)
{
    bd_addr_ = bb->bd_addr_;
    if (l) {
	remote_bd_addr = l->remote->bd_addr;
    }
    txType = rxType = hdr_bt::Invalid;
    _hasData = 1;
    ttlSlots_ = 0;
}


// return TxBuffer type: 0 - ACL   1 - SCO   2 - baseband (BBSIG)
// current implementation doesn't put baseband signaling pkt into TxBuffer
// So, it never returns 2.
int TxBuffer::type()
{
    return (_link ? _link->type() : BBSIG);
}

// #define DEBUGGHOSTPKT

#ifdef DEBUGGHOSTPKT
#define SHOWBUFFER printf("%d:%d %s cur: %x next: %x\n", bd_addr_, \
		_slot, __FUNCTION__, (uint32_t)_current, (uint32_t)_next)
#else
#define SHOWBUFFER
#endif

void TxBuffer::reset_seqn()
{
    // Inversed before a new CRC pkt is transmitted.
    // Therefore the first CRC pkt carrying seqn 1 as specs says.
    _seqn_tx = 0;

    // The first incoming CRC pkt carrying seqn 1, which is 
    // different from this value, is identified as a new pkt.
    _seqn_rx = 0;

    // ACK
    _arqn = 0;

    if (_current) {
	Packet::free(_current);
	_current = NULL;
    }
    if (_next) {
	Packet::free(_next);
	_next = NULL;
    }
}

///////////////////////////////////////////////////////////////////////
//   Note about ACK/NAK                                              //
//-------------------------------------------------------------------//
// Facts in current implentment
// 1. A CRC pkt is always retranmitted until an ACK is received, or a flush
//    is performed. No other pkts are in between.
// 2. If a non-CRC pkt is received, all previous CRC pkts are ACK'd.
// 3. Therefore, upon receiving a non-CRC pkt, NAK can be set, although
//    spec1.1 says ACK is not changed.
// 4. A slave always gets a pkt prior to sending something.  It cannot 
//    detect pkt loss.  If loss happens, it thinks it is not polled.
//    This may not true if a better collison detection module is used.
//      -- we may work on this such that the slave may see a correct hdr
//         instead of the whole packet, so it know it is polled. TODO
// 5. A master can detect pkt loss, if it receives nothing in the 
//    following slot.

// Current ACK/NAK scheme in ucbt
// 1. Upon receiving a CRC pkt sucessfully, ACK is set, otherwise NAK is set.
// 2. A master is check the possible loss in receiving slot to set NAK.
// 3. Retransmission will update ACK.
// 
// **** This model can be refined if the ErrMode differentiates header error
//      and payload error.

// transmit a packet in TxBuffer
// if _suspended == 1, only POLL pkts are transmitted.
// In case that two ends of the link are not syn'd, eg. one is awake 
// while the other is in Hold/Sniff/Pack state, the txBuffer is marked 
// as suspended, so that Data pkt is not flushed.
///////////////////////////////////////////////////////////////////////

// This function is called when BB decides to transmit a pkt from
// this TxBuffer, i.e. the timing is aligned to the beginning of a slot.
Packet *TxBuffer::transmit()
{
    hdr_bt *bh;

    // The peer is not present.  Sends POLL pkt instead.
    // Note this behavior is not defined in the spec.  Spec2.0 does suggest
    // in case if possbile out of sync, MA may transmit one-slot packet so
    // slaves can re-sync more easily.
    if (_suspended) {
#ifdef PRINT_SUSPEND_TXBUF
	fprintf(BtStat::log, "%d:%d TxBuffer is suspended.\n",
		bb_->bd_addr_, _slot);
#endif
	Packet *p = bb_->stamp(_link->genPollPkt(), this);
	bb_->_polled_lt_addr = HDR_BT(p)->lt_addr;
	HDR_BT(p)->nokeep = 1;

	return p;		// XXX When to free p ??
    }

    SHOWBUFFER;
    // check if a flush is needed, or a new pkt should be transmitted.
    if (_current) {
	bh = HDR_BT(_current);

#ifdef BTDEBUG
	if (bh->comment_ == NULL) {	// debug purpose
	    fprintf(stderr,
		    "*%d %f ghost packages in TxBuffer::transmit()\n",
		    bb_->bd_addr_, Scheduler::instance().clock());
	    bh->dump(stderr, BTTXPREFIX, bb_->bd_addr_, bb_->state_str_s());	// "t "
	    _current = 0;
	}
#endif

	if (bh->bcast && bh->transmitCount >=
	    _link->lmp_->_num_Broadcast_Retran) {
	    switch_reg();
	} else if (bh->transmitCount > 0 && !bh->isCRCPkt()) {
	    switch_reg();
	} else if (bh->transmitCount >= _link->lmp_->_max_num_retran) {
	    flush();
	}
    }
    // try to get a data pkt to send if _current is NULL.
    if (!_current) {
	_current = _next;
	_link->callback();	// get a packet from lmp if it has one.
    }
    // send a ACK if it can not be piggy-backed.
    if (!_current && _send_ack) {
	if (bb_->isMaster()) {
	    // Master sends POLL pkt including ACK.
	    // We let the master always send POLL, so a reply is expected.
	    _link->sendPoll(1);
	} else {
	    _link->sendNull();
	}
    }
    _send_ack = 0;

    // master sends a POLL packet on ACL link if it has nothing to send
    if (!_current && bb_->_polling && type() == ACL &&
	_slot >= Baseband::MinTxBufferSlot) {
	_link->sendPoll();	// NAK
    }
    // I really have nothing to send at this moment.
    if (!_current) {
	return NULL;
    }
    // Finally, a packet is ready to transmit.
    bh = HDR_BT(_current);
#ifdef BTDEBUG
    if (bh->comment_ == NULL) {	// It should not happen.
	fprintf(stderr,
		"**%d:%d %x %f ghost packages in TxBuffer::transmit()\n",
		bb_->bd_addr_, _slot, (uint32_t) _current,
		Scheduler::instance().clock());
	bh->dump(stderr, BTTXPREFIX, bb_->bd_addr_, bb_->state_str_s());	// "t "
	_current = NULL;
	return NULL;
    }
#endif

    bb_->stamp(_current, this);
    if (bh->transmitCount == 1) {	// new packet
	if (bh->ph.l_ch == L_CH_L2CAP_CONT && bh->ph.length == 0) {
	    // a new flush pkt. _seqn_tx is not changed.
	    bh->seqn = _seqn_tx;
	} else if (bh->isCRCPkt()) {
#ifdef SEQ_8BIT
	    _seqn_tx++;
#else
	    _seqn_tx = !_seqn_tx;
#endif
	    bh->seqn = _seqn_tx;
	}
    }
    // Check if the master receives successfully at the slave-to-master slot.
    // (_arqn is set to 0(NAK) when receiving a non-CRC pkt.)
    if (bb_->isMaster()) {
	if (_fails > 0) {
	    _arqn = 0;
	}
    }
    bh->arqn = _arqn;

    bb_->_polled_lt_addr = HDR_BT(_current)->lt_addr;

    // _fails is basically the transmission counter. It is reseted
    // to 0 upon a successful receiption.
    if (!bh->bcast) {
	_fails++;
    }
    _ttlSent++;
    if (type() != SCO) {
	if (_fails > FAILS_SCHRED_FOR_TPOLL_DBL) {
	    T_poll += T_poll;
	    if (T_poll > T_poll_max) {
		T_poll = T_poll_max;
	    }
	}
	if (_fails >= _link->lmp_->failTriggerSchred_) {
	    _link->failTrigger();
	}
    }

    return _current;
}

// return: 1 -- p will go to upper layer,
//         0 -- p will be dropped.
int TxBuffer::handle_recv(Packet * p)
{
    // Scheduler & s = Scheduler::instance();
    SHOWBUFFER;
    _lastPktRecvT = bb_->clk_;
    hdr_bt *bh = HDR_BT(p);
    if (bh->txBuffer->dstTxSlot() == 0) {
	bh->txBuffer->_dstTxSlot = bh->srcTxSlot;
    }
    rxType = bh->type;

    if (type() == SCO) {
	if (!_scoStarted) {
	    if (bh->scoOnly()) {
		_scoStarted = 1;
	    } else {
		return 0;
	    }
	}
	if (bh->isScoPkt()) {
	    if (bh->type == hdr_bt::DV) {
		fprintf
		    (stderr,
		     "\n*** DV packet can not handled by the simulator.\n");
		exit(1);
	    }
	    _ttlFails += (_fails > 1 ? _fails - 1 : 0);
	    _fails = 0;
	    return 1;
	} else {		// non-SCO pkt
	    return 0;
	}
    } else if (type() != ACL) {
	return 0;
    }
    // when in SNIFF mode, notify a incoming pkt.
    if (link()->_in_sniff_attempt) {
	link()->recvd_in_sniff_attempt(p);
    }

    resume();			// clear the _suspended flag if set.
    if (!bb_->isMaster()) {
	bb_->polled(bh->txBuffer->slot());	// turn on tx in schedWord
	update_T_poll();	// Estimating T_poll using by link mode negotiation.

    } else {

	// receive a Poll/Null pair. put the link into sleep.
	if (bh->type == hdr_bt::Null && txType == hdr_bt::Poll) {
	    _hasData = 0;
	} else {
	    _hasData = 1;
	}

	deficit_ -= hdr_bt::slot_num(bh->type);
	ttlSlots_ += hdr_bt::slot_num(bh->type);
    }

    _ttlFails += (_fails > 1 ? _fails - 1 : 0);
    _fails = 0;

    if (bh->type != hdr_bt::Poll && bh->type != hdr_bt::Null) {
	_lastDataPktRecvT = bb_->clk_;
	_nullCntr = 0;
	T_poll = T_poll_default;
    } else {
	_nullCntr++;
	if (!_hasData && bb_->isMaster()) {
	    T_poll += T_poll;
	    if (T_poll > T_poll_max) {
		T_poll = T_poll_max;
	    }
#ifdef PRINT_TPOLL_BKOFF
	    fprintf(BtStat::log,
		    "Receive POLL/NULL %d times, back off %d\n", _nullCntr,
		    T_poll);
#endif
	}
	if (_nullCntr >= _link->lmp_->nullTriggerSchred_) {
	    _link->nullTrigger();
	}
    }

    if (bh->isCRCAclPkt()) {
	_arqn = 1;		// send ACK
	uchar seqn_expect;

#ifdef SEQ_8BIT
	if (bh->transmitCount == 1) {	// Debug purpose.
	    if (bh->seqn != (uchar) (_seqn_rx + 1)) {
		fprintf(stderr,
			"seqn wrong: %d expect %d fixit by cheating.\n",
			bh->seqn, (uchar) _seqn_rx + 1);
		_seqn_rx = bh->seqn - 1;
	    }
	}
	if (bh->seqn != (uchar) (_seqn_rx + 1)) {	// retransmission
	    seqn_expect = _seqn_rx + 1;
#else
	if (bh->seqn == _seqn_rx) {	// retransmission
	    seqn_expect = (_seqn_rx ? 0 : 1);
#endif
	    fprintf(BtStat::log, "Duplicated pkt: seqn %d, expect %d ",
		    bh->seqn, seqn_expect);
	    bh->dump(BtStat::log, BTRXPREFIX, bb_->bd_addr_,
		     bb_->state_str_s());

	    link()->sendACK();
	    ack(bh->arqn);
	    return 0;
	} else {
	    _seqn_rx = bh->seqn;	// ie. SEQN_old
	}
    } else {
	_arqn = 0;
    }

    ack(bh->arqn);

    if (bh->isCRCAclPkt() || bh->type == hdr_bt::Poll) {
	link()->sendACK();
    }

    return ((bh->isCRCAclPkt() || bh->type == hdr_bt::AUX1) ? 1 : 0);
}

// slave measure T_poll in last TPOLLUPDATE_PKTNUM packets.
void TxBuffer::update_T_poll()
{
    int i, j;
    int32_t sum = 0;

    if (++_t_index == TPOLLUPDATE_PKTNUM) {
	// _t_index initiated to -1 or TPOLLUPDATE_PKTNUM - 1
	_t_index = 0;
    }

    _T[_t_index] = bb_->clk_;
    if (_t_cntr < TPOLLUPDATE_PKTNUM - 1) {
	if (_t_cntr > 0) {
	    for (i = 0; i < _t_cntr; i++) {
		sum += _T[i + 1] - _T[i];
	    }
	    _link->T_poll = sum / _t_cntr / 2;
	}
	_t_cntr++;
    } else {			// _t_cntr = TPOLLUPDATE_PKTNUM - 1
	// _T[] can store TPOLLUPDATE_PKTNUM - 1 newest T_poll's.
	j = _t_index;
	for (i = 0; i < _t_cntr; i++) {
	    if (j == 0) {
		sum += _T[0] - _T[TPOLLUPDATE_PKTNUM - 1];
		j = 7;
	    } else {
		sum += _T[j] - _T[j - 1];
		j--;
	    }
	}
	_link->T_poll = sum / _t_cntr / 2;
    }
}

// ack() implements ARQ
void TxBuffer::ack(int a)
{
    if (a) {
	// check if a ACK for detach request arrived.
	if (_current
	    && HDR_BT(_current)->u.lmpcmd.opcode == LMP::LMP_DETACH) {
	    link()->recv_detach_ack();
	}
	switch_reg();
    }
}

// LMP puts a packet into TxBuffer
// return 0 on failure -- Buffer is full.
//        1 on success
int TxBuffer::push(Packet * p)
{
    SHOWBUFFER;
    if (!available()) {
	return 0;
    }

    if (_current == NULL && _next == NULL) {
	_current = p;
    } else if (_next == NULL) {
	_next = p;
    } else {
	_current = _next;
	_next = p;
    }
    return 1;
}

// 1. discard the current CRC pkt
// 2. send L2CAP cont pkt with lengh 0
// 3. send new L2CAP pkt upon receiving a ACK
void TxBuffer::flush()
{
    SHOWBUFFER;
    fprintf(BtStat::log, "# %d:%d flush ", bd_addr_, _slot);
    if (_current) {
	HDR_BT(_current)->dump(BtStat::log, BTRXPREFIX, bb_->bd_addr_,
			       bb_->state_str_s());
	Packet::free(_current);
    } else {
	fprintf(BtStat::log, "_current is NULL.\n");
    }

    // discard L2CAP cont pkts.
    // if _next is a L2CAP cont pkt, discard it.
    // discard L2CAP cont pkt in link l2capQ.
    //
    // if _next is a L2CAP start pkt, it will be kept.
    //
    // Note that spec1.1 says if _next is a new L2CAP start pkt,
    // it can be transmited immediately. ie. set _current to it.
    // Here a L2CAP Cont Pkt with length 0 is always transmitted.
    if (!_next) {
	_link->flushL2CAPPkt();
    } else if (HDR_BT(_next)->ph.l_ch == L_CH_L2CAP_CONT) {
	Packet::free(_next);
	_link->flushL2CAPPkt();
	_next = NULL;
    }
    // generate a L2CAP Cont Pkt with length 0.
    _current = Packet::alloc();
    hdr_bt *bh = HDR_BT(_current);
    bh->pid = hdr_bt::pidcntr++;
    bh->type = hdr_bt::DM1;
    bh->ph.l_ch = L_CH_L2CAP_CONT;
    bh->ph.length = 0;
    bh->size = hdr_bt::packet_size(bh->type, bh->ph.length);
    bh->receiver = remote_bd_addr;
    bh->comment("FL");
}

void TxBuffer::switch_reg()
{
    SHOWBUFFER;
    if (_current) {
	if (_current->uid_ > 0) {	// Shouldn't happen
	    fprintf(BtStat::logstat, "%d _current scheduled to be freed. ",
		    bd_addr_);
	    HDR_BT(_current)->dump(BtStat::logstat, 'F', bb_->bd_addr_,
				   "XXX");
	    Scheduler::instance().cancel(_current);
	}
	Packet::free(_current);
    }
    _current = _next;
    _next = NULL;
    _link->callback();
}

int TxBuffer::hasBcast()
{
    if (_current && HDR_BT(_current)->transmitCount >=
	bb_->lmp_->_num_Broadcast_Retran) {
	switch_reg();
    }
    return (_current ? 1 : 0);
}

// Used by the master to implement link scheduling.
// The master decides if T_poll should be increased.
int TxBuffer::hasDataPkt()
{
    return _hasData && !_suspended;
#if 0
    return !_suspended && (_nullCntr < 2 ||
			   (_current
			    && HDR_BT(_current)->type != hdr_bt::Poll));
#endif
}


//////////////////////////////////////////////////////////
//                      BTSchedWord                     //
//////////////////////////////////////////////////////////

// NOTE: SchedWord control is almost out of date.

// SchedWord as a control word for bb set by lmp.  Think of it as one of
// the methods to implement timing control over bb.

// For baseband signaling (page, inq, ..., etc), 
//      Master: send at even slots, receive at odd slots, like srsrsrsrsr
//      Slave:  allow to send at arbitrary time, since it has to align 
//              with Master's clock.  like ssssssssss.

BTSchedWord::BTSchedWord(bool b):in_use(0), len(2)
{
    word = new uint8_t[len];
    if (b) {			// can send bb signals in any slot
	word[0] = word[1] = Baseband::BasebandSlot;
    } else {			// cannot send
	word[0] = word[1] = Baseband::RecvSlot;
    }
}

// copy constructor
BTSchedWord::BTSchedWord(BTSchedWord & b):in_use(0), len(b.len)
{
    word = new uint8_t[len];
    memcpy(word, b.word, sizeof(uint8_t) * len);
}

BTSchedWord::BTSchedWord(BTSchedWord & b, int l):in_use(0), len(l)
{
    word = new uint8_t[len];
    int s = (len < b.len ? len : b.len);
    memcpy(word, b.word, sizeof(uint8_t) * s);
    for (int i = b.len; i < len; i++) {
	word[i++] = Baseband::BasebandSlot;
	word[i] = Baseband::RecvSlot;
    }
}

// generate schedule word for the master confirming to Bluetooth BB specs. 
// i.e. send at even slots, receive at odd slots.
BTSchedWord::BTSchedWord(int l):in_use(0), len(l)
{
    word = new uint8_t[len];
    for (int i = 0; i < len; i++) {
	if (i % 2 < 1) {
	    word[i] = Baseband::BasebandSlot;
	} else {
	    word[i] = Baseband::RecvSlot;
	}
    }
}

// generate a default schedule word for normal pico operation
// drdrdr/rrrrrr
BTSchedWord::BTSchedWord(int l, int master, int dummy):in_use(1), len(l)
{
    word = new uint8_t[len];

    for (int i = 0; i < len; i++) {
	if (master && (i % 2 == 0)) {
	    word[i] = Baseband::DynamicSlot;;
	} else {
	    word[i] = Baseband::RecvSlot;
	}
    }
}

BTSchedWord::BTSchedWord(int l, bool b):in_use(0), len(l)
{
    word = new uint8_t[len];
    for (int i = 0; i < len; i++) {
	if (b) {
	    word[i] = Baseband::BasebandSlot;
	} else {
	    word[i] = Baseband::RecvSlot;
	}
    }
}

void BTSchedWord::dump(FILE *out)
{
    if (!out) {
	out = BtStat::logstat;
    }
    fprintf(out, "BTSchedWord(%d):", len);
    if (len == 1) {
	fprintf(out, "ooops, len = 1\n");
    }
    for (int i = 0; i < len; i++) {
	fprintf(out, " %d", word[i]);
    }
    fprintf(out, "\n");
}


//////////////////////////////////////////////////////////
//                      Baseband                        //
//////////////////////////////////////////////////////////

// Chain all baseband instances together to broadcast packets.
Baseband *Baseband::_chain = NULL;

BTLossMod *Baseband::lossmod = NULL;

int Baseband::trace_all_tx_ = 1;
int Baseband::trace_all_rx_ = 1;
int Baseband::trace_all_poll_ = 1;
int Baseband::trace_all_null_ = 1;
int Baseband::trace_all_in_air_ = 0;
int Baseband::trace_all_link_ = 1;
int Baseband::trace_all_stat_ = 1;
int Baseband::trace_all_awmmf_ = 0;

int32_t Baseband::T_poll_max = MAX_T_POLL;
int32_t Baseband::T_poll_default = T_POLL_DEFAULT;

BTInterferQue *Baseband::_interfQ = NULL;
int Baseband::check_wifi_ = 0;
double Baseband::collisionDist = BT_INTERFERE_DISTANCE;
double Baseband::radioRange = BT_EFF_DISTANCE;
double Baseband::toroidal_x_ = -1.0;
double Baseband::toroidal_y_ = -1.0;
int Baseband::useSynByGod_ = 0;

Baseband::Baseband()
:
clkn_handler(this), clk_handler(this), resync_handler(this),
_trxtoTimer(this),
txHandler(this), recvHandler(this),
recvHandlerLastbit(this), slaveSendnonConnHandler(this)
{
    bind("T_w_inquiry_scan_", &T_w_inquiry_scan_);
    bind("T_inquiry_scan_", &T_inquiry_scan_);
    bind("Inquiry_Scan_Type_", &Inquiry_Scan_Type_);

    bind("T_w_page_scan_", &T_w_page_scan_);
    bind("T_page_scan_", &T_page_scan_);
    bind("Page_Scan_Type_", &Page_Scan_Type_);

    bind("ver_", &ver_);

    bind("pageTO_", &pageTO_);
    bind("backoffParam_", &backoffParam);
    bind("SR_mode_", &SR_mode_);
    bind("N_page_", &N_page_);
    bind("N_inquiry_", &N_inquiry_);

    bind("driftType_", &driftType_);
    bind("clkdrift_", &clkdrift_);

    // bind("pmEnabled_", &pmEnabled_);
    bind("energyMin_", &energyMin_);
    bind("energy_", &energy_);
    bind("activeEnrgConRate_", &activeEnrgConRate_);
    bind("activeTime_", &activeTime_);
    bind("startTime_", &startTime_);
    bind("numTurnOn_", &numTurnOn_);
    bind("warmUpTime_", &warmUpTime_);


    x1_range = 0;
    y1_range = 0;
    x2_range = BT_RANGE;
    y2_range = BT_RANGE;

    X_ = Random::uniform() * (x2_range - x1_range) + x1_range;
    Y_ = Random::uniform() * (y2_range - y1_range) + y1_range;
    Z_ = 0;
    // speed_ = 0;
    _dXperTick = 0;		// speed = 0
    _dYperTick = 0;

#ifdef BTDEBUG
    // fprintf(stderr, "Initial position: (%f, %f)\n", X_, Y_);
#endif

    ver_ = BTSPEC;

    _init();

    if (_chain == NULL) {
	_chain = this;
	_chain->_next = _chain;
	lossmod = new BTLossMod();
	BtStat::init_logfiles();
	_interfQ = new BTInterferQue[79];
    } else {
	_next = _chain->_next;
	_chain->_next = this;
	_chain = this;
    }
    linkSched = new BTDRR(this);
}

void Baseband::_init()
{
    isMaster_ = 0;
    clk_ = 0;
    _receiver = 0;
    clke_ = 0;
    clkf_ = 0;

    _reset_as = 1;		// reset schedWord after sending, used by slaves.

    _insleep = 0;
    trace_me_tx_ = 0;
    trace_me_rx_ = 0;
    trace_me_poll_ = 0;
    trace_me_null_ = 0;
    trace_me_in_air_ = 0;
    trace_me_stat_ = 0;
    trace_me_link_ = 0;
    trace_me_awmmf_ = 0;
    _started = 0;

    inRS_ = 0;

    activeBuffer = NULL;	// txBuffer in tranceiving.
    _in_transmit = 0;
    _in_receiving = 0;
    // _occup_slots = 0;
    _rxPkt = 0;

    _afhEnabled = 0;
    Page_Scan_Type_ = Standard;
    Inquiry_Scan_Type_ = Standard;

    _sched_word = 0;
    maxNumTxBuffer = 16;
    numTxBuffer = MinTxBufferSlot;

    drift_clk = 0;

    suspendClkn_ = 0;

    N_page_ = NPAGE;
    pageTO_ = PAGETO;
    SR_mode_ = 1;

    T_w_page_scan_ = TWPAGESCAN;
    T_page_scan_ = TPAGESCAN;
    connectable = 0;

    slave_rsps_count_incr_slot = 5;	// set false
    slave_rsps_count = 0;
    master_rsps_count = 0;

    _sco_state = SCO_IDLE;

    pagerespTO = PAGERESPTO;
    newconnectionTO = NEWCONNECTIONTO;
    pagerespTimer = 0;

    T_w_inquiry_scan_ = TWINQUIRYSCAN;
    T_inquiry_scan_ = TINQUIRYSCAN;
    discoverable = 0;
    inBackoff = 0;
    pagescan_after_inqscan = 0;
    page_after_inq = 0;

    N_inquiry_ = NINQUIRY;
    inqrespTO = INQRESPTO;
    backoffParam = BACKOFF;

    driftType_ = BT_DRIFT_OFF;
    clkdrift_ = 0;

    SlotTime = BTSlotTime;
    Tick = SlotTime / 2;

    _state = _stable_state = STANDBY;

    _lastRecvT = 0;

    _polled_lt_addr = 0;
    _poll_time = -1;

    _prioLevelReq = 0;

    _sched_word = new BTSchedWord(2);	// set a default sched_word
    _txBuffer = new TxBuffer *[maxNumTxBuffer];
    memset(_txBuffer, 0, sizeof(TxBuffer *) * maxNumTxBuffer);
    _txBuffer[BcastSlot] = new TxBuffer(this, 0, BcastSlot);
    _txBuffer[BeaconSlot] = new TxBuffer(this, 0, BeaconSlot);

    _resyncWind = 625E-6;
    _resyncWindSlotNum = 2;
    _resyncCntr = 0;
    _maxResync = 9;
    _resyncWindStartT = -1;

    _active_t = 0;		// has to be 0
    energyMin_ = 0.1;
    energy_ = 1;
    activeEnrgConRate_ = 1E-4;
    activeTime_ = 0;
    startTime_ = 0;
    numTurnOn_ = 0;
    trxTurnarndTime_ = 0.00022;	// 220 us
    warmUpTime_ = 0.0002;	// 200 us

    _trx_st = TRX_OFF;
    _lastclkt = -1;

    t_clkn_00 = 0;
    t_clk_00 = 0;

    for (int i = 0; i < 6; i++) {
	_lt_sco_table[i] = 0;
    }
}

void Baseband::reset()
{
    for (int i = BcastSlot; i < maxNumTxBuffer; i++) {
	if (_txBuffer[i]) {
	    delete _txBuffer[i];
	}
    }
    delete[]_txBuffer;
    _txBuffer = NULL;
    _init();
}

void Baseband::reset_all()
{
    Baseband *wk = this;
    do {
	wk->reset();
    } while ((wk = wk->_next) != this);
}

void Baseband::on()
{
    double slotoffset = 0;
    if (!_started) {
	Scheduler::instance().schedule(&clkn_handler, &clkn_ev,
				       slotoffset);
	_started = 1;
	clkn_ = (clkn_ & 0xfffffffc) + 3;
	t_clkn_00 = clkn_ev.time_ - SlotTime * 2;
    }
    // MAX_SLOT_DRIFT = 20E-6
    // BTSlotTime = 625E-6
    double maxdriftperslot = MAX_SLOT_DRIFT * BTSlotTime;
    if (driftType_ == BT_DRIFT) {
	SlotTime = Random::uniform(BTSlotTime - maxdriftperslot,
				   BTSlotTime + maxdriftperslot);
    } else if (driftType_ == BT_DRIFT_NORMAL) {
	SlotTime = Random::normal(BTSlotTime, BT_DRIFT_NORMAL_STD);
	while (SlotTime > BTSlotTime + maxdriftperslot ||
	       SlotTime < BTSlotTime - maxdriftperslot) {
	    SlotTime = Random::normal(BTSlotTime, BT_DRIFT_NORMAL_STD);
	}
    } else if (driftType_ == BT_DRIFT_USER) {
	SlotTime = BTSlotTime + clkdrift_ * 1E-6 * BTSlotTime;
	fprintf(stderr, "%d clkdrift_ : %d\n", bd_addr_, clkdrift_);
    } else {
	SlotTime = BTSlotTime;
    }
    Tick = SlotTime / 2;
}

void Baseband::turn_on_tx()
{
    Scheduler & s = Scheduler::instance();
    double now = s.clock();

    if (_trxoff_ev.uid_ > 0) {
	s.cancel(&_trxoff_ev);
    }

    if (_trx_st == TX_ON) {
	return;
    }

    DUMP_TRX_ST;

    if (_trx_st != RX_ON) {
	_active_t = now;
	numTurnOn_++;
    }
    _trx_st = TX_ON;
}

void Baseband::turn_on_rx()
{
    Scheduler & s = Scheduler::instance();
    double now = s.clock();

    if (_trxoff_ev.uid_ > 0) {
	s.cancel(&_trxoff_ev);
    }

    if (_trx_st == RX_ON) {
	return;
    }

    DUMP_TRX_ST;

    if (_trx_st != TX_ON) {
	_active_t = now;
	numTurnOn_++;
    }
    _trx_st = RX_ON;
}

void Baseband::turn_off_rx_to()
{
    double now = Scheduler::instance().clock();
    if (_trx_st == TRX_OFF) {
	return;
    }

    DUMP_TRX_ST;

    if (_active_t < 0) {
	fprintf(stderr, "%d %f %s turn off before started.\n",
		bd_addr_, now, __FUNCTION__);
    } else {
	energy_ -= (now - _active_t + warmUpTime_) * activeEnrgConRate_;
	activeTime_ += (now - _active_t);
	if (energy_ < energyMin_) {
	    off();
	}
    }
    _active_t = -1;
    _trx_st = TRX_OFF;
}

void Baseband::turn_off_trx()
{
/*
    if (_trxoff_ev.st != 0) {
	fprintf(stderr, "TRX_off: event:%d is not cleared.\n", _trxoff_ev.st);
    }
*/
    double now = Scheduler::instance().clock();

    if (_trx_st == TRX_OFF) {
	return;
    }

    DUMP_TRX_ST;

    if (_active_t < 0) {
	fprintf(stderr, "%d %f %s turn off before started.\n",
		bd_addr_, now, __FUNCTION__);
    } else {
	energy_ -= (now - _active_t) * activeEnrgConRate_;
	activeTime_ += (now - _active_t);
	if (energy_ < energyMin_) {
	    off();
	}
    }
    _active_t = -1;
    _trx_st = TRX_OFF;
}

void Baseband::turn_off_trx(BTTRXoffEvent * e)
{
#if 0
    if (e->st != _trx_st) {
	fprintf(stderr, "TRX_st doesn't match: cur:%d turnoff:%d\n",
		_trx_st, e->st);
    }
#endif

    DUMP_TRX_ST1;

    turn_off_trx();
    e->clearSt();
}

void Baseband::energyReset()
{
    startTime_ = Scheduler::instance().clock();
    energy_ = 1;
    activeTime_ = 0;
    numTurnOn_ = 0;

    for (int i = MinTxBufferSlot; i < maxNumTxBuffer; i++) {
	if (_txBuffer[i]) {
	    _txBuffer[i]->ttlSlots_ = 0;
	}
    }
}

void Baseband::dumpTtlSlots(FILE * f)
{
    if (!f) {
	f = stdout;
    }
    int ad = bd_addr_;

    fprintf(f, "ttlSlots %d: ", ad);
    for (int i = MinTxBufferSlot; i < maxNumTxBuffer; i++) {
	if (_txBuffer[i]) {
	    fprintf(f, " %d", _txBuffer[i]->ttlSlots_);
	}
    }
    fprintf(f, "\n");
}

void Baseband::dumpEnergy(FILE * f)
{
    int adjnumTurnOn = 0;
    Scheduler & s = Scheduler::instance();
    if (!f) {
	f = stdout;
    }
#if 0
    if (_trx_st != TRX_OFF) {
	trx_st_t st = _trx_st;
	turn_off_trx();
	if (st == TX_ON) {
	    turn_on_tx();
	} else {
	    turn_on_rx();
	}
	adjnumTurnOn = 1;
    }
#endif

    double tt = s.clock() - startTime_;
    int ad = bd_addr_;

    fprintf(f, "E %d tt: %f at: %f %f e: %f n: %d \n",
	    ad, tt, activeTime_, (activeTime_ / tt), energy_, numTurnOn_);

    if (adjnumTurnOn) {
	numTurnOn_--;
    }
}

void Baseband::off()
{
}

int Baseband::command(int argc, const char *const *argv)
{
    if (argc == 3) {
	if (!strcmp(argv[1], "test-fh")) {
	    bd_addr_t addr = strtol(argv[2], NULL, 0);
	    test_fh(addr);
	    return TCL_OK;
	}
    } else if (argc == 5) {
	if (!strcmp(argv[1], "analysis")) {
	    bd_addr_t addr = strtol(argv[2], NULL, 0);
	    int clk = atoi(argv[3]);
	    int len = atoi(argv[4]);
	    signed char *buf =
		seq_analysis(addr, FHChannel, clk, 0, len, 2);
	    delete[]buf;
	    return TCL_OK;
	}
    }
    return BiConnector::command(argc, argv);
}

// Used by PRR. So you can specify prio for a specific link even if it
// has not been formed yet.
void Baseband::set_prio(bd_addr_t remote, int prio)
{
    int i;
    for (i = MinTxBufferSlot; i < maxNumTxBuffer; i++) {
	if (_txBuffer[i] && _txBuffer[i]->remote_bd_addr == remote &&
	    (_txBuffer[i]->type() == ACL)) {
	    _txBuffer[i]->prio = prio;
	    return;
	}
    }

    // txBuffer for remote doesn't exist yet, put the request in queue.
    _prioLevelReq = new PrioLevelReq(remote, prio, _prioLevelReq);
}

// When a new txBuffer is created, apply prio setting if a prioLevelReq exists.
void Baseband::_try_to_set_prio(TxBuffer * txBuffer)
{
    if (!_prioLevelReq) {
	return;
    }
    PrioLevelReq *par = _prioLevelReq;	// parent (of wk) pointer
    if (_prioLevelReq->addr == txBuffer->remote_bd_addr) {
	txBuffer->prio = _prioLevelReq->prio;
	_prioLevelReq = _prioLevelReq->next;
	delete par;
	return;
    }
    PrioLevelReq *wk = _prioLevelReq->next;
    while (wk) {
	if (wk->addr == txBuffer->remote_bd_addr) {
	    txBuffer->prio = wk->prio;
	    par->next = wk->next;
	    delete wk;
	    return;
	}
	par = wk;
	wk = wk->next;
    }
}

TxBuffer *Baseband::allocateTxBuffer(LMPLink * link)
{
    int slot;

    // Locate a free slot.  If no, double the buffer size.
    if (++numTxBuffer == maxNumTxBuffer) {
	slot = maxNumTxBuffer;
	maxNumTxBuffer += maxNumTxBuffer;
	TxBuffer **ntx = new TxBuffer *[maxNumTxBuffer];
	memcpy(ntx, _txBuffer, sizeof(TxBuffer *) * slot);
	memset(&ntx[slot], 0, sizeof(TxBuffer *) * slot);
	delete[]_txBuffer;
	_txBuffer = ntx;
    } else {
	for (int i = MinTxBufferSlot; i < maxNumTxBuffer; i++) {
	    if (_txBuffer[i] == NULL) {
		slot = i;
		break;
	    }
	}
    }

    _txBuffer[slot] = new TxBuffer(this, link, slot);
    if (_prioLevelReq && _txBuffer[slot]->type() == ACL) {
	_try_to_set_prio(_txBuffer[slot]);
    }
    return _txBuffer[slot];
}

void Baseband::freeTxBuffer(TxBuffer * tx)
{
    int slot = tx->slot();
    _txBuffer[slot] = NULL;
    numTxBuffer--;
    delete tx;
}

void Baseband::sendBBSig(Packet * p)
{
    // BBsig packets are never kepted in txBuffer.
    HDR_BT(p)->nokeep = 1;
    sendDown(p);
}

// This function send packet to phy layer
void Baseband::sendDown(Packet * p, Handler * h)
{
    _transmit(p);
}

int Baseband::tranceiveSlots(hdr_bt * bh)
{
    // TODO: need more exact calculation??
/*
    if (bh->type == hdr_bt::Id) {
	return 1;
    }
*/
#if 0
    if (bh->size > 1626) {	// DM3/DH3 1626/1622
	return 10;		// 5 slots
    } else if (bh->size > 366) {
	return 6;		// 3 slots
    } else if (bh->size >= 126) {
	return 2;		// 1 slot
    } else {
	return 1;		// 1/2 slot
    }
#endif

    return (bh->type == hdr_bt::Id ? 1 : hdr_bt::slot_num(bh->type) * 2);
}

void Baseband::tx_handle(Packet * p)
{
    if (0 && lmp_->curPico && lmp_->curPico->rfChannel_
	&& state() == CONNECTION && HDR_BT(p)->type != hdr_bt::HLO) {
	// in BTChannel, the pkt is copied over to the whole piconet.
	lmp_->curPico->rfChannel_->recv(p);

    } else {
	hdr_bt *bh = HDR_BT(p);
	int nokeep = bh->nokeep;
	if (nokeep) {
	    bh->nokeep = 0;
	}
	Baseband *wk = _next;
	while (wk != this) {	// doesn't loop back to myself

	    // every one gets it at the time that first bit arrives    
	    // Virtually, we consider BTDELAY is very small (close to 0).  Here
	    // BTDELAY is set to half of the max slot misalignment for
	    // timing syn at the receiver side.  By this way, the clk does not
	    // need to be adjusted to generate correct FH.  See bt.h.
	    // s.schedule(&wk->recvHandler, p->copy(), BTDELAY);

	    //if (wk->_trx_st == RX_ON) {
	    wk->recv_handle(p->copy());
	    //}
	    wk = wk->_next;
	}
	if (nokeep) {
	    Packet::free(p);
	}
    }
}

// This function may be moved down to phy layer, if it exists.
void Baseband::_transmit(Packet * p)
{
    Scheduler & s = Scheduler::instance();
    hdr_cmn *ch = HDR_CMN(p);
    hdr_bt *bh = HDR_BT(p);
    ch->direction() = hdr_cmn::UP;
    // ch->size() = bh->size / 8;
    bh->ts_ = s.clock();
    bh->sender = bd_addr_;

    activeBuffer = lookupTxBuffer(bh);
    _in_transmit = tranceiveSlots(bh);
    _in_receiving = 0;

    turn_on_tx();
    _trxoff_ev.st = _trx_st;
    s.schedule(&_trxtoTimer, &_trxoff_ev, bh->txtime() + trxTurnarndTime_);
    if (p->uid_ > 0) {
	s.cancel(p);
    }
    s.schedule(&txHandler, p, BTDELAY);

    _interfQ[bh->fs_].add(bd_addr_, X_, Y_, bh->ts_,
			  bh->ts_ + bh->txtime());

    int tr = 0;
    if ((trace_me_null_ || trace_all_null_) && bh->type == hdr_bt::Null) {
	tr = 1;
    }
    if ((trace_me_poll_ || trace_all_poll_) && bh->type == hdr_bt::Poll) {
	tr = 1;
    }
    if ((trace_all_tx_ == 1 || trace_me_tx_ == 1)
	&& bh->type != hdr_bt::Null && bh->type != hdr_bt::Poll) {
	tr = 1;
    }

    if (tr) {
	bh->dump(BtStat::log, BTTXPREFIX, bd_addr_, state_str_s());	// "t "
	if (bh->extinfo >= 10) {
	    bh->dump_sf();
	}
    }
    // Note, p is freed only right before next new pkt is transmitted, 
    // since p == _current at this moment.  However, the packets not kept
    // in TxBuffer should be freed in tx_handle().
}

// put time stamp on the packet
Packet *Baseband::stamp(Packet * p, TxBuffer * tx)
{
    hdr_bt *bh = HDR_BT(p);
    bh->lt_addr = tx->link()->lt_addr;
    bh->clk = clk_;
    bh->ac = master_bd_addr_;
    bh->srcTxSlot = tx->slot();
    bh->dstTxSlot = tx->dstTxSlot();
    bh->sender = bd_addr_;
    bh->transmitCount++;
    bh->X_ = X();
    bh->Y_ = Y();
    // bh->Z_ = Z();
    bh->nokeep = 0;

    if (isMaster()) {
	FH_sequence_type fhs = (tx->afhEnabled_ ? FHAFH : FHChannel);
	bh->fs_ = FH_kernel(clk_, clkf_, fhs, master_bd_addr_);

	if (tx->afhEnabled_) {
	    _afhEnabled = 1;
	    _transmit_fs = bh->fs_;	// Used for same channel mechanism.
	} else {
	    _afhEnabled = 0;
	}
    } else if (tx->afhEnabled_) {
	bh->fs_ = _transmit_fs;
    } else {
	bh->fs_ = FH_kernel(clk_, clkf_, FHChannel, master_bd_addr_);
    }

    return p;
}

// Never called
void Baseband::sendUp(Packet * p, Handler * h)
{
    abort();
    // uptarget_->recv(p, h);
}

int Baseband::isBusy()
{
#if 0
    return _state == PAGE ||
	_state == PAGE_SCAN ||
	_state == NEW_CONNECTION_MASTER ||
	_state == NEW_CONNECTION_SLAVE ||
	_state == ROLE_SWITCH_MASTER ||
	_state == RS_NEW_CONNECTION_MASTER ||
	_state == MASTER_RESP ||
	_state == SLAVE_RESP ||
	_state == INQUIRY_RESP || _state == INQUIRY;
#endif
    // return _state != STANDBY && _state != CONNECTION;

    // It is too strong in case that scan window is small.
    // That scan and connect interlaced.
    return discoverable || connectable || inRS_ ||
	(_state != STANDBY && _state != CONNECTION);
}

void Baseband::setPiconetParam(Piconet * pico)
{
    isMaster_ = pico->isMaster();
    if (state() != CONNECTION) {
	change_state(CONNECTION);
    }
    master_bd_addr_ = pico->master->bd_addr;
    pico->compute_sched();
    set_sched_word(pico->_sched_word);
    pico->_sched_word->dump();

    if (isMaster_) {
	_polling = 1;
	_reset_as = 0;
	linkSched->init();
	// fprintf(stdout, "* %d is master\n", bd_addr_);
    } else {
	_polling = 0;
	_reset_as = 1;
	lt_addr = pico->activeLink->lt_addr;
	// fprintf(stdout, "* %d is slave\n", bd_addr_);
    }
    _in_receiving = 0;		// terminate current receiving.
    _in_transmit = 0;		// ??
    // _rxPkt = 0;
    _txSlot = 0;
    _sco_state = SCO_IDLE;
    setScoLTtable(pico->sco_table);
}

// doesn't consider clk wrap round
int Baseband::sched(int clk, BTSchedWord * sched)
{
    int ret = sched->word[(clk >> 1) % sched->len];

    // a txbuffer maybe just freed.
    if (ret >= MinTxBufferSlot && !_txBuffer[ret]) {
	ret = DynamicSlot;
    }

    if (ret < ReserveSlot) {
	return ret;
    }

    if (ret == RecvSlot) {
	return ret;
    }

    if (ret >= MinTxBufferSlot && _txBuffer[ret]->type() == SCO) {
	return ret;
    }
    // Slave's SchedWord defauts to receive only.  Upone polled, the 
    // following slot is set to send. After sending, that slot is set
    // to receive again.
    if (ret >= MinTxBufferSlot &&
	_txBuffer[ret]->type() == ACL &&
	(_reset_as || _txBuffer[ret]->link()->_reset_as)) {
	sched->word[(clk >> 1) % sched->len] = RecvSlot;
	return ret;
    }
    // Check if any Bcast pkt pending.  Send it first.  We don't consider
    // the case where bcast overload the network.  The principle is,
    // bcast is sent only necessary, so, deliver them first.
    if (_txBuffer[BcastSlot] && _txBuffer[BcastSlot]->hasBcast()) {
	return BcastSlot;
    }
    // BeaconSlot should be handled the same way if PARK is supported.

    if (!isMaster()) {
	return ret;
    }
    // Master schedule the sending by different algorithms.
    if (!lmp_->curPico) {
	fprintf(BtStat::logstat,
		BTPREFIX1 "%d oops, curPico = null at bb sched().\n",
		bd_addr_);
	return RecvSlot;
    } else if (lmp_->curPico->numActiveLink == 0) {
	return RecvSlot;
    }
    return linkSched->sched(clk, sched, ret);
}

// Letting slave send.
void Baseband::polled(int slot)
{
    _sched_word->word[((_polling_clk >> 1)) % _sched_word->len] = slot;
}

void Baseband::change_state(BBState st)
{
    const char *ps = state_str();
    int turnonrx = 1;

    pagerespTimer = 0;

    switch (st) {
    case INQUIRY_SCAN:
	T_w_inquiry_scan_timer = T_w_inquiry_scan_;

	// interlaced scan
	if (Inquiry_Scan_Type_ != Standard) {
	    Inquiry_Scan_Type_ = Interlaced;
	    if (T_w_inquiry_scan_ + T_w_inquiry_scan_ <= T_inquiry_scan_) {
		T_w_inquiry_scan_timer += T_w_inquiry_scan_;
	    }
	}
	break;

    case PAGE_SCAN:
	T_w_page_scan_timer = T_w_page_scan_;

	// interlaced scan
	if (Page_Scan_Type_ != Standard) {
	    Page_Scan_Type_ = Interlaced;
	    if (T_w_page_scan_ + T_w_page_scan_ <= T_page_scan_) {
		T_w_page_scan_timer += T_w_page_scan_;
	    }
	}
	break;

    case INQUIRY_RESP:
	inqrespTimer = inqrespTO;
	break;
    case MASTER_RESP:
    case SLAVE_RESP:
	pagerespTimer = pagerespTO;
	break;
    case ROLE_SWITCH_MASTER:
    case ROLE_SWITCH_SLAVE:
	inRS_ = 1;
    case RS_NEW_CONNECTION_SLAVE:
    case RS_NEW_CONNECTION_MASTER:
    case NEW_CONNECTION_SLAVE:
    case NEW_CONNECTION_MASTER:
	newconnectionTimer = newconnectionTO;
	break;
    default:
	turnonrx = 0;
	break;
    }

/*
    if (turnonrx) {
	turn_on_rx();
    }
*/

    _state = st;
    if (trace_all_stat_ || trace_me_stat_) {
	fprintf(BtStat::logstat,
		BTPREFIX1 "%d at %f CHANGE ST %s -> %s clkn:%d clk:%d\n",
		bd_addr_,
		Scheduler::instance().clock(), ps, state_str(),
		clkn_, clk_);
    }
}

// Use for LR network where a pair of POLL/NULL trigger the link into sleep.
void Baseband::putLinkIntoSleep(LMPLink * link)
{
}

// If a slave didn't hear a packet during 250ms, it should perform
// channel re-sychronization. This is done whenever a slave is waken up
// from a suspened mode such as SNIFF, HOLD and PARK.  The search window 
// should be increased to a proper value according to the time of losing 
// sync.  
void Baseband::enter_re_sync(double t)
{
    Scheduler & s = Scheduler::instance();

    // clk_ was set to the value of the beginning of next frame. 
    // It is so arranged in order to be the right value in the beginning of
    // next frame, remembering that it is increased by _resyncWindSlotNum * 2
    // in the event handler handle_re_synchronize().
    //clk_ -= (_resyncWindSlotNum * 2 - 2); 
    clk_ -= (_resyncWindSlotNum * 2);
    _resyncCntr = 0;

    change_state(RE_SYNC);
    if (resync_ev.uid_ > 0) {
	s.cancel(&resync_ev);
    }
    s.schedule(&resync_handler, &resync_ev, t);
}

// Event handler
void Baseband::handle_re_synchronize(Event * e)
{
    Scheduler & s = Scheduler::instance();
    if (++_resyncCntr <= _maxResync) {
	s.schedule(&resync_handler, e, SlotTime * _resyncWindSlotNum);
    }

    _resyncWindStartT = s.clock();
#ifdef PRINTRESYN
    printf("==== %d:%d _resyncWindStartT = %f win:%f num:%d\n", bd_addr_,
	   _resyncCntr,
	   _resyncWindStartT, _resyncWind, _resyncWindSlotNum);
#endif
    turn_on_rx();
    _trxoff_ev.st = _trx_st;
    s.schedule(&_trxtoTimer, &_trxoff_ev, _resyncWind);	// set up reSync window

    clk_ += (_resyncWindSlotNum * 2);
}

// perform the re-synchronization.
void Baseband::re_sync()
{
    Scheduler & s = Scheduler::instance();
    s.cancel(&resync_ev);
    change_state(CONNECTION);

    // Check timing
    t_clk_00 = s.clock();
    fprintf(stdout, "%f sched clk at %f by re_syn()\n",
	    s.clock(), s.clock() + SlotTime - MAX_SLOT_DRIFT / 2);
    s.schedule(&clk_handler, &clk_ev, SlotTime - MAX_SLOT_DRIFT / 2);

    int clkoffset;
    int slotoffset;
    comp_clkoffset(MAX_SLOT_DRIFT / 2, &clkoffset, &slotoffset);
#ifdef PRINTRESYN
    printf("==== %d:%d RE_SYN'd: %f - %f = %f w:%f n:%d\n", bd_addr_,
	   _resyncCntr, s.clock(),
	   _resyncWindStartT, (s.clock() - _resyncWindStartT),
	   _resyncWind, _resyncWindSlotNum);
#endif
    if (lmp_->curPico) {
	lmp_->curPico->clk_offset = clkoffset;
	lmp_->curPico->slot_offset = slotoffset;
	LMPLink *link = lmp_->curPico->activeLink;
	if (link && link->_sniff_ev && link->_sniff_ev->uid_ > 0) {
	    double t =
		link->_sniff_ev->time_ + (s.clock() - _resyncWindStartT);
	    s.cancel(link->_sniff_ev);
	    s.schedule(&lmp_->_timer, link->_sniff_ev, t - s.clock());
	    link->_lastSynClk = clkn_;
	}
	if (link && link->_sniff_ev_to && link->_sniff_ev_to->uid_ > 0) {
	    double t = link->_sniff_ev_to->time_ +
		(s.clock() - _resyncWindStartT);
	    s.cancel(link->_sniff_ev_to);
	    s.schedule(&lmp_->_timer, link->_sniff_ev_to, t - s.clock());
	}
    }
    s.cancel(&resync_ev);
}

// for transmitting in connection mode. The master sets clk_ = clkn_
// for master, this event is 1 usec later than clkn event, so clkn_
// is always updated before clk_ is.
void Baseband::handle_clk(Event * e)
{
    Scheduler & s = Scheduler::instance();
    BBState st = state();

    int curSlot;
    Packet *p;
    TxBuffer *txb;
    int turnonrx = 1;

    if ((clk_ & 0x01)) {
	fprintf(stderr, "OOOOps, clk_0 is not 0\n");
	clk_++;
	s.schedule(&clk_handler, e, Tick);
	return;
    }
    // doesn't consider wrap around. This happens once a day, though
    clk_++;
    clk_++;

    if ((clk_ & 0x03) == 0) {
	t_clk_00 = s.clock();
    }
    // Check SCO slots
    // XXX should we abort a receiving to prepare the SCO t/rx ??
    // consider if it is receiving an page/inquiry response.
    if ((txb = lookupScoTxBuffer())) {

	if (isMaster()) {
	    if ((clk_ & 0x02)) {
		_polled_lt_addr = txb->link()->lt_addr;
		_sco_state = SCO_RECV;
	    } else {
		_sco_state = SCO_SEND;
	    }

	} else {
	    _sco_state = ((clk_ & 0x02) ? SCO_SEND : SCO_RECV);
	}

	_in_transmit = 0;
	_in_receiving = 0;

	if (_sco_state == SCO_SEND) {
	    turnonrx = 0;
	    if ((p = txb->transmit()) != NULL) {
		sendDown(p, 0);
	    }
	}
	// fprintf(stdout, "%d clk :%d\n", bd_addr_, clk_);

	goto update;

    } else {

	_sco_state = SCO_IDLE;

	// The following 2 counter are needed because trx state alone cannot
	// determine the right tx timing.  e.g. if the MA transmits a DH3 
	// packets which has 630 symbols, then MA shouldn't transmit in next 
	// frame and should wait in RX in the second slot of next frame.
	_in_transmit--;
	_in_transmit--;
	if (_in_transmit > 0) {
	    s.schedule(&clk_handler, e, SlotTime);
	    return;
	}
	_in_receiving--;
	_in_receiving--;
	if (_in_receiving > 0) {
	    s.schedule(&clk_handler, e, SlotTime);
	    return;
	}
    }

/*
    if (_trx_st != TRX_OFF) {
	s.schedule(&clk_handler, e, SlotTime);
	return;
    }
*/


    switch (st) {
    case NEW_CONNECTION_SLAVE:
	if (--newconnectionTimer == 0) {
	    change_state(PAGE_SCAN);
	    connectable = 1;
	    s.cancel(&clk_ev);
	    turn_on_rx();
	    return;
	}
	// we still use _slave_send() to send response at this phrase.
	break;

    case ROLE_SWITCH_MASTER:	// switch to Master Role from Slave Role
	if (--newconnectionTimer == 0) {
	    change_state(CONNECTION);
	    lmp_->role_switch_bb_complete(_slave, 0);	// Failed.
	    turn_on_rx_to();
	    return;
	} else if ((clk_ & 0x03) == 0
		   && sched(clk_, _sched_word) == BasebandSlot) {
	    p = genFHSPacket(FHChannel, master_bd_addr_,
			     clk_, bd_addr_, slave_lt_addr, _slave);

	    HDR_BT(p)->u.fhs.clk = (clkn_ & 0xFFFFFFFC);
	    HDR_BT(p)->lt_addr = 0;
	    HDR_BT(p)->comment("F");
	    sendBBSig(p);
	    fprintf(BtStat::logstat, BTPREFIX1 "master send clk: %d\n",
		    clkn_);
	} else if ((clk_ & 0x02)) {
	    turn_on_rx_to();
	}
	turnonrx = 0;
	break;
    case ROLE_SWITCH_SLAVE:	// Switch to Slave Role
	if (--newconnectionTimer == 0) {
	    change_state(CONNECTION);
	    lmp_->role_switch_bb_complete(_slave, 0);	// Failed.
	    return;
	}
	break;
    case RS_NEW_CONNECTION_MASTER:
	if (--newconnectionTimer == 0) {
	    change_state(CONNECTION);
	    lmp_->role_switch_bb_complete(_slave, 0);	// Failed.
	    turn_on_rx_to();
	    return;
	} else if ((clk_ & 0x01) == 0
		   && sched(clkn_, _sched_word) == BasebandSlot) {
	    p = genPollPacket(FHChannel, master_bd_addr_, clk_, _slave);
	    HDR_BT(p)->lt_addr = slave_lt_addr;
	    HDR_BT(p)->comment("P");
	    sendBBSig(p);
	    turnonrx = 0;
	}
	break;
    case RS_NEW_CONNECTION_SLAVE:
	if (--newconnectionTimer == 0) {
	    change_state(CONNECTION);
	    lmp_->role_switch_bb_complete(_slave, 0);	// Failed.
	    turn_on_rx_to();
	    return;
	}
	break;
    case UNPARK_SLAVE:
	if ((clk_ & 0x02)) {
	    // FIXME: reset _access_request_slot, POLL
	    _access_request_slot++;
	    _access_request_slot++;
	    if (ar_addr_ == _access_request_slot) {
		p = genIdPacket(FHChannel, master_bd_addr_, clk_,
				master_bd_addr_);
		sendBBSig(p);	// send in the first half slot
		turnonrx = 0;
	    } else if (ar_addr_ == _access_request_slot + 1) {
		p = genIdPacket(FHChannel, master_bd_addr_, clk_,
				master_bd_addr_);
		// I need to send at the second half slot.
		// Yes. Both packets at the same slot have the same freq.
		s.schedule(&slaveSendnonConnHandler, p, Tick);
		turnonrx = 0;
	    }
	}
	break;

    case CONNECTION:

	curSlot = sched(clk_, _sched_word);

	if (curSlot >= BcastSlot) {
	    if ((p = _txBuffer[curSlot]->transmit()) != NULL) {
		_txSlot = curSlot;
		_txClk = clk_;
		hdr_bt *bh = HDR_BT(p);
		sendDown(p, 0);
		if (isMaster()) {
		    _txBuffer[_txSlot]->txType = bh->type;
		    _txBuffer[_txSlot]->deficit_ -=
			hdr_bt::slot_num(bh->type);
		    _txBuffer[_txSlot]->ttlSlots_ +=
			hdr_bt::slot_num(bh->type);
		}
	    }
	    turnonrx = 0;
	}
#if 1
	if (isMaster()) {	// XXX: this violate schedword mechnism.
	    if ((clk_ & 0x02) == 0) {
		turnonrx = 0;
	    }
	} else {
	    if ((clk_ & 0x02) == 2) {
		turnonrx = 0;
	    }
	}
#endif
	break;
    default:
	curSlot = sched(clk_, _sched_word);	// SCO
	if (curSlot >= MinTxBufferSlot
	    && _txBuffer[curSlot]->type() == SCO) {
	    if ((p = _txBuffer[curSlot]->transmit()) != NULL) {
		sendDown(p, 0);
	    }
	    turnonrx = 0;
	}
#if 1
	if (isMaster()) {	// XXX: this violate schedword mechnism.
	    if ((clk_ & 0x02) == 0) {
		turnonrx = 0;
	    }
	} else {
	    if ((clk_ & 0x02) == 2) {
		turnonrx = 0;
	    }
	}
#endif
    }

  update:
    if (turnonrx) {
	turn_on_rx_to();
    }
    // TODO: fine tune about t_clk_00
    if (isMaster() || driftType_ == BT_DRIFT_OFF || (clk_ & 0x10) == 0) {
	s.schedule(&clk_handler, e, SlotTime);
    } else {
	// fprintf(stderr, "%d drift_clk: %f\n", bd_addr_, drift_clk * 1E6);
	s.schedule(&clk_handler, e, SlotTime + drift_clk);
	drift_clk = 0;
    }
}

void Baseband::setdest(double destx, double desty, double speed)
{
    double d =
	sqrt((destx - X_) * (destx - X_) + (desty - Y_) * (desty - Y_));
    dX_ = speed * (destx - X_) / d;
    dY_ = speed * (desty - Y_) / d;
    _dXperTick = dX_ * Tick;
    _dYperTick = dY_ * Tick;
}

int Baseband::wakeupClkn()
{
    Scheduler & s = Scheduler::instance();
    double sleept = s.clock() - t_clkn_suspend_;
    int nt = int (sleept / Tick);
    clkn_ = clkn_suspend_ + nt;
    int clkndiff = clkn_ - (clkn_suspend_ & 0xfffffffc);
    t_clkn_00 += (clkndiff / 4) * SlotTime * 2;

    double nextclkt = t_clkn_suspend_ + Tick * nt + Tick;
    double offsetinframe = s.clock() - t_clkn_00;

    // There may have round up error, so compared to 5 Tick
    if (offsetinframe >= SlotTime * 2) {
	t_clkn_00 += SlotTime * 2;
    }
    double t = nextclkt - s.clock();
    if (t <= 0) {
	t = Tick;
    }

    X_ = X_suspend_ + _dXperTick * nt;
    Y_ = Y_suspend_ + _dYperTick * nt;
    s.cancel(&clkn_ev);
    s.schedule(&clkn_handler, &clkn_ev, t);
    _insleep = 0;

    return 0;
}

int Baseband::trySuspendClknSucc()
{
    if (lmp_->curPico) {
	return 0;
    }
    // check the earliest wait up time

    Scheduler & s = Scheduler::instance();
    double wakeupt = 1E10;

    Piconet *pico = lmp_->suspendPico;
    LMPLink *link;
    do {
	if (pico->activeLink) {
	    fprintf(stderr, "%d act link in suspnedPico.\n", bd_addr_);
	    return 0;
	}
	link = pico->suspendLink;
	do {
	    if (!link->_in_sniff) {
		return 0;
	    }
	    if (link->_sniff_ev->time_ < wakeupt) {
		wakeupt = link->_sniff_ev->time_;
	    }
	} while ((link = link->next) != pico->suspendLink);
    } while ((pico = pico->next) != lmp_->suspendPico);

    int dist = int ((wakeupt - s.clock()) / Tick) - 12;

    if (dist <= 0) {
	return 0;
    }

    X_suspend_ = X_;
    Y_suspend_ = Y_;
    X_ += _dXperTick * dist;
    Y_ += _dYperTick * dist;

    // clkn_--;
    clkn_suspend_ = clkn_;
    t_clkn_suspend_ = s.clock();
    clkn_ += (dist - 1);
    s.schedule(&clkn_handler, &clkn_ev, dist * Tick);
    fprintf(stdout, "%d %s cancel clk\n", bd_addr_, __FUNCTION__);
    s.cancel(&clk_ev);
    // fprintf(stderr, "%d Suspend clkn for %d ticks.\n", bd_addr_, dist);
    return 1;
}

// handle baseband events
//
// can handle page scan and inquiry scan co-exists. Need some tests.
void Baseband::handle_clkn(Event * e)
{
    Scheduler & s = Scheduler::instance();

    Packet *p;
    hdr_bt *bh;

    // doesn't consider wrap around. This happens once a day, though
    ++clkn_;
    ++clke_;			// meaningfull only if being set.

    // update position
    X_ += _dXperTick;
    Y_ += _dYperTick;

    if ((clkn_ & 0x03) == 0) {
	t_clkn_00 = s.clock();

	// Optimization: if the device are going to sleep for a long time,
	// the clock can be turn off temporarily.
	if (suspendClkn_) {
	    suspendClkn_ = 0;
	    if (trySuspendClknSucc()) {
		_insleep = 1;
		return;
	    }
	}
    }
    _insleep = 0;

#if 0
    // -- broken
    // This is awkward.  Try to find some way to update t/rx state.
    // TODO:
    if (clk_ev.uid_ <= 0) {
	_in_transmit--;
	_in_receiving--;
    }
#endif

    // specs says slave_rsps_count should increase when clkn_10 turns to 0.
    // This is a bug. Since a turn around message may only take
    // 1.5 slots of time to back when 'slave_rsps_count' still not increased.
    if ((clkn_ & 0x03) == slave_rsps_count_incr_slot) {
	slave_rsps_count++;
    }
    // meaningfull only if clke_ being set.
    if ((clke_ & 0x03) == 0) {
	master_rsps_count++;
    }
    // Check InqScan first.  Give it preferrence over PageScan if both Requests
    // are issued at the same time.
    if (discoverable) {
	if (--T_inquiry_scan_timer <= 0) {
	    T_inquiry_scan_timer = T_inquiry_scan_;

	    if (state() == PAGE_SCAN) {
		// Page Scan in progress, wait ...
		// This makes scan interval a little larger.
		// But should not be a problem.
		T_inquiry_scan_timer = T_w_page_scan_timer;

	    } else if ((!inBackoff)
		       && (state() != INQUIRY_RESP)) {
		// if in INQUIRY_RESP, skip INQUIRY_SCAN.     
		change_state(INQUIRY_SCAN);
	    }
	    // should we reset it ?? spec 1.2 says it is arbitrary chosen.
	    // inquiry_rsps_count = 0; 
	}
    }

    if (inBackoff) {		// inquiry scan back off in progress
	if (--backoffTimer <= 0) {
	    inBackoff = 0;

	    if (state() == PAGE_SCAN) {
		// page scan in progress, sleep for a while
		inBackoff = 1;
		backoffTimer = T_w_page_scan_timer;
	    } else {
		change_state(INQUIRY_RESP);
	    }
	}
    }
    // FIXME: need to handle special case: PSCAN after pageresp/newconn timeout
    if (connectable) {
	if (pagerespTimer <= 0 && state() != NEW_CONNECTION_SLAVE
		&& --T_page_scan_timer <= 0) {
	    T_page_scan_timer = T_page_scan_;
	    if (state() == INQUIRY_SCAN) {
		// delay until inq scan finish
		T_page_scan_timer = T_w_inquiry_scan_timer;
	    } else if (state() == INQUIRY_RESP) {	// delay ...
		T_page_scan_timer = inqrespTimer;
	    } else {
		change_state(PAGE_SCAN);
	    }
	}
    }

    switch (state()) {

    case PAGE_SCAN:
	if (!connectable) {
	    fprintf(stderr,
		    "**OOps, PAGE_SCAN in non-connectable mode.\n");
	    break;
	}
	if (--T_w_page_scan_timer <= 0) {
	    _trxoff_ev.clearSt();
	    turn_off_trx();
	    change_state(_stable_state);
	    break;

	    // interlaced scan
	} else if (T_w_page_scan_timer <= T_w_page_scan_
		   && Page_Scan_Type_ == Interlaced
		   && (T_w_page_scan_ * 2) <= T_page_scan_) {
	    Page_Scan_Type_ = InterlacedSecondPart;
	}
	turn_on_rx();
	break;

    case SLAVE_RESP:
	if (--pagerespTimer <= 0) {
	    // change_state(PAGE_SCAN); // also set pagerespTimer
	    connectable = 1;
	    T_page_scan_timer = 1;	// at next CLK, pscan will be performed.
	}
	// should do nothing ??
	turn_on_rx();		//XXX: what is the timing ref??
	// s.schedule(&_rxtoTimer, &_rxoff_ev, Tick + MAX_SLOT_DRIFT);

	break;

    case PAGE:

	if (--page_timer <= 0) {
	    change_state(_stable_state);
	    lmp_->page_complete(_slave, 0);	// failed.
	    break;
	}
	// Change train only at frame boundary
	if (--page_train_switch_timer <= 0 && (clkn_ & 0x03) == 0) {
	    change_train();
	    page_train_switch_timer = N_page_ * 32;
	}
	if (sched(clkn_, _sched_word) == BasebandSlot) {
	    p = genIdPacket(FHPage, _slave, clke_, _slave);
	    HDR_BT(p)->comment("P");
	    HDR_BT(p)->clk = clke_;
	    sendBBSig(p);
	    // } else if (_in_receiving <= 0) {     // Check if I'm receiving a resp
	    // XXX when _in_receiving is updated ???
	} else if (_in_receiving <= 1) {	// Check if I'm receiving a resp
	    turn_on_rx_to();
	}
	break;

    case MASTER_RESP:
	if (--pagerespTimer <= 0) {
	    change_state(PAGE);
	} else if ((clkn_ & 0x01) == 0
		   && sched(clkn_, _sched_word) == BasebandSlot) {
	    slave_lt_addr = lmp_->get_lt_addr(_slave);
	    p = genFHSPacket(FHMasterResp, _slave,
			     clke_, bd_addr_, slave_lt_addr, _slave);
	    HDR_BT(p)->u.fhs.clk = clkn_ & 0xFFFFFFFC;
	    HDR_BT(p)->comment("MR");
	    fprintf(BtStat::logstat, BTPREFIX1 "LT: %d\n", slave_lt_addr);
	    HDR_BT(p)->clk = clke_;
	    sendBBSig(p);
	} else {
	    turn_on_rx_to();
	}
	break;

    case NEW_CONNECTION_MASTER:
	if (--newconnectionTimer <= 0) {
	    change_state(PAGE);
	} else if ((clkn_ & 0x01) == 0
		   && sched(clkn_, _sched_word) == BasebandSlot) {
	    p = genPollPacket(FHChannel, bd_addr_, clkn_, _slave);
	    HDR_BT(p)->lt_addr = slave_lt_addr;
	    HDR_BT(p)->comment("NP");
	    sendBBSig(p);
	} else {
	    turn_on_rx_to();
	}
	break;

    case NEW_CONNECTION_SLAVE:
	// moved to clk_handler;
	break;

    case INQUIRY_SCAN:
	if (!discoverable) {
	    fprintf(stderr,
		    "**OOps, INQUIRY_SCAN in non-discovable mode.\n");
	    break;
	}
	if (--T_w_inquiry_scan_timer <= 0) {
	    _trxoff_ev.clearSt();
	    turn_off_trx();
	    change_state(_stable_state);
	    break;

	    // interlaced scan
	} else if (T_w_inquiry_scan_timer <= T_w_inquiry_scan_
		   && Inquiry_Scan_Type_ == Interlaced
		   && (T_w_inquiry_scan_ * 2) <= T_inquiry_scan_) {
	    Inquiry_Scan_Type_ = InterlacedSecondPart;
	}
	turn_on_rx();
	break;

    case INQUIRY_RESP:
	if (--inqrespTimer <= 0) {
	    // change_state(_stable_state);
	    turn_off_trx();
	    discoverable = 1;
	    change_state(INQUIRY_SCAN);
	} else {
	    turn_on_rx();	// XXX
	}
	break;

    case INQUIRY:
	if (--inq_timer <= 0) {
	    change_state(_stable_state);

	    // the following may change _state by lmp schuduler.
	    lmp_->inquiry_complete(inq_num_responses_);
	    break;
	}
	if (--inquiry_train_switch_timer <= 0 && (clkn_ & 0x03) == 0) {
	    change_train();
	    inquiry_train_switch_timer = N_inquiry_ * 32;
	}

	if (sched(clkn_, _sched_word) == BasebandSlot) {
	    if (_trx_st == RX_ON) {
		break;
	    }
	    p = genIdPacket(FHInquiry, lap, clkn_, BD_ADDR_BCAST);
	    bh = HDR_BT(p);
	    bh->comment("Q");
	    sendBBSig(p);
	    // } else if (_in_receiving <= 0) {     // Check if I'm receiving a resp
	    // XXX when _in_receiving is updated ???
	} else if (_in_receiving <= 1) {	// Check if I'm receiving a resp
	    turn_on_rx_to();
	}
	break;

    default:
	break;
    }
    s.schedule(&clkn_handler, e, Tick);
}

// Check if first bit of an incoming pkt is within the +-10us shredhold.
// Since we set delay as 10us.  We check to see if it is in [0,20us] range
// instead.  MAX_SLOT_DRIFT = 20us.
// offset is either a Slot or Half Slot.
int Baseband::recvTimingIsOk(double clk00, double offset, double *drift)
{
    *drift = Scheduler::instance().clock() - offset - clk00;

    if (*drift > MAX_SLOT_DRIFT || *drift < 0) {
#ifdef PRINT_RECV_TIMING_MISMATCH
	if (bd_addr_ == _receiver) {
	    fprintf(BtStat::logstat,
		    BTPREFIX1 "%d recvTiming: t:%f t00:%f dr:%f \n",
		    bd_addr_, Scheduler::instance().clock(), clk00,
		    *drift);
	}
#endif
	*drift = 0;
	// *drift = *drift - BTDELAY;
	return 0;
    } else {
	*drift = *drift - BTDELAY;
	return 1;
    }
}

int Baseband::ltAddrIsValid(uchar ltaddr)
{
    return lmp_->curPico->ltAddrIsValid(ltaddr);
}

clk_t Baseband::_interlaceClk(clk_t clk)
{
    int clk_16_12 = (clk >> 12) & 0x1F;
    clk_16_12 = (clk_16_12 + 16) % 32;
    return ((clk & 0xFFFE0FFF) | (clk_16_12 << 12));
}

int Baseband::comp_clkoffset(double timi, int *clkoffset, int *sltoffset)
{
    Scheduler & s = Scheduler::instance();
    *clkoffset = (int) ((clk_ & 0xfffffffc) - (clkn_ & 0xfffffffc));
    *sltoffset =
	// (int) ((s.clock() - bh->txtime() - BTDELAY +
	(int) ((s.clock() - timi + BT_CLKN_CLK_DIFF - t_clkn_00) * 1E6);
    if (*sltoffset < 0) {
	*sltoffset += 1250;
	*clkoffset += 4;
    }

    return 1;
}

// A real syn should be done by tuning the slave to a specific frequency,
// to sniff a master's packet.
int Baseband::synToChannelByGod(hdr_bt * bh)
{
    if (isMaster()) {
	return 0;
    }
    Scheduler & s = Scheduler::instance();
    int i;
    int clk = clk_;
    if (clk & 0x01) {
	clk--;
    }
    for (i = 0; i < SYNSLOTMAX; i++, i++) {
	if (FH_kernel(clk + i, 0, FHChannel, master_bd_addr_) == bh->fs_) {
	    clk = clk + i;
	    break;
	}
	if (FH_kernel(clk - i - 2, 0, FHChannel, master_bd_addr_) ==
	    bh->fs_) {
	    clk = clk - i - 2;
	    break;
	}
    }
    if (i == SYNSLOTMAX) {	// Correct clk is not found.
	return 0;
    }
#ifdef DUMPSYNBYGOD
    fprintf(BtStat::log, "%d-%d at %f %d SynToChByGod: clkdiff: %d ",
	    bd_addr_, master_bd_addr_, s.clock(),
	    (int) (s.clock() / BTSlotTime), clk - clk_);
#endif
    clk_ = clk;
    if (clk_ev.uid_ > 0) {
#ifdef DUMPSYNBYGOD
	double t =
	    SlotTime - (clk_ev.time_ - s.clock()) - MAX_SLOT_DRIFT / 2;
	fprintf(BtStat::log, " timing %f.\n", t);
#endif
	fprintf(stdout, "%d %s cancel clk\n", bd_addr_, __FUNCTION__);
	s.cancel(&clk_ev);
    }
    fprintf(BtStat::log, "%f sched clk at %f by %s\n",
	    s.clock(), s.clock() + SlotTime - BTDELAY + BT_CLKN_CLK_DIFF,
	    __FUNCTION__);
    s.schedule(&clk_handler, &clk_ev,
	       SlotTime - BTDELAY + BT_CLKN_CLK_DIFF);
    double new_t_clk_00 = s.clock() - BTDELAY + BT_CLKN_CLK_DIFF;
#ifdef DUMPSYNBYGOD
    fprintf(BtStat::log, " t_clk_00 : %f -> %f, %f\n", t_clk_00,
	    new_t_clk_00, new_t_clk_00 - t_clk_00);
#endif
    t_clk_00 = new_t_clk_00;

    int clkoffset;
    int slotoffset;
    comp_clkoffset(0, &clkoffset, &slotoffset);
    if (lmp_->curPico) {
	lmp_->curPico->clk_offset = clkoffset;
	lmp_->curPico->slot_offset = slotoffset;
    }

    return 1;
}

// This is a filter to filter out mismatching frequency and access code
// This is applied when first bit arrives.
//   returns:
//      0: fails to receive, ie. ac/fs mismatch
//      1: receiving whole packet
//      2: decoding hdr only
//      3: mismatch, donot set RX_OFF, eg. in page scan state.
int Baseband::recv_filter(Packet * p)
{
    Scheduler & s = Scheduler::instance();
    hdr_bt *bh = HDR_BT(p);
    int clk;
    bd_addr_t fsaddr;
    bd_addr_t accesscode;
    FH_sequence_type fstype;
    // int fs;
    // double t_since_clk00;
    // double drift;            // trash
    // double offset;
    int hdrMismatch = 0;

    _receiver = bh->receiver;


#if 0
    // Check if the receving is busy receiving another packet.
    if (_in_receiving > 0) {
	return 0;
    }
#endif

    if (_sco_state == SCO_IDLE) {
	switch (state()) {

	case PAGE_SCAN:
	    if (Page_Scan_Type_ == InterlacedSecondPart) {
		clk = _interlaceClk(clkn_);
	    } else {
		clk = clkn_;
	    }
	    fstype = FHPageScan;
	    fsaddr = bd_addr_;

	    if (bh->type != hdr_bt::Id) {
		hdrMismatch = 1;
	    }
	    break;

	case PAGE:
	    clk = clke_;
	    fstype = FHPage;
	    fsaddr = _slave;

	    if (bh->type != hdr_bt::Id) {
		hdrMismatch = 1;
	    }
	    break;

	case MASTER_RESP:
	    // clk = clke_;  // specs says so. Since only clke_1 accounts. set to 2.
	    clk = 2;
	    fstype = FHMasterResp;
	    fsaddr = _slave;

	    if (bh->type != hdr_bt::Id) {
		hdrMismatch = 1;
	    }
	    break;

	case SLAVE_RESP:
	    // clk = clkn_;  // specs says so. Since only clkn_1 accounts. Set to 0.
	    clk = 0;
#ifdef BTDEBUG_000
	    fprintf(BtStat::logstat, BTPREFIX1 "%d slave_rsps_count:%d\n",
		    bd_addr_, slave_rsps_count);
#endif
	    fstype = FHSlaveResp;
	    fsaddr = bd_addr_;

	    if (bh->type != hdr_bt::FHS) {
		hdrMismatch = 1;
	    }
	    break;

	case NEW_CONNECTION_MASTER:
	case NEW_CONNECTION_SLAVE:
	case RS_NEW_CONNECTION_SLAVE:
	case RS_NEW_CONNECTION_MASTER:
	case ROLE_SWITCH_MASTER:
	case ROLE_SWITCH_SLAVE:
	case UNPARK_SLAVE:
	case CONNECTION:

#if 0
	    // this is necessary only for receiving access req from parked slaves
	    // out of dated!!
	    // I'm going to remove Park mode.
	    if (bh->type == hdr_bt::Id) {
		return 0;
	    }
#endif

	    if (state() == NEW_CONNECTION_MASTER) {
		clk = clkn_;
		t_clk_00 = t_clkn_00 + BT_CLKN_CLK_DIFF;
	    } else {
		clk = clk_;
	    }
	    fsaddr = master_bd_addr_;
	    fstype = FHChannel;

	    break;

	case RE_SYNC:
	    fsaddr = master_bd_addr_;
	    fstype = FHChannel;
	    clk = clk_;
	    break;

	case INQUIRY_SCAN:
	    if (Inquiry_Scan_Type_ == InterlacedSecondPart) {
		clk = _interlaceClk(clkn_);
	    } else {
		clk = clkn_;
	    }
	    fstype = FHInqScan;
	    fsaddr = lap;

	    if (bh->type != hdr_bt::Id) {
		hdrMismatch = 1;
	    }
	    break;

	case INQUIRY_RESP:
	    if (Inquiry_Scan_Type_ == InterlacedSecondPart) {
		clk = _interlaceClk(clkn_);
	    } else {
		clk = clkn_;
	    }
	    // fstype = FHInqResp;
	    fstype = FHInqScan;	// still listen to inquiry scan fs.
	    fsaddr = lap;

	    if (bh->type != hdr_bt::Id) {
		hdrMismatch = 1;
	    }
	    break;

	case INQUIRY:
	    // FIXME: if slave a replies at first half slot and slave b replies
	    //        at the second half.  The inquirer cannot get the second
	    //        reply.  Moreover, if a slave replies at the second half
	    //        slot.  The inquirer can not send the ID package at the
	    //        first half slot of the following master slot.  Anyway,
	    //        The impact should be minimal.
	    clk = clkn_;
	    fstype = FHInquiry;
	    fsaddr = lap;

	    if (bh->type != hdr_bt::FHS) {
		hdrMismatch = 1;
	    }
	    break;

	default:
	    return 0;
	}
    } else if (_sco_state == SCO_SEND) {
	return 0;
    } else {
	fsaddr = master_bd_addr_;
	fstype = FHChannel;
	clk = clk_;
    }

    accesscode = fsaddr;

    // check if ac match.
    if (bh->ac != accesscode) {
	return 0;
    }
    // TODO: Haven't consider SCO cases yet.
    if (state() == CONNECTION && _afhEnabled && isMaster()) {
	_freq = _transmit_fs;	// same channel mechanism
    } else {
	if (state() == CONNECTION && _afhEnabled) {
	    fstype = FHAFH;
	}
	_freq = FH_kernel(clk, clkf_, fstype, fsaddr);
    }

#if 0
    // For debug purpose only.
    if (trace_all_in_air_ || trace_me_in_air_) {
	bh->dump(BtStat::log, BTINAIRPREFIX, bd_addr_, state_str_s());
	fprintf(BtStat::log,
		" -- fs:%d(p)-%d clk:%d-%d ac:%d-%d clk00:%f %f\n",
		bh->fs_, _freq,
		bh->clk, clk, bh->ac, accesscode, t_clk_00, t_clkn_00);
#if 0
	fprintf(BtStat::log, " -- %d %d %d %d %d %d %d %d %d\n",
		FH_kernel(clk - 4, clkf_, fstype, fsaddr),
		FH_kernel(clk - 3, clkf_, fstype, fsaddr),
		FH_kernel(clk - 2, clkf_, fstype, fsaddr),
		FH_kernel(clk - 1, clkf_, fstype, fsaddr),
		FH_kernel(clk - 0, clkf_, fstype, fsaddr),
		FH_kernel(clk + 1, clkf_, fstype, fsaddr),
		FH_kernel(clk + 2, clkf_, fstype, fsaddr),
		FH_kernel(clk + 3, clkf_, fstype, fsaddr),
		FH_kernel(clk + 4, clkf_, fstype, fsaddr));
#endif
    }
#endif

    // Check if fs match.
    if (bh->fs_ != _freq) {
	if ((bh->receiver == bd_addr_ && state() != PAGE_SCAN &&
	     bh->type != hdr_bt::Id) ||
	    (state() == RE_SYNC && bh->clk % 4 == 0) ||
	    ((trace_all_in_air_ || trace_me_in_air_)
	     && (state() == INQUIRY_SCAN || state() == PAGE_SCAN))) {
	    bh->dump(BtStat::log, 'x', bd_addr_, state_str_s());
	    fprintf(BtStat::log,
		    " -- %f fs:%d(p)-%d clk:%d-%d ac:%d-%d clk00:%f %f\n",
		    s.clock(), bh->fs_, _freq,
		    bh->clk, clk, bh->ac, accesscode, t_clk_00, t_clkn_00);
	}

	if (useSynByGod_ && !isMaster() && state() == CONNECTION
	    && bh->receiver == bd_addr_ && synToChannelByGod(bh)) {
	    drift_clk = 0;
	    clk = clk_;
	} else {
	    return 0;
	}
    }

    if (hdrMismatch) {
	return 2;
    }
    // Check if it is addressed to me.
    // Note that master should process any packet it receives
    // Note Id packet does not have am field because it does not has a header
    // But it doesn't matter for simulation purpose since it's set to 0 always,
    // or takes as a broadcast pkt.
    if (isMaster()) {
	if (lmp_->curPico && state() == CONNECTION) {
#if 0
	    if (!ltAddrIsValid(bh->lt_addr)) {
		// This is ok.  Maybe the link is just suspended at the moment.
		fprintf(BtStat::logstat, "** %d recving invalid LT_ADDR, ",
			bd_addr_);
		bh->dump(BtStat::logstat, BTERRPREFIX, bd_addr_,
			 state_str_s());
		return 2;
	    }
#endif
	    if (bh->lt_addr != _polled_lt_addr) {
		fprintf(BtStat::logstat, "** %d recving from slave not "
			"polled: %d, expect %d ",
			bd_addr_, bh->lt_addr, _polled_lt_addr);
		bh->dump(BtStat::logstat, BTERRPREFIX, bd_addr_,
			 state_str_s());
		return 2;
	    }
	}
    } else {
	if (state() == RE_SYNC) {
	    re_sync();
	} else if (state() == CONNECTION && drift_clk == 0) {
	    // Synchronize to the channel
	    double drift = s.clock() - t_clk_00 - BTDELAY;
	    if (drift < -MAX_SLOT_DRIFT || drift > MAX_SLOT_DRIFT) {
		if (drift < BTSlotTime - MAX_SLOT_DRIFT ||
		    drift > BTSlotTime + MAX_SLOT_DRIFT) {
		    fprintf(BtStat::log,
			    "Werid drift %d :%f t_clk_00: %f now: %f %d\n",
			    bd_addr_, drift, t_clk_00, s.clock(), clk_);
		    // bh->dump(BtStat::log, '-', bd_addr_, state_str_s());
		}
	    } else {
		drift_clk = -drift;
	    }
	}
	// Check if I'm the intended receiver
	if (bh->lt_addr != 0 && bh->lt_addr != lt_addr) {
	    return 2;
	}
    }

    // force receive once every Slot
    if (s.clock() - _lastRecvT < SlotTime) {
	return 0;
    }
    _lastRecvT = s.clock();

    activeBuffer = lookupTxBuffer(bh);

    _in_receiving = tranceiveSlots(bh);
    _polling_clk = clk + _in_receiving;

    // we need to record some slave parameters at this moment
    // we generate slave reply package here.  Later on, if the received package
    // pass the collision and possible loss test, the reply is scheduled for
    // transmitting.  Then at transmitting time, we check if free slot
    // available for transmitting it.
    switch (state()) {
    case PAGE_SCAN:
	if (Page_Scan_Type_ == InterlacedSecondPart) {
	    clkf_ = _interlaceClk(clkn_);
	} else {
	    clkf_ = clkn_;
	}
	slave_rsps_count = 0;
	slave_rsps_count_incr_slot = (clkn_ + 3) & 0x03;
	bh->reply = genIdPacket(FHSlaveResp, bd_addr_, 2, bh->sender);
	HDR_BT(bh->reply)->comment("SR");
	HDR_BT(bh->reply)->nextState = SLAVE_RESP;
	break;

    case SLAVE_RESP:
	// Note:  upon receiving FHS packet from the master, the slave
	// reply with a ID package and enter CONNECTION state.  If the
	// replying ID package lost, the slave can't receive from the 
	// master any more until timeout to force them back to paging and
	// page scan states.  Anyway, ID package is supposed to be robust,
	// and it never gets lost in the effective range. 
	bh->reply = genIdPacket(FHSlaveResp, bd_addr_, 2, bh->sender);
	HDR_BT(bh->reply)->comment("FR");
	HDR_BT(bh->reply)->nextState = NEW_CONNECTION_SLAVE;
	break;

    case INQUIRY_SCAN:
	if (ver_ < 12) {
	    break;
	}
	if (Inquiry_Scan_Type_ == InterlacedSecondPart) {
	    clk = _interlaceClk(clkn_);
	} else {
	    clk = clkn_;
	}
	bh->reply =
	    genFHSPacket(FHInqResp, lap, clk + 2, bd_addr_, 0, bh->sender);
	HDR_BT(bh->reply)->comment("QR");
	inquiry_rsps_count++;
	// HDR_BT(bh->reply)->nextState = INVALID;  // does't change
	// HDR_BT(bh->reply)->nextState = INQUIRY_SCAN;
	break;

    case INQUIRY_RESP:
	if (Inquiry_Scan_Type_ == InterlacedSecondPart) {
	    clk = _interlaceClk(clkn_);
	} else {
	    clk = clkn_;
	}
	bh->reply =
	    genFHSPacket(FHInqResp, lap, clk + 2, bd_addr_, 0, bh->sender);
	HDR_BT(bh->reply)->comment("QR");
	inquiry_rsps_count++;
	// HDR_BT(bh->reply)->nextState = STANDBY;  // does't change
	HDR_BT(bh->reply)->nextState = INQUIRY_SCAN;
	break;

    case NEW_CONNECTION_SLAVE:
    case RS_NEW_CONNECTION_SLAVE:
	bh->reply = genNullPacket(FHChannel, master_bd_addr_,
				  clk_ + 2, master_bd_addr_);
	HDR_BT(bh->reply)->lt_addr = lt_addr;
	HDR_BT(bh->reply)->comment("NR");
	HDR_BT(bh->reply)->nextState = INVALID;	// does't change
	break;

    case CONNECTION:
	if (_afhEnabled && !isMaster()) {
	    _transmit_fs = bh->fs_;	// same channel mechanism
	}
#if 0
	// check if receiving FHS to be taken over by new master
	if (bh->type != hdr_bt::FHS) {
	    break;
	}
#endif
	break;

    case ROLE_SWITCH_SLAVE:
	bh->reply = genIdPacket(FHChannel, master_bd_addr_,
				clk_ + 2, _slave);
	HDR_BT(bh->reply)->lt_addr = 0;
	HDR_BT(bh->reply)->comment("i");
	HDR_BT(bh->reply)->nextState = RS_NEW_CONNECTION_SLAVE;
	break;

    case PAGE:
	clkf_ = clke_;		// froze clock clkn16_12
	break;

    case NEW_CONNECTION_MASTER:
	clk_ = clkn_;
	t_clk_00 = t_clkn_00 + BT_CLKN_CLK_DIFF;
	break;
    default:
	break;
    }

    return 1;
}

//  slave send response in 6 cases:
//  send id in PAGE_SCAN -- slave response
//  send id in SLAVE_RESP -- reply to FHS
//  send fhs in INQUIRY_RESP
//  send null in NEW_CONNECTION_SLAVE
//  send id in ROLE_SWITCH_SLAVE
//  send null in RS_NEW_CONNECTION_SLAVE
//  special care need to take of the schduling problem, since the sched_word
//     was aligned to clkn_ .
void Baseband::_slave_reply(hdr_bt * bh)
{
    Scheduler & s = Scheduler::instance();
    Packet *p = bh->reply;

    if (!p) {
	fprintf(BtStat::logstat,
		BTPREFIX1
		"%d Baseband::_slave_reply(): reply with NULL pointer ",
		bd_addr_);
	bh->dump(BtStat::log, BTRXPREFIX, bd_addr_, state_str_s());	// "r "
	return;
    }

    switch (state()) {

    case SLAVE_RESP:
	HDR_BT(p)->clk = clkn_;
	break;
    case PAGE_SCAN:
	HDR_BT(p)->clk = clkf_ + slave_rsps_count;
	break;
    case ROLE_SWITCH_SLAVE:
    case CONNECTION:		// being taken over by a new master

    case INQUIRY_RESP:
    case NEW_CONNECTION_SLAVE:
    case RS_NEW_CONNECTION_SLAVE:
	break;

    default:
	fprintf(stderr, "OOOOps... %d slave sends in unknown state.\n",
		bd_addr_);
    }

    bh->reply = 0;
    // s.schedule(&slaveSendnonConnHandler, p, SlotTime - BTDELAY - bh->txtime());
    s.schedule(&slaveSendnonConnHandler, p, SlotTime - bh->txtime());
}

// slave send response packet.  The timing is a little tricky, since the
// clock is not synchronized and the slave transmitting has to be adapted
// to the master's clock.  ie. reply exactly 1 slot later after receiving the
// first bit.
void Baseband::slave_send_nonconn(Packet * p)
{

#if 0
    // Let's leave this job to the sender.  The sender should ensure this 
    // is enough time for the reply to be transmitted.

    // check if free slot available to transmit
    if (_sco_state == SCO_SEND || _sco_state == SCO_RECV ||
	(_sco_state == SCO_PRIO && (Scheduler::instance().clock()
				    + HDR_BT(p)->txtime() >
				    _sco_slot_start_time))) {
	Packet::free(p);
	return;
    }
#endif

    BBState st = (BBState) HDR_BT(p)->nextState;
    sendBBSig(p);

/*
    if (st == PAGE_SCAN) {	// PageScan after inq Resp
	discoverable = 0;
    }
*/

    if (st != INVALID) {
	change_state(st);
    }
/*
    // disable Page Scan timer
    if (st == SLAVE_RESP) {
	connectable = 0;
    }
*/
}

int Baseband::collision(hdr_bt * bthdr)
{
    return _interfQ[bthdr->fs_].collide(this, bthdr);
}

void Baseband::setScoLTtable(LMPLink ** t)
{
    for (int i = 0; i < 6; i++) {
	_lt_sco_table[i] = (t[i] ? t[i]->txBuffer : NULL);
    }
}

TxBuffer *Baseband::lookupTxBuffer(hdr_bt * bh)
{
    if (!lmp_->curPico || bh->lt_addr == 0) {
	return NULL;
    }
    return lmp_->curPico->lookupTxBuffer(bh);
}

// find the txBuffer for the SCO piconet link
TxBuffer *Baseband::lookupScoTxBuffer()
{
    return _lt_sco_table[(clk_ % 24) / 4];
}

// Recving functions are invoked twice: when the first bit arrives, check
// if this bb can receive it; when the last bit arrives, check if 
// collision occurs and possible lost.

// The first bit arrives.
void Baseband::recv_handle(Packet * p)
{
    Scheduler & s = Scheduler::instance();
    hdr_bt *bh = HDR_BT(p);
    int st;
    // double t;

/*
    if (_in_receiving > 0 || _in_transmit > 0) {
	Packet::free(p);
	return;
    }
*/

    if (_trx_st != RX_ON) {
	// bh->dump(BtStat::log, 'd', bd_addr_, state_str_s());
	// Packet::free(p);
	// return;
    } else {
	// bh->dump(BtStat::log, 'y', bd_addr_, state_str_s());
    }

    if ((st = recv_filter(p)) == 1) {	// check if I can received it.
	// Check collison and loss when last bit finishes.
	// s.schedule(&recvHandler, p, bh->txtime());
	_rxPkt = p;
	s.schedule(&recvHandlerLastbit, p, bh->txtime() - BTDELAY);
	s.cancel(&_trxoff_ev);
	_trxoff_ev.st = _trx_st;
	s.schedule(&_trxtoTimer, &_trxoff_ev,
		   bh->txtime() + trxTurnarndTime_);
    } else {
	Packet::free(p);
	if (st == 2) {
	    s.cancel(&_trxoff_ev);
	    _trxoff_ev.st = _trx_st;
	    s.schedule(&_trxtoTimer, &_trxoff_ev, 126E-6);
	}
    }
}

// The last bit finishes.
void Baseband::recv_handle_lastbit(Packet * p)
{
    Scheduler & s = Scheduler::instance();
    hdr_bt *bh = HDR_BT(p);
    double t;

    _rxPkt = NULL;

    if (_trx_st != RX_ON) {
	fprintf(BtStat::log, "%d ***RX OFF:%d %f when receiving: ",
		bd_addr_, _trx_st, s.clock());
	bh->dump(BtStat::log, '?', bd_addr_, state_str_s());
#if 1
	if (bh->reply) {
	    Packet::free(bh->reply);
	}
	Packet::free(p);
	return;
#endif
    }

	_trxoff_ev.clearSt();
	turn_off_trx();

    if (collision(bh)) {
	if (bh->reply) {
	    Packet::free(bh->reply);
	}
	Packet::free(p);
	return;
    }
    // at the time of switch piconet, receving maybe interrupted.
    if (_in_receiving <= 0 && state() == CONNECTION) {
	bh->dump(BtStat::log, BTLOSTPREFIX, bd_addr_, state_str_s());
	fprintf(BtStat::log, "%d _in_receiving:%d\n", bd_addr_,
		_in_receiving);
#if 1
	if (bh->reply) {
	    Packet::free(bh->reply);
	}
	Packet::free(p);
	return;
#endif
    }

    if (lossmod->lost(this, bh)) {
	// fprintf(BtStat::log, "%d ", bd_addr_);
	bh->dump(BtStat::log, BTLOSTPREFIX, bd_addr_, state_str_s());
	if (bh->reply) {
	    Packet::free(bh->reply);
	}
	Packet::free(p);
	return;
    }

    int tr = 0;
    if ((trace_me_null_ || trace_all_null_) && bh->type == hdr_bt::Null) {
	tr = 1;
    }
    if ((trace_me_poll_ || trace_all_poll_) && bh->type == hdr_bt::Poll) {
	tr = 1;
    }
    if ((trace_all_rx_ == 1 || trace_me_rx_ == 1)
	&& bh->type != hdr_bt::Null && bh->type != hdr_bt::Poll) {
	tr = 1;
    }
    if (tr) {
	bh->dump(BtStat::log, BTRXPREFIX, bd_addr_, state_str_s());
	if (bh->extinfo >= 10) {
	    bh->dump_sf();
	}
    }

    if (_sco_state == SCO_SEND) {
	if (bh->reply) {
	    Packet::free(bh->reply);
	}
	Packet::free(p);
	return;
    } else if (_sco_state == SCO_RECV) {
	bh->txBuffer = lookupScoTxBuffer();
	if (bh->txBuffer && bh->txBuffer->handle_recv(p)) {
	    if (bh->txBuffer->dstTxSlot() < MinTxBufferSlot) {
		bh->txBuffer->dstTxSlot(bh->srcTxSlot);
	    }
	    uptarget_->recv(p, (Handler *) 0);
	} else {
	    if (bh->reply) {
		Packet::free(bh->reply);
	    }
	    Packet::free(p);
	}
	return;
    }


    switch (state()) {

    case PAGE_SCAN:
	_slave_reply(bh);
	break;

    case SLAVE_RESP:

	clk_ = bh->u.fhs.clk;
	master_bd_addr_ = bh->u.fhs.bd_addr;
	lt_addr = bh->u.fhs.lt_addr;

	if (clk_ev.uid_ > 0) {
	    fprintf(stdout, "%d %s cancel clk\n", bd_addr_, __FUNCTION__);
	    s.cancel(&clk_ev);
	}
	fprintf(BtStat::log, "%f sched clk at %f by %s (SLAVE_RESP)\n",
		s.clock(),
		s.clock() + SlotTime - bh->txtime() + BT_CLKN_CLK_DIFF,
		__FUNCTION__);
	s.schedule(&clk_handler, &clk_ev,
		   SlotTime - bh->txtime() + BT_CLKN_CLK_DIFF);
	_slave_reply(bh);
	break;

    case ROLE_SWITCH_SLAVE:
	clk_ = bh->u.fhs.clk;
	master_bd_addr_ = bh->u.fhs.bd_addr;
	lt_addr = bh->u.fhs.lt_addr;

	if (clk_ev.uid_ > 0) {
	    fprintf(stdout, "%d %s cancel clk\n", bd_addr_, __FUNCTION__);
	    s.cancel(&clk_ev);
	}

	t = SlotTime + SlotTime - bh->txtime() + slot_offset * 1E-6;
	t += BT_CLKN_CLK_DIFF;
	clk_ += 6;

	fprintf(BtStat::log,
		"%f sched clk at %f by %s (ROLE_SWITCH_SLAVE)\n",
		s.clock(), s.clock() + t, __FUNCTION__);
	s.schedule(&clk_handler, &clk_ev, t);
	fprintf(BtStat::logstat,
		BTPREFIX1 "RS_slave: clk_:%d master:%d lt:%d t:%f\n", clk_,
		master_bd_addr_, lt_addr, t + s.clock());
	_slave_reply(bh);
	break;

    case ROLE_SWITCH_MASTER:
	if (bh->type == hdr_bt::Id) {
	    newconnectionTimer = newconnectionTO;
	    change_state(RS_NEW_CONNECTION_MASTER);

	    master_bd_addr_ = bd_addr_;
	    isMaster_ = 1;
	    if (clk_ev.uid_ > 0) {
		fprintf(stdout, "%d %s cancel clk\n", bd_addr_,
			__FUNCTION__);
		s.cancel(&clk_ev);
	    }
	    double t = SlotTime - bh->txtime() + slot_offset * 1E-6 +
		BT_CLKN_CLK_DIFF;
	    clk_ = (clkn_ & 0xFFFFFFFC) + 2;
	    if (t >= SlotTime + SlotTime) {
		clk_ += 4;
	    }
	    fprintf(BtStat::log,
		    "%f sched clk at %f by %s (ROLE_SWITCH_MASTER)\n",
		    s.clock(), s.clock() + t, __FUNCTION__);
	    s.schedule(&clk_handler, &clk_ev, t);

	    fprintf(BtStat::logstat,
		    BTPREFIX1 "RS_master: clk_:%d master:%d t:%f\n", clk_,
		    master_bd_addr_, t + s.clock());
	}
	break;

    case PAGE:
	// clkf_ = clke_;   // froze clock clkn16_12  // move forward
	if (bh->type == hdr_bt::Id) {
	    change_state(MASTER_RESP);
	    master_rsps_count = 0;
	    if (node_->stat_) {
		node_->stat_->add_page_time(s.clock() - _page_start_time);
	    } else {
		fprintf(stderr, "Page time: %f %d\n",
			s.clock() - _page_start_time,
			int ((s.clock() - _page_start_time) / BTSlotTime +
			     0.5));
	    }
	}
	break;

    case RS_NEW_CONNECTION_MASTER:
	change_state(CONNECTION);
	_stable_state = CONNECTION;

	lmp_->role_switch_bb_complete(_slave, 1);
	break;

    case RS_NEW_CONNECTION_SLAVE:
	if (bh->type == hdr_bt::Poll && bh->reply) {
	    _slave_reply(bh);

	    change_state(CONNECTION);
	    _stable_state = CONNECTION;
	    isMaster_ = 0;
	    comp_clkoffset(bh->txtime(), &clock_offset, &slot_offset);
	    lmp_->role_switch_bb_complete(_slave, 1);
	}
	break;

    case NEW_CONNECTION_SLAVE:
	if (bh->type == hdr_bt::Poll && bh->reply) {
	    _slave_reply(bh);

	    connectable = 0;
	    change_state(CONNECTION);
	    _stable_state = CONNECTION;
	    isMaster_ = 0;
	    fprintf(BtStat::logstat, BTPREFIX1 "Slave lt_addr: %d\n",
		    lt_addr);
	    int clkoffset;
	    int sltoffset;
	    comp_clkoffset(bh->txtime(), &clkoffset, &sltoffset);
	    lmp_->connection_ind(master_bd_addr_, lt_addr,
				 clkoffset, sltoffset);
	    // Why return clkoffset, sltoffset since they change over time.
	    inquiryScan_cancel();
	}
	break;

    case NEW_CONNECTION_MASTER:
	change_state(CONNECTION);
	_stable_state = CONNECTION;
	if (clk_ev.uid_ > 0) {
	    fprintf(stdout, "%d %s cancel clk\n", bd_addr_, __FUNCTION__);
	    s.cancel(&clk_ev);
	}

	fprintf(BtStat::log,
		"%f sched clk at %f by %s (NEW_CONNECTION_MASTER)\n",
		s.clock(),
		s.clock() + SlotTime - bh->txtime() + BT_CLKN_CLK_DIFF,
		__FUNCTION__);
	s.schedule(&clk_handler, &clk_ev, SlotTime - bh->txtime()
		   + BT_CLKN_CLK_DIFF);
	t_clk_00 = t_clkn_00 + BT_CLKN_CLK_DIFF;
	clk_ = (clkn_ & 0xFFFFFFFC) + 2;
	master_bd_addr_ = bd_addr_;
	isMaster_ = 1;
	lmp_->page_complete(_slave, 1);
	break;

    case MASTER_RESP:
	if (bh->type == hdr_bt::Id) {
	    newconnectionTimer = newconnectionTO;
	    change_state(NEW_CONNECTION_MASTER);
	    clk_ = clkn_;
	    master_bd_addr_ = bd_addr_;
	}
	break;

    case INQUIRY_RESP:
	if (bh->type == hdr_bt::Id && bh->reply) {
	    if (pagescan_after_inqscan) {
		connectable = 1;
		HDR_BT(bh->reply)->nextState = PAGE_SCAN;
		lmp_->HCI_Write_Page_Scan_Activity(4096, 4096);
		discoverable = 0;
	    }
	    _slave_reply(bh);
	    if (trace_all_stat_ || trace_me_stat_) {
		fprintf(BtStat::logstat, BTPREFIX1
			"%d send INQUIRY_RESP.\n", bd_addr_);
	    }
	}
	break;

    case INQUIRY_SCAN:
	if (bh->type == hdr_bt::Id) {
	    inBackoff = 1;
	    backoffTimer = Random::integer(backoffParam);
	    if (trace_all_stat_ || trace_me_stat_) {
		fprintf(BtStat::logstat, BTPREFIX1 "%d backoffTimer:%d\n",
			bd_addr_, backoffTimer);
	    }
	    if (ver_ < 12) {
		change_state(_stable_state);
	    } else {
		_state = INQUIRY_RESP;	// for FH purpose
		HDR_BT(bh->reply)->nextState = _stable_state;
		if (pagescan_after_inqscan) {
		    connectable = 1;
		    HDR_BT(bh->reply)->nextState = PAGE_SCAN;
		    lmp_->HCI_Write_Page_Scan_Activity(4096, 4096);
		    discoverable = 0;
		}
		_slave_reply(bh);
		if (trace_all_stat_ || trace_me_stat_) {
		    fprintf(BtStat::logstat, BTPREFIX1
			    "%d send INQUIRY_RESP.\n", bd_addr_);
		}
	    }
	}
	break;

    case INQUIRY:
	if (bh->type == hdr_bt::FHS) {
	    handle_inquiry_response(bh);
	    if (trace_all_stat_ || trace_me_stat_) {
		fprintf(BtStat::logstat, BTPREFIX1
			"%d recv INQUIRY_RESP.\n", bd_addr_);
	    }
	}
	break;


    case STANDBY:
	break;

    default:			// CONNECTION

	// being taken over by a new master
	if (bh->type == hdr_bt::FHS && !isMaster() && bh->reply
	    && state() == CONNECTION) {
	    clk_ = bh->u.fhs.clk;
	    master_bd_addr_ = bh->u.fhs.bd_addr;
	    lt_addr = bh->u.fhs.lt_addr;

	    if (clk_ev.uid_ > 0) {
		fprintf(stdout, "%d %s cancel clk\n", bd_addr_,
			__FUNCTION__);
		s.cancel(&clk_ev);
	    }

	    t = SlotTime - bh->txtime() + SlotTime + slot_offset * 1E-6;
	    // FIXME
	    // BT_CLKN_CLK_DIFF ??
	    clk_ += 6;

	    s.schedule(&clk_handler, &clk_ev, t);
	    fprintf(BtStat::logstat,
		    BTPREFIX1 "RS_slave: clk_:%d master:%d t:%f\n", clk_,
		    master_bd_addr_, t + s.clock());
	    fprintf(BtStat::logstat,
		    "%f start clk at %f\n", s.clock(), t + s.clock());

	    _slave_reply(bh);
	    break;
	}

	if (isMaster() && _inBeacon && bh->type == hdr_bt::Id) {
	    int tick = (int) ((s.clock() - _beacon_instant) / Tick);
	    lmp_->unpark_req(tick);
	    break;
	}

	if (bh->lt_addr == 0) {	// broadcasting pkt
	    bh->bcast = 1;

	    if (state() == UNPARK_SLAVE) {	// allowed to response next slot.
	    }
	    // set sender to the master ??
	    uptarget_->recv(p, (Handler *) 0);
	    return;

	} else if ((bh->txBuffer = lookupTxBuffer(bh))) {
	    if (bh->txBuffer->dstTxSlot() < MinTxBufferSlot) {
		bh->txBuffer->dstTxSlot(bh->srcTxSlot);
	    }
	    // printf("txBuffer->slot(): %d\n", bh->txBuffer->slot());
	    if (bh->txBuffer->handle_recv(p)) {
		uptarget_->recv(p, (Handler *) 0);
		return;
	    }
	}
    }

    if (bh->reply) {
	Packet::free(bh->reply);
    }
    Packet::free(p);
}

// prev state: STANDBY / CONNECTION
// next state: Slave-Response
//
// sched word in page_scan mode:
//
// the slave's transmission is aligned to the master's clock by recording
// the time the first bit arrives.  It can transmit anytime unless colliding
// to a SCO link. So the transmission is determined by SCO link state
// instead of a sched_word of its own.
//
// The following variables should be set before calling this function.
//      T_w_page_scan_ default to 36 (18 slots or 11.25ms)
//      T_page_scan_ default to 4096 (2048 slots or 1.28s)
//
// SR mode:         T_page_scan_
//      R0      <=1.28s & = T_w_page_scan_
//      R1      <=1.28s
//      R2      <=2.56s
//
void Baseband::page_scan(BTSchedWord * sched_word)
{
    connectable = 1;

    // at next clkn event, it decreases to 0 and triggers setting timer
    T_page_scan_timer = 1;
    isMaster_ = 0;
    set_sched_word(sched_word);

    fprintf(BtStat::logstat,
	    BTPREFIX1 "%d start page scan. clkn:%d \n", bd_addr_, clkn_);
    lmp_->dump(BtStat::logstat, 1);
}

void Baseband::page_scan_cancel()
{
    if (connectable) {
	connectable = 0;
	if (state() == PAGE_SCAN) {
	    change_state(_stable_state);
	}
    }

	_trxoff_ev.clearSt();
	turn_off_trx();
}

// The spec only specify N_inquiry_ and N_page_ for HV3 or EV3 links,
// that is, T_poll = 6.
// suppose that numSco <= 2.
// A HV2 link counts as 2 HV3 links here, although it take 1/2 load instead
// of 2/3 loads of two HV3 links.  Anyway, the spec doesn't say anything
// about HV2 links.
void Baseband::setNInquiry(int numSco)
{
    N_inquiry_ = 256 * (numSco + 1);
}

void Baseband::setNPage(int numSco)
{
    int base = (SR_mode_ == 0 ? 1 : SR_mode_ * 128);
    N_page_ = base * (numSco + 1);
}

// prev state: STANDBY / CONNECTION
// next state: Master-Response
// Terminate: pageTO / receive response
void Baseband::page(bd_addr_t slave, uint8_t ps_rep_mod, uint8_t psmod,
		    int16_t clock_offset, BTSchedWord * sched_word)
// void Baseband::page(Bd_info * bd, ConnectionHandle * connhand, int timeout,
//                  BTSchedWord * sched_word)
{
    // N_page: repeat trainType times
    // SR mode      N_page (no SCO)  one SCO        two SCO
    //   R0           >= 1            >=2           >=3
    //   R1           >= 128          >=256         >=384
    //   R2           >= 256          >=512         >=768

    // page_conn_handle = connhand;
    _slave = slave;
    page_sr_mod = ps_rep_mod;
    page_ps_mod = psmod;
    // pageTO_ = timeout;
    page_timer = pageTO_;
    page_train_switch_timer = N_page_ * 32;
    set_sched_word(sched_word);

#if 0
    /* specs says that the highest bit of bd->offset indicates if it's valid
     * and the last 2 bits of clke_ should always equal to last 2 bits of
     * clkn_.
     */
    int offset = 0;
    if (bd->offset & 0x8000) {
	offset = (bd->offset & 0x7FFF) << 2;
    }
    clke_ = clkn_ + offset;
#endif

    // clke_ = clkn_ + (clock_offset & 0xfffc);
    clke_ = clkn_ + (clock_offset & 0xFFFFFFFC);
    fprintf(BtStat::logstat,
	    BTPREFIX1 "%d start to page %d. clkn:%d, offset:%d, clke:%d\n",
	    bd_addr_, slave, clkn_, clock_offset, clke_);
    lmp_->dump(BtStat::logstat, 1);
    train_ = Train_A;
    change_state(PAGE);
    // notices that PAGE should not be interrupted (expt SCO), otherwise
    // it cannot get back, unlike PAGE SCAN
    isMaster_ = 1;

    _page_start_time = Scheduler::instance().clock();
}

// bypass pageTO_
void Baseband::page(bd_addr_t slave, uint8_t ps_rep_mod, uint8_t psmod,
		    int16_t clock_offset, BTSchedWord * sched_word, int to)
{
    page(slave, ps_rep_mod, psmod, clock_offset, sched_word);
    page_timer = to;
}

void Baseband::page_cancel()
{
    if (state() != PAGE) {
	return;
    }
    // page_timer = 1;  // reset timer to force it run out
    _trxoff_ev.clearSt();
    turn_off_trx();
    change_state(_stable_state);
}

// The following variables should be set before calling this function.
//      lap = giac
//      T_w_inquiry_scan_ default to 36 (18 slots or 11.25ms)
//      T_inquiry_scan_ default to 4096 (2048 slots or 1.28s)
void Baseband::inquiry_scan(BTSchedWord * sched_word)
{
    discoverable = 1;
    T_inquiry_scan_timer = 1;	// let the clkn_ routine to set it up
    inquiry_rsps_count = 0;	// Is it necessary to reset N to 0 ??
    set_sched_word(sched_word);
}

void Baseband::inquiryScan_cancel()
{
    if (inBackoff) {
	inBackoff = 0;
    }
    if (state() == INQUIRY_RESP) {
	change_state(_stable_state);
    }
    if (discoverable) {
	discoverable = 0;
	if (state() == INQUIRY_SCAN) {
	    change_state(_stable_state);
	}
    }

	_trxoff_ev.clearSt();
	turn_off_trx();
}

void Baseband::inquiry_cancel()
{
    if (state() != INQUIRY) {
	return;
    }
    // Specs says don't return anything to upper layer.
    _trxoff_ev.clearSt();
    turn_off_trx();
    change_state(_stable_state);
}

void Baseband::inquiry(int Lap, int inquiry_length, int num_responses,
		       BTSchedWord * schedword)
{
    //              no SCO          one SCO         two SCO
    //  N_inquiry    >=256            >=512           >=768
    //  
    //  at least 3 tran switches
    //
    change_state(INQUIRY);
    // _inqParam.periodic = 0;
    lap = Lap;
    inq_max_num_responses_ = num_responses;
    inq_num_responses_ = 0;
    inquiryTO_ = inquiry_length;	// 1.28 - 61.44s
    inq_timer = inquiry_length;	// 1.28 - 61.44s
    discoved_bd = 0;
    inquiry_train_switch_timer = N_inquiry_ * 32;
    set_sched_word(schedword);
    train_ = Train_A;

    _inq_start_time = Scheduler::instance().clock();
}

void Baseband::handle_inquiry_response(hdr_bt * bh)
{
    double now = Scheduler::instance().clock();
    Bd_info *bd =
	new Bd_info(bh->u.fhs.bd_addr, bh->u.fhs.clk, bh->u.fhs.clk -
		    (clkn_ & 0xFFFFFFFC));
    bd->last_seen_time = now;
    bd->dist = distance(bh->X_, bh->Y_);
    bd->dump();

    int isnew;
    bd = lmp_->_add_bd_info(bd, &isnew);	// old bd may got deleted.

    if (page_after_inq) {
	// page(bd->bd_addr, 0, 0, bd->offset, _sched_word);
	node_->bnep_->connect(bd->bd_addr);
	return;
    }

    if (!isnew) {		// bd is not newly discovered.
	return;
    }

    if (++inq_num_responses_ == inq_max_num_responses_) {
	change_state(_stable_state);
	lmp_->inquiry_complete(inq_num_responses_);	// may change _state.
    }

    double it = now - _inq_start_time;
    if (node_->stat_) {
	node_->stat_->add_inq_time(it, inq_num_responses_);
    } else {
	fprintf(stderr, "Inq time: %f %d num: %d ave: %f %d\n", it,
		int (it / BTSlotTime + 0.5), inq_num_responses_,
		it / inq_num_responses_,
		int (it / BTSlotTime / inq_num_responses_ + 0.5));
    }
}

Packet *Baseband::_allocPacket(FH_sequence_type fs, bd_addr_t addr,
			       clk_t clk, bd_addr_t recv,
			       hdr_bt::packet_type t)
{
    Packet *p = Packet::alloc();
    hdr_bt *bh = HDR_BT(p);

    bh->pid = hdr_bt::pidcntr++;
    bh->ac = addr;
    bh->lt_addr = 0;
    bh->type = t;

    bh->sender = bd_addr_;
    bh->receiver = recv;
    bh->srcTxSlot = bh->dstTxSlot = 0;

    bh->fs_ = FH_kernel(clk, clkf_, fs, addr);
    bh->clk = clk;
    bh->size = hdr_bt::packet_size(t);
    bh->transmitCount = 0;
    bh->comment("");
    bh->nextState = STANDBY;

    bh->X_ = X();
    bh->Y_ = Y();
    // bh->Z_ = Z();

    return p;
}

Packet *Baseband::genIdPacket(FH_sequence_type fs, bd_addr_t addr,
			      clk_t clk, bd_addr_t recv)
{
    return _allocPacket(fs, addr, clk, recv, hdr_bt::Id);
}

Packet *Baseband::genFHSPacket(FH_sequence_type fs, bd_addr_t addr,
			       clk_t clk, bd_addr_t myaddr, int am,
			       bd_addr_t recv)
{
    Packet *p = _allocPacket(fs, addr, clk, recv, hdr_bt::FHS);
    hdr_bt *bh = HDR_BT(p);
    bh->u.fhs.bd_addr = myaddr;
    bh->u.fhs.clk = clk & 0xFFFFFFFC;
    bh->u.fhs.lt_addr = am;

    return p;
}

Packet *Baseband::genPollPacket(FH_sequence_type fs, bd_addr_t addr,
				clk_t clk, bd_addr_t recv)
{
    return _allocPacket(fs, addr, clk, recv, hdr_bt::Poll);
}

Packet *Baseband::genNullPacket(FH_sequence_type fs, bd_addr_t addr,
				clk_t clk, bd_addr_t recv)
{
    return _allocPacket(fs, addr, clk, recv, hdr_bt::Null);
}

// Baseband frequency hopping kernel
// bd_addr is LAP + 4 LSB bits of UAP = 28 bits
int Baseband::FH_kernel(clk_t CLK, clk_t CLKF,
			FH_sequence_type FH_seq, bd_addr_t bd_addr)
{
    int X, Y1, Y2, A, B, C, D, E, F;	// see specs for the meanings
    int Z, P;
    int Ze[5];			// Z expanded
    int reg_bank_index;
    int Xp79, Xprs79, Xprm79, Xir79;
    // unsigned int Xi79;
    int k_offset = (train_ == Train_A ? 24 : 8);
    int N;
    int i;
    int ret;

    A = (bd_addr >> 23) & 0x1F;
    B = (bd_addr >> 19) & 0x0F;
    C = ((bd_addr >> 4) & 0x10)
	+ ((bd_addr >> 3) & 0x08)
	+ ((bd_addr >> 2) & 0x04) + ((bd_addr >> 1) & 0x02) +
	(bd_addr & 0x01);
    D = (bd_addr >> 10) & 0x01FF;
    E = ((bd_addr >> 7) & 0x40)
	+ ((bd_addr >> 6) & 0x20)
	+ ((bd_addr >> 5) & 0x10)
	+ ((bd_addr >> 4) & 0x08)
	+ ((bd_addr >> 3) & 0x04) + ((bd_addr >> 2) & 0x02) +
	((bd_addr >> 1) & 0x01);
    F = 0;

    switch (FH_seq) {
    case FHPage:
    case FHInquiry:		// Xi79 == Xp79
	// a offset 64 added to make sure it the difference is positive.
	// It does change the result and make output to agree with
	// the reference result in the specification.
	Xp79 = (((CLK >> 12) & 0x1F) + k_offset +
		(((CLK >> 1) & 0x0E) + (CLK & 0x01) + 64 -
		 ((CLK >> 12) & 0x1F)) % 16) % 32;
	X = Xp79 & 0x1F;
	Y1 = (CLK >> 1) & 0x01;
	Y2 = Y1 * 32;
	break;
    case FHPageScan:
	X = (CLK >> 12) & 0x1F;
	Y1 = 0;
	Y2 = 0;
	break;
    case FHMasterResp:
	N = master_rsps_count;
	Xprm79 = (((CLKF >> 12) & 0x1F) + k_offset +
		  (((CLKF >> 1) & 0x0E) + (CLKF & 0x01) + 64 -
		   ((CLKF >> 12) & 0x1F)) % 16 + N) % 32;
	X = Xprm79 & 0x1F;
	Y1 = (CLK >> 1) & 0x01;
	Y2 = Y1 * 32;
	break;
    case FHSlaveResp:
	N = slave_rsps_count;
	Xprs79 = (((CLKF >> 12) & 0x1F) + N) % 32;
	X = Xprs79 & 0x1F;
	Y1 = (CLK >> 1) & 0x01;
	Y2 = Y1 * 32;
	break;
    case FHInqScan:
	N = inquiry_rsps_count;
	Xir79 = (((CLK >> 12) & 0x1F) + N) % 32;
	X = Xir79 & 0x1F;
	Y1 = 0;
	Y2 = 0;
	break;
    case FHInqResp:
	N = inquiry_rsps_count;
	Xir79 = (((CLK >> 12) & 0x1F) + N) % 32;
	X = Xir79 & 0x1F;
	Y1 = 1;
	Y2 = 32;
	break;
    case FHChannel:
	X = (CLK >> 2) & 0x1F;
	Y1 = (CLK >> 1) & 0x01;
	Y2 = Y1 * 32;
	A ^= ((CLK >> 21) & 0x1F);
	C ^= ((CLK >> 16) & 0x1F);
	D ^= ((CLK >> 7) & 0x1FF);
	F = (((CLK >> 7) & 0x1FFFFF) * 16) % 79;
	break;
    case FHAFH:
	X = (CLK >> 2) & 0x1F;
	Y1 = 0;			// Same channel mechanisim. master channel always.
	Y2 = 0;
	A ^= ((CLK >> 21) & 0x1F);
	C ^= ((CLK >> 16) & 0x1F);
	D ^= ((CLK >> 7) & 0x1FF);
	F = (((CLK >> 7) & 0x1FFFFF) * 16) % 79;
	break;
    }

    /* First addition & XOR operations */
    Z = ((X + A) % 32) ^ B;

    /* Permutation operation */
    Y1 = (Y1 << 4) + (Y1 << 3) + (Y1 << 2) + (Y1 << 1) + Y1;
    P = ((C ^ Y1) << 9) + D;

    for (i = 0; i < 5; i++) {
	Ze[i] = Z & 0x01;
	Z >>= 1;
    }
#define BUTTERFLY(k,i,j) if((P>>k)&0x01) {int t=Ze[i]; Ze[i]=Ze[j]; Ze[j]=t; }
    /*
       if ( (p >> 13) & 0x01) {
       int t = Z & 0x02;
       Z =  (Z & 0xFFFFFFFC) | ((Z >> 1) & 0x02);
       Z =  (Z & 0xFFFFFFFB) | t << 1;
       }
     */
    BUTTERFLY(13, 1, 2);
    BUTTERFLY(12, 0, 3);
    BUTTERFLY(11, 1, 3);
    BUTTERFLY(10, 2, 4);
    BUTTERFLY(9, 0, 3);
    BUTTERFLY(8, 1, 4);
    BUTTERFLY(7, 3, 4);
    BUTTERFLY(6, 0, 2);
    BUTTERFLY(5, 1, 3);
    BUTTERFLY(4, 0, 4);
    BUTTERFLY(3, 3, 4);
    BUTTERFLY(2, 1, 2);
    BUTTERFLY(1, 2, 3);
    BUTTERFLY(0, 0, 1);
#undef BUTTERFLY
    Z = (Ze[4] << 4) + (Ze[3] << 3) + (Ze[2] << 2) + (Ze[1] << 1) + Ze[0];

    /* Second addition operation */
    reg_bank_index = (Z + E + F + Y2) % 79;

    /* select register in Register bank */
    ret = (reg_bank_index <= 39 ? reg_bank_index * 2 :
	   (reg_bank_index - 39) * 2 - 1);

    /* Adapted Frequency Hopping */
    // if (FH_seq == FHAFH && _notUsedCh[ret]) {
    if (FH_seq == FHAFH && _usedChSet.notUsed(ret)) {
	F = (((CLK >> 7) & 0x1FFFFF) * 16) % _usedChSet.numUsedCh();
	reg_bank_index = (Z + E + F + Y2) % _usedChSet.numUsedCh();
	ret = _usedChSet.usedCh(reg_bank_index);
    }

    return ret;
}

// This method returns the sequence.
signed char *Baseband::seq_analysis(bd_addr_t bdaddr, FH_sequence_type fs,
				    int clk, int clkf, int len, int step,
				    signed char *buf)
{
    if (!buf) {
	buf = new signed char[len];
    }

    int i;
    for (i = 0; i < len; i++) {
	buf[i] = FH_kernel(clk + i * step, clkf, fs, bdaddr);
    }

    int prev[79];
    int max[79];
    int min[79];
    int sum[79];
    int cntr[79];

    for (i = 0; i < 79; i++) {
	prev[i] = -1;
	max[i] = 0;
	min[i] = 255;
	sum[i] = 0;
	cntr[i] = 0;
    }

    for (i = 0; i < len; i++) {
	if (prev[buf[i]] < 0) {
	    prev[buf[i]] = i * step;
	} else {
	    int diff = i * step - prev[buf[i]];
	    prev[buf[i]] = i * step;
	    if (diff > max[buf[i]]) {
		max[buf[i]] = diff;
	    }
	    if (diff < min[buf[i]]) {
		min[buf[i]] = diff;
	    }
	    sum[buf[i]] += diff;
	    cntr[buf[i]]++;
	}
    }

    for (i = 0; i < 79; i++) {
	fprintf(stdout, "%d %d %d %0.2f %d\n",
		i, min[i], max[i], double (sum[i]) / cntr[i], cntr[i] + 1);
    }

    return buf;
}

// This method intends to produce the sample data on specs 1.1 pp963-968
// addr[] = { 0x0, 0x2a96ef25, 0x6587cba9 };
// 0x9e8b33 is used for inquiry and inquiry scan
int Baseband::test_fh(bd_addr_t bdaddr)
{
    unsigned int clk, clkf, fh;
    FH_sequence_type fs;
    int i, j;

    printf("\nNEW SET\n\n");

    ////////////////////////////////////////////////////////////////
    //                                                            //
    //              test inquiry scan/page scan                   //
    //                                                            //
    ////////////////////////////////////////////////////////////////

    inquiry_rsps_count = 0;
    fs = FHInqScan;
    clk = 0x00;

    printf("Hop sequence {k} for PAGE SCAN/INQUIRY SCAN SUBSTATE:\n");
    printf("CLKN start:     0x%07x\n", clk);
    printf("UAP / LAP:      0x%08x\n", bdaddr);
    printf
	("#ticks:         0000 | 1000 | 2000 | 3000 | 4000 | 5000 | 6000 | 7000 |\n");
    printf
	("                --------------------------------------------------------");

    for (i = 0; i < 64; i++) {
	fh = FH_kernel(clk + 0x1000 * i, 0, fs, bdaddr);
	if (i % 8 == 0) {
	    printf("\n0x%07x:     ", clk + 0x1000 * i);
	}
	printf("   %.02d |", fh);
    }
    printf("\n");
    printf("\n");

    ////////////////////////////////////////////////////////////////
    //                                                            //
    //                      test inquiry /page                    //
    //                                                            //
    ////////////////////////////////////////////////////////////////

    inquiry_rsps_count = 0;
    fs = FHPage;
    clk = 0x00;
    train_ = Train_A;

    printf("Hop sequence {k} for PAGE STATE/INQUIRY SUBSTATE:\n");
    printf("CLKE start:     0x%07x\n", clk);
    printf("UAP / LAP:      0x%08x\n", bdaddr);
    printf
	("#ticks:         00 01 02 03 | 04 05 06 07 | 08 09 0a 0b | 0c 0d 0e 0f |\n");
    printf
	("                --------------------------------------------------------");

    for (j = 0; j < 4; j++) {
	// printf("clk:%d:%x fs:%d bdaddr:%d:%x\n",
	//        clk, clk, fs, bdaddr, bdaddr);
	for (i = 0; i < 64; i++) {
#if 0
	    if (((clk + i) / (128 * 32)) % 2) {
		train_ = Train_B;
	    } else {
		train_ = Train_A;
	    }
#endif
	    fh = FH_kernel(clk + i, 0, fs, bdaddr);
	    if (i % 16 == 0) {
		printf("\n0x%07x:      ", clk + i);
	    }
	    printf("%.02d ", fh);
	    if (i % 4 == 3) {
		printf("| ");
	    }
	}
	if (j < 3) {
	    printf("\n...");
	}
	clk += 0x1000;

	train_ = (train_ == Train_A ? Train_B : Train_A);
    }
    printf("\n");


    ////////////////////////////////////////////////////////////////
    //                                                            //
    //               test slave page response                     //
    //                                                            //
    ////////////////////////////////////////////////////////////////

    slave_rsps_count = 0;
    fs = FHSlaveResp;
    clkf = 0x10;

    printf("\nHop sequence {k} for SLAVE PAGE RESPONSE SUBSTATE:\n");
    printf("CLKN* =         0x%07x\n", clkf);
    printf("UAP / LAP:      0x%08x\n", bdaddr);
    printf
	("#ticks:         00 | 02 04 | 06 08 | 0a 0c | 0e 10 | 12 14 | 16 18 | 1a 1c | 1e\n");
    printf
	("                ----------------------------------------------------------------");
    // printf("slave_response\n\n");
    // printf("clk:%d:%x fs:%d bdaddr:%d:%x\n", clk, clk, fs, bdaddr, bdaddr);
    for (i = 0; i < 64; i++) {
	clk = clkf + 2 + i * 2;
	fh = FH_kernel(clk, clkf, fs, bdaddr);
	if (i % 2 == 0) {	// right before CLK1 becomes 0
	    slave_rsps_count++;
	}

	if (i % 16 == 0) {
	    printf("\n0x%07x:      ", clk);
	    // printf("\n");
	}
	printf("%.02d ", fh);
	if (i % 2 == 0) {
	    printf("| ");
	}
    }
    printf("\n");
    printf("\n");

    ////////////////////////////////////////////////////////////////
    //                                                            //
    //               test master page response                    //
    //                                                            //
    ////////////////////////////////////////////////////////////////

    fs = FHMasterResp;
    train_ = Train_A;
    clkf = 0x12;
    master_rsps_count = 0;

    printf("\nHop sequence {k} for MASTER PAGE RESPONSE SUBSTATE:\n");
    printf("Offset value:   %d\n", (train_ == Train_A ? 24 : 8));
    printf("CLKE* =         0x%07x\n", clkf);
    printf("UAP / LAP:      0x%08x\n", bdaddr);
    printf
	("#ticks:         00 02 | 04 06 | 08 0a | 0c 0e | 10 12 | 14 16 | 18 1a | 1c 1e |\n");
    printf
	("                ----------------------------------------------------------------");
    // printf("clk:%d:%x fs:%d bdaddr:%d:%x\n", clk, clk, fs, bdaddr, bdaddr);
    for (i = 0; i < 64; i++) {
	clk = clkf + 2 + i * 2;
	if (i % 2 == 0) {	// right before CLK1 becomes 0
	    master_rsps_count++;
	}
	fh = FH_kernel(clk, clkf, fs, bdaddr);
	if (i % 16 == 0) {
	    printf("\n0x%07x:      ", clk);
	    // printf("\n");
	}
	printf("%.02d ", fh);
	if (i % 2 == 1) {
	    printf("| ");
	}
    }
    printf("\n");
    printf("\n");

    ////////////////////////////////////////////////////////////////
    //                                                            //
    //               test basic channel hopping                   //
    //                                                            //
    ////////////////////////////////////////////////////////////////

    clk = 0x10;
    fs = FHChannel;
    printf
	("\nHop sequence {k} for CONNECTION STATE (Basic channel hopping sequence; ie, non-AFH):\n");
    printf("CLK start:       0x%07x\n", clk);
    printf("UAP / LAP:       0x%08x\n", bdaddr);
    printf
	("#ticks:          00 02 | 04 06 | 08 0a | 0c 0e | 10 12 | 14 16 | 18 1a | 1c 1e |\n");
    printf
	("                 ----------------------------------------------------------------");
    // printf("clk:%d:%x fs:%d bdaddr:%d:%x\n", clk, clk, fs, bdaddr, bdaddr);
    for (i = 0; i < 512; i++) {
	fh = FH_kernel(clk + 2 * i, 0, fs, bdaddr);
	if (i % 16 == 0) {
	    printf("\n0x%07x:       ", clk + 2 * i);
	}
	printf("%.02d ", fh);
	if (i % 2 == 1) {
	    printf("| ");
	}
    }
    printf("\n");
    printf("\n");

    ////////////////////////////////////////////////////////////////
    //                                                            //
    //             test adapted channel hopping                   //
    //                                                            //
    ////////////////////////////////////////////////////////////////

    clk = 0x10;
    test_afh(clk, bdaddr, "0x7FFFFFFFFFFFFFFFFFFF",
	     "Hop Sequence {k} for CONNECTION STATE (Adapted channel hopping sequence with all channel used; ie, AFH(79)):");

    test_afh(clk, bdaddr, "0x7FFFFFFFFFFFFFC00000",
	     "Hop Sequence {k} for CONNECTION STATE (Adapted channel hopping sequence with channels 0 to 21 unused):");

    test_afh(clk, bdaddr, "0x55555555555555555555",
	     "Hop Sequence {k} for CONNECTION STATE (Adapted channel hopping sequence with even channels used):");

    test_afh(clk, bdaddr, "0x2AAAAAAAAAAAAAAAAAAA",
	     "Hop Sequence {k} for CONNECTION STATE (Adapted channel hopping sequence with odd channels used):");

    return 0;
}

    // test adapted channel hopping
void Baseband::test_afh(int clk, int bdaddr, const char *map,
			const char *s)
{
    unsigned int fh;
    FH_sequence_type fs;
    int i;
    fs = FHAFH;
    _usedChSet.importMapByHexString(map);

    printf("%s\n", s);
    printf("CLK start:    0x%07x\n", clk);
    printf("ULAP:         0x%08x\n", bdaddr);
    printf("Used Channels:0x");
    _usedChSet.dump();
    printf
	("#ticks:       00 02 | 04 06 | 08 0a | 0c 0e | 10 12 | 14 16 | 18 1a | 1c 1e |\n");
    printf
	("              ---------------------------------------------------------------");
    for (i = 0; i < 512; i++) {
	fh = FH_kernel(clk + 2 * i, 0, fs, bdaddr);
	if (i % 16 == 0) {
	    printf("\n0x%07x     ", clk + 2 * i);
	}
	printf("%.02d ", fh);
	if (i % 2 == 1) {
	    printf("| ");
	}
    }
    printf("\n\n\n");
}


//////////////////////////////////////////////////////////
//                      Handlers                        //
//////////////////////////////////////////////////////////

void BTCLKNHandler::handle(Event * e)
{
    bb_->handle_clkn(e);
}

void BTCLKHandler::handle(Event * e)
{
    bb_->handle_clk(e);
}

void BTRESYNCHandler::handle(Event * e)
{
    bb_->handle_re_synchronize(e);
}

void BTTRXTOTimer::handle(Event * e)
{
    bb_->turn_off_trx((BTTRXoffEvent *) e);
}

void BTTXHandler::handle(Event * e)
{
    bb_->tx_handle((Packet *) e);
}

// Slave send packet in non-connection state
void BTRECVHandler::handle(Event * e)
{
    bb_->recv_handle((Packet *) e);
}

void BTRECVlastBitHandler::handle(Event * e)
{
    bb_->recv_handle_lastbit((Packet *) e);
}

// Slave send packet in non-connection state
void BTslaveSendnonConnHandler::handle(Event * e)
{
    bb_->slave_send_nonconn((Packet *) e);
}
