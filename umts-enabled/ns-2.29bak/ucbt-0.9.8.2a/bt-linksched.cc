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

#include "bt-linksched.h"
#include "baseband.h"
#include "lmp.h"
#include "lmp-link.h"
#include "lmp-piconet.h"
#include "random.h"


//////////////////////////////////////////////////////////
//                      BTRR                           //
//////////////////////////////////////////////////////////
BTLinkScheduler::BTLinkScheduler(Baseband * bb)
:bb_(bb), inited(0) 
{
    // bb_->lmp_->l2capChPolicy_ = LMP::l2RR;
} 


//////////////////////////////////////////////////////////
//                      BTRR                           //
//////////////////////////////////////////////////////////
// Round Robin
// We basically ignore computed sched_word for non-QoS ACL guys.
void BTRR::init()
{
    if (bb_->lmp_->curPico && bb_->lmp_->curPico->activeLink) {
	lastSchedSlot = bb_->lmp_->curPico->activeLink->txBuffer->slot();
	lastSchedSlot_highclass = lastSchedSlot;
	drrPass = 1;
	inited = 1;
    }
}

int BTRR::sched(int clk, BTSchedWord * sched, int curSlot)
{
    int i;
    LMPLink *wk, *next;

    if (!inited) {
	return Baseband::RecvSlot;
    }

    if (curSlot == Baseband::ReserveSlot) {	// not handled at this moment
	return Baseband::RecvSlot;
    } else if (curSlot != Baseband::DynamicSlot
	       && bb_->_txBuffer[curSlot]->prioClass >= TxBuffer::High) {
	return curSlot;
    }

    if (!bb_->_txBuffer[lastSchedSlot]) {
	init();
    }
    if (bb_->_txBuffer[lastSchedSlot]->link()->suspended) {
	wk = bb_->lmp_->curPico->activeLink;
    } else {
	wk = bb_->_txBuffer[lastSchedSlot]->link()->next;
    }
    next = wk;

    // first pass, try to get a pkt from an non-QoS guy
    for (i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {
	// FIXME: This should change to credit based.
	if (wk->txBuffer->prioClass >= TxBuffer::High) {
	    wk = wk->next;
	    continue;
	}

	if (wk->txBuffer->hasDataPkt()) {
	    lastSchedSlot = wk->txBuffer->slot();
	    wk->txBuffer->lastPollClk = bb_->clk_;
	    return lastSchedSlot;

	    // to avoid starvation.  Force to poll every T_poll_max.
	} else if (((bb_->clk_ - wk->txBuffer->lastPollClk) << 1) >=
		   wk->txBuffer->T_poll_max) {
	    // wk->txBuffer->T_poll) {
	    lastSchedSlot = wk->txBuffer->slot();
	    wk->txBuffer->lastPollClk = bb_->clk_;
	    return lastSchedSlot;
	}
	wk = wk->next;
    }

    // second pass.  Check if a QoS guy wants to send more.
    if (!bb_->_txBuffer[lastSchedSlot_highclass]) {
	lastSchedSlot_highclass = 
		bb_->lmp_->curPico->activeLink->txBuffer->slot();
    }
    if (bb_->_txBuffer[lastSchedSlot_highclass]->link()->suspended) {
	wk = bb_->lmp_->curPico->activeLink;
    } else {
	wk = bb_->_txBuffer[lastSchedSlot_highclass]->link()->next;
    }
    for (i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {
	if (wk->txBuffer->prioClass == TxBuffer::High &&
	    wk->txBuffer->hasDataPkt()) {
	    lastSchedSlot_highclass = wk->txBuffer->slot();
	    wk->txBuffer->lastPollClk_highclass = bb_->clk_;
	    return lastSchedSlot_highclass;
	}
	wk = wk->next;
    }

    lastSchedSlot = next->txBuffer->slot();
    next->txBuffer->lastPollClk = bb_->clk_;
    return lastSchedSlot;

    // third pass, used a dynamic T_poll 
    // The reason: the master should not poll if nobody has anything
    // to send, as it causes interferences to other piconets.
    // However we don't want to miss a slave if it has data at this
    // moment.
    wk = next;
    for (i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {
	if (((bb_->clk_ - wk->txBuffer->lastPollClk) << 1) >=
	    wk->txBuffer->T_poll) {
	    lastSchedSlot = wk->txBuffer->slot();
	    wk->txBuffer->lastPollClk = bb_->clk_;
	    return lastSchedSlot;
	}
	wk = wk->next;
    }

    return Baseband::RecvSlot;	// Nothing to send
}


//////////////////////////////////////////////////////////
//                      BTDRR                           //
//////////////////////////////////////////////////////////
// Deficit Round Robin
// We basically ignore computed sched_word for non-QoS ACL guys.
void BTDRR::init()
{
    if (bb_->lmp_->curPico && bb_->lmp_->curPico->activeLink) {
	lastSchedSlot = bb_->lmp_->curPico->activeLink->txBuffer->slot();
	lastSchedSlot_highclass = lastSchedSlot;
	drrPass = 1;
	inited = 1;

	LMPLink *wk = bb_->lmp_->curPico->activeLink;
	for (int i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {
	    wk->txBuffer->deficit_ = 0;
	}
    }
}

int BTDRR::sched(int clk, BTSchedWord * sched, int curSlot)
{
    int i;
    LMPLink *wk, *next;

    if (!inited) {
	return Baseband::RecvSlot;
    }

    if (curSlot == Baseband::ReserveSlot) {	// not handled at this moment
	return Baseband::RecvSlot;
    } else if (curSlot != Baseband::DynamicSlot
	       && bb_->_txBuffer[curSlot]->prioClass >= TxBuffer::High) {
	return curSlot;
    }

    if (!bb_->_txBuffer[lastSchedSlot]) {
	lastSchedSlot = bb_->lmp_->curPico->activeLink->txBuffer->slot();
	lastSchedSlot_highclass = lastSchedSlot;
    }
    if (bb_->_txBuffer[lastSchedSlot]->link()->suspended) {
	wk = bb_->lmp_->curPico->activeLink;
    } else {
	wk = bb_->_txBuffer[lastSchedSlot]->link()->next;
    }
    next = wk;

    // first pass, try to get a pkt from an non-QoS guy
    for (int x = 0; x < 5; x++) {	// maximun slot usage is 10 slots
	for (i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {
	    // FIXME: This should change to credit based.
	    if (wk->txBuffer->prioClass >= TxBuffer::High) {
		wk = wk->next;
		continue;
	    }

	    if (wk->txBuffer->deficit_ >= 0 && wk->txBuffer->hasDataPkt()) {
		lastSchedSlot = wk->txBuffer->slot();
		wk->txBuffer->lastPollClk = bb_->clk_;
		return lastSchedSlot;

		// to avoid starvation.  Force to poll every T_poll_max.
	    } else if (((bb_->clk_ - wk->txBuffer->lastPollClk) << 1) >=
		       wk->txBuffer->T_poll_max) {
		// wk->txBuffer->T_poll) {
		lastSchedSlot = wk->txBuffer->slot();
		wk->txBuffer->lastPollClk = bb_->clk_;
		return lastSchedSlot;
	    }
	    wk = wk->next;
	}
	int negative = 0;
	for (i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {
	    if (wk->txBuffer->deficit_ < 0) {
		negative = 1;
	    }
	    wk->txBuffer->deficit_ += 2;
	    if (wk->txBuffer->deficit_ > 0) {
		wk->txBuffer->deficit_ = 0;
	    }
	    wk = wk->next;
	}
	if (!negative) {
	    break;
	}
    }

    // second pass.  Check if a QoS guy wants to send more.
    if (!bb_->_txBuffer[lastSchedSlot_highclass]) {
	lastSchedSlot_highclass = 
		bb_->lmp_->curPico->activeLink->txBuffer->slot();
    }
    if (bb_->_txBuffer[lastSchedSlot_highclass]->link()->suspended) {
	wk = bb_->lmp_->curPico->activeLink;
    } else {
	wk = bb_->_txBuffer[lastSchedSlot_highclass]->link()->next;
    }
    for (i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {
	if (wk->txBuffer->prioClass == TxBuffer::High &&
	    wk->txBuffer->hasDataPkt()) {
	    lastSchedSlot_highclass = wk->txBuffer->slot();
	    wk->txBuffer->lastPollClk_highclass = bb_->clk_;
	    return lastSchedSlot_highclass;
	}
	wk = wk->next;
    }

    lastSchedSlot = next->txBuffer->slot();
    next->txBuffer->lastPollClk = bb_->clk_;
    return lastSchedSlot;

    // third pass, used a dynamic T_poll 
    // The reason: the master should not poll if nobody has anything
    // to send, as it causes interferences to other piconets.
    // However we don't want to miss a slave if it has data at this
    // moment.
    wk = next;
    for (i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {
	if (((bb_->clk_ - wk->txBuffer->lastPollClk) << 1) >=
	    wk->txBuffer->T_poll) {
	    lastSchedSlot = wk->txBuffer->slot();
	    wk->txBuffer->lastPollClk = bb_->clk_;
	    return lastSchedSlot;
	}
	wk = wk->next;
    }

    return Baseband::RecvSlot;	// Nothing to send
}

//////////////////////////////////////////////////////////
//                      BTERR                           //
//////////////////////////////////////////////////////////
// Exhaustive Round Robin
void BTERR::init()
{
    if (bb_->lmp_->curPico && bb_->lmp_->curPico->activeLink) {
	lastSchedSlot = bb_->lmp_->curPico->activeLink->txBuffer->slot();
	drrPass = 1;
    }
}

int BTERR::sched(int clk, BTSchedWord * sched, int curSlot)
{
    int i;
    LMPLink *wk;

    // QoS req
    if (bb_->_txBuffer[curSlot] &&
	bb_->_txBuffer[curSlot]->prioClass >= TxBuffer::High) {
	return curSlot;
    }

    if (!bb_->_txBuffer[lastSchedSlot]) {
	init();
    }
    if (bb_->_txBuffer[lastSchedSlot]->link()->suspended) {
	wk = bb_->lmp_->curPico->activeLink;
    } else {
	wk = bb_->_txBuffer[lastSchedSlot]->link();
    }
    // Exhaustive ??
    if (wk->txBuffer->hasDataPkt() || wk->txBuffer->nullCntr() == 0) {
	wk->txBuffer->lastPollClk = bb_->clk_;
	lastSchedSlot = wk->txBuffer->slot();
	return lastSchedSlot;
    }

    wk = wk->next;
    for (i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {
	if (wk->txBuffer->hasDataPkt()) {
	    lastSchedSlot = wk->txBuffer->slot();
	    wk->txBuffer->lastPollClk = bb_->clk_;
	    return lastSchedSlot;
	} else if (((bb_->clk_ - wk->txBuffer->lastPollClk) << 1) >=
		   wk->txBuffer->T_poll_max) {
	    // wk->txBuffer->T_poll) {
	    lastSchedSlot = wk->txBuffer->slot();
	    wk->txBuffer->lastPollClk = bb_->clk_;
	    return lastSchedSlot;
	}
	wk = wk->next;
    }

    return Baseband::RecvSlot;	// Nothing to send
}

//////////////////////////////////////////////////////////
//                      BTPRR                           //
//////////////////////////////////////////////////////////
// Priority Round Robin
void BTPRR::init()
{
    if (bb_->lmp_->curPico && bb_->lmp_->curPico->activeLink) {
	lastSchedSlot = bb_->lmp_->curPico->activeLink->txBuffer->slot();
	drrPass = 1;
	inited = 1;
    }
}

int BTPRR::sched(int clk, BTSchedWord * sched, int curSlot)
{
    if (!inited) {
	return Baseband::RecvSlot;
    }

    int i;
    int curPrioLevel = PRR_MIN_PRIO_MINUS_ONE, retSlot;
    LMPLink *wk;

    // QoS req
    if (bb_->_txBuffer[curSlot]
	&& bb_->_txBuffer[curSlot]->prioClass >= TxBuffer::High) {
	return curSlot;
    }

    if (!bb_->_txBuffer[lastSchedSlot]) {
	init();
    }
    if (bb_->_txBuffer[lastSchedSlot]->link()->suspended) {
	wk = bb_->lmp_->curPico->activeLink;
    } else {
	wk = bb_->_txBuffer[lastSchedSlot]->link()->next;
    }

    // Find the guy who has data for has not been polled for a long period.
    for (i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {

	// Force poll every T_poll_max, to avoid starvation.
	if (((bb_->clk_ - wk->txBuffer->lastPollClk) << 1) >=
	    wk->txBuffer->T_poll_max) {
	    lastSchedSlot = wk->txBuffer->slot();
	    wk->txBuffer->lastPollClk = bb_->clk_;
	    return lastSchedSlot;
	}
	// Check for priority.
	if (wk->txBuffer->hasDataPkt()
	    && wk->txBuffer->prio > curPrioLevel) {
	    retSlot = wk->txBuffer->slot();
	    curPrioLevel = wk->txBuffer->prio;
	}
	wk = wk->next;
    }

    if (curPrioLevel > PRR_MIN_PRIO_MINUS_ONE) {
	lastSchedSlot = retSlot;
	bb_->_txBuffer[lastSchedSlot]->lastPollClk = bb_->clk_;
	return lastSchedSlot;
    }
    // Second pass, used a dynamic T_poll 
    // The reason: the master should not poll if nobody has anything
    // to send, as it causes interferences to other piconets.
    // However we don't want to miss a slave if it has data at this
    // moment.
    for (i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {
	if (((bb_->clk_ - wk->txBuffer->lastPollClk) << 1) >=
	    wk->txBuffer->T_poll) {
	    lastSchedSlot = wk->txBuffer->slot();
	    wk->txBuffer->lastPollClk = bb_->clk_;
	    return lastSchedSlot;
	}
	wk = wk->next;
    }

    return Baseband::RecvSlot;	// Nothing to send
}

//////////////////////////////////////////////////////////
//                      BTFCFS                           //
//////////////////////////////////////////////////////////
// First Come First Serve
BTFCFS::BTFCFS(Baseband * bb)
:BTLinkScheduler(bb) 
{
    // bb_->lmp_->l2capChPolicy_ = LMP::l2FCFS;
}

void BTFCFS::init()
{
    if (bb_->lmp_->curPico && bb_->lmp_->curPico->activeLink) {
	lastSchedSlot = bb_->lmp_->curPico->activeLink->txBuffer->slot();
	drrPass = 1;
	inited = 1;
    }
}

int BTFCFS::sched(int clk, BTSchedWord * sched, int curSlot)
{
    if (!inited) {
	return Baseband::RecvSlot;
    }

    // QoS req
    if (bb_->_txBuffer[curSlot]
	&& bb_->_txBuffer[curSlot]->prioClass >= TxBuffer::High) {
	return curSlot;
    }

    LMPLink *wk = bb_->lmp_->curPico->activeLink;
    int i;
    double ts = 1E10;
    int fcSlot = -1;

    for (i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {
	if (!wk->txBuffer->_suspended && wk->txBuffer->_current
		&& HDR_BT(wk->txBuffer->_current)->ts_ < ts) {
	    fcSlot = wk->txBuffer->slot();
	    ts = HDR_BT(wk->txBuffer->_current)->ts_;
	}
	wk = wk->next;
    }

    return (fcSlot > 0 ? lastSchedSlot = fcSlot : Baseband::RecvSlot);
}

