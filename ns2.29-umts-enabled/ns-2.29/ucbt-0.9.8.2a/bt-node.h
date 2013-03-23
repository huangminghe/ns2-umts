/*
 * Copyright (c) 2004, University of Cincinnati, Ohio.
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

#ifndef __ns_bt_node_h__
#define __ns_bt_node_h__

#include "mobilenode.h"

#include "baseband.h"
#include "lmp.h"
#include "l2cap.h"
#include "sco-agent.h"
#include "bnep.h"
#include "sdp.h"
#include "rt-bt.h"
#include "bt-stat.h"

class BTNodeTimer:public Handler {
  public:
    BTNodeTimer(class BTNode *nd): node_(nd) {}
    void handle(Event *);

  private:
    class BTNode *node_;
};

class BTNode:public Node {
    friend class LMP;
  public:

    Baseband *bb_;
    L2CAP *l2cap_;
    BNEP *bnep_;
    ScoAgent *_agent;
    SDP *sdp_;
    MobileNode *wifi_;
    class ScatFormator * scatFormator_;

    static int _numFileOpened;
    static const char *_tracefilename[MAXTRACEFILES];
    static FILE *_tracefile[MAXTRACEFILES];
    static char filenameBuff[4096];
    // static char* filenameBuffAvl;
    static int filenameBuffInd;

    int setTrace(const char *cmdname, const char *arg, int);
    int setTraceStream(FILE ** tfileref, const char *fname,
		       const char *cmd, int);

    void on();

  public:
    LMP * lmp_;
    BTRoutingIF *ragent_;
    BtStat *stat_;
    BTNodeTimer _timer;
    Event on_ev_;
    int _on;

    int randomizeSlotOffset_;
    int enable_clkdrfit_in_rp_;
    FILE *logfile_;

    Trace* log_target_;

    BTNode();
    int command(int, const char *const *);
    void setup(uint32_t addr, Baseband * bb, LMP * lmp,
		       L2CAP * l2cap, BNEP *, SDP *);
    void sco_connect(BTNode *, ScoAgent *, ScoAgent *);
    void addScoAgent(ScoAgent * a);
    inline MobileNode *mobilenode() { return wifi_; } 
    void set_mobilenode(MobileNode * m);
    void printStat();
    void printAllStat();
    BTNode *lookupNode(bd_addr_t n);
    int masterLink(bd_addr_t rmt);
    void linkDetached(bd_addr_t addr, uchar reason);
    void flushPkt(bd_addr_t addr);
    //void join(BTNode *slave, hdr_bt::packet_type pt, hdr_bt::packet_type rpt);
    void bnep_join(BTNode * slave, hdr_bt::packet_type pt,
		  hdr_bt::packet_type rpt);
    void energyResetAllNodes();
    void setall_scanwhenon(int v);
    void force_on();
    void forceSuspendCurPico();
    L2CAPChannel *setupL2CAPChannel(ConnectionHandle *connh, bd_addr_t rmt, 
			L2CAPChannel *rcid);

    void dump_str(const char *s) {
	unsigned int len = strlen(s);
	for (unsigned int i = 0; i < len; i++) {
	    putchar(s[i]);
	    printf("[%d] ", s[i]);
	}
	printf("(len:%u)\n", len);
    }

};

#endif				// __ns_bt_node_h__
