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

#ifndef __ns_bt_sdp_h__
#define __ns_bt_sdp_h__

// Service Discovery Protocol
//

#include "bi-connector.h"
#include "l2cap.h"
#include "agent.h"
#include "baseband.h"
#include "lmp.h"

#define SDP_ErrorResp		0x01
#define SDP_ServSrchReq		0x02
#define SDP_ServSrchRes		0x03
#define SDP_ServAttrReq		0x04
#define SDP_ServAttrRes		0x05
#define SDP_ServSrchAttrReq	0x06
#define SDP_ServSrchAttrRes	0x07

struct hdr_sdp {
    uchar pduid;		// defined above
    uint16_t transid;
    uint16_t paramLen;		// size() == paramLen + 1 + 2 + 2;
    double timeout;
    // uchar param[SDP_PARAM_SIZE] // let make this into *data

    static int offset_;
    inline static int &offset() {
	return offset_;
    } 
    inline static hdr_sdp *access(Packet * p) {
	return (hdr_sdp *) p->access(offset_);
    }

    char *dump(char *buf, int len) {
	snprintf(buf, len, "pduid:%d transid:%d paramLen:%d",
		 pduid, transid, paramLen);
	buf[len - 1] = '\0';
	return buf;
    }
};

class SDP;

class SDPTimer:public Handler {
  public:
    SDPTimer(SDP * s):_sdp(s) {} 
    void handle(Event *);
  private:
    SDP * _sdp;
};

class SDPInqCallback:public Handler {
  public:
    SDPInqCallback(SDP * s):_sdp(s) {} 
    void handle(Event *);
  private:
    SDP * _sdp;
};

class SDP:public Connector {
  public:
    class Connection {
      public:
	class SDP * sdp_;
	Connection *next;
	L2CAPChannel *cid;
	int daddr;
	int _ready;
	PacketQueue _q;

	Connection(SDP * s, L2CAPChannel * c = 0)
	:sdp_(s), next(0), cid(c), daddr(-1), _ready(0), _q() {} 

	void send() {
	    Packet *p;
	    // FIXME: check timeout of pkt first
	    while ((p = _q.deque())) {
		cid->enque(p);
	    }
	}
    };

    SDP();
    void setup(bd_addr_t ad, LMP * l, L2CAP * l2, BTNode * node);
    void sendToAll();
    void recv(Packet *, L2CAPChannel * ch);
    int command(int argc, const char *const *argv);

    Connection *connect(bd_addr_t addr);
    void channel_setup_complete(L2CAPChannel * ch);
    Connection *addConnection(L2CAPChannel * ch);
    void removeConnection(SDP::Connection * c);
    Connection *lookupConnection(bd_addr_t addr);
    Connection *lookupConnection(L2CAPChannel * ch);

    void inq_complete();
    Packet *gen_sdp_pkt(uchar *, int);
    void inquiry();
    void inqAndSend(Packet *);

    void SDP_serviceSearchReq(uchar *, int);
    void SDP_serviceSearchRsp(Packet *, L2CAPChannel *);
    void SDP_serviceAttributeReq(uchar *, int);
    void SDP_serviceAttributeRsp(Packet *, L2CAPChannel *);
    void SDP_serviceSearchAttributeReq(uchar *, int);
    void SDP_serviceSearchAttributeRsp(Packet *, L2CAPChannel *);

    bd_addr_t bd_addr_;
    L2CAP *l2cap_;
    LMP *lmp_;
    BTNode *btnode_;

    int packetType_;
    SDPTimer _timer;
    SDPInqCallback _inqCallback;
    Event _ev;
    double _lastInqT;
    double inqTShred_;
    int _in_inquiry;

    Connection *_conn;
    int _conn_num;
    int connNumShred;
    Bd_info *nb_;
    int nb_num;

    int _transid;
    PacketQueue _q;
};

#endif				// __ns_bt_sdp_h__
