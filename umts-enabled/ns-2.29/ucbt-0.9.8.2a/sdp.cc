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

/*
 *	sdp.cc
 */

#include "sdp.h"
#include "baseband.h"


int hdr_sdp::offset_;

static class SDPHeaderClass:public PacketHeaderClass {
  public:
    SDPHeaderClass():PacketHeaderClass("PacketHeader/SDP", sizeof(hdr_sdp)) {
	bind_offset(&hdr_sdp::offset_);
    }
} class_sdphdr;

static class SDPclass:public TclClass {
  public:
    SDPclass():TclClass("SDP") {} 

    TclObject *create(int, const char *const *) {
	return (new SDP());
    }
} class_sdp_agent;

void SDPTimer::handle(Event * e)
{}

void SDPInqCallback::handle(Event *)
{
    _sdp->inq_complete();
}

SDP::SDP()
:  Connector(), _timer(this), _inqCallback(this), _ev(), _lastInqT(-9999),
inqTShred_(60), _in_inquiry(0), _conn(0), _conn_num(0), connNumShred(1),
nb_(0), nb_num(0), _transid(0), _q()
{}

void SDP::inq_complete()
{
    if (nb_) {
	lmp_->destroyNeighborList(nb_);
    }
    nb_ = lmp_->getNeighborList(&nb_num);
    _in_inquiry = 0;
    // waitForInq_ = 0;
    // make_connections();

    if (nb_num <= 0) {
	printf("*** Ooops, no neighbors are found. Inquiry failed.\n");
	return;
    }
    Bd_info *wk = nb_;
    for (int i = 0; i < nb_num; i++) {
	connect(wk->bd_addr);
	wk = wk->next;
    }
    sendToAll();
}

SDP::Connection * SDP::lookupConnection(bd_addr_t addr)
{
    Connection *wk = _conn;
    while (wk) {
	if (wk->daddr == addr) {
	    return wk;
	}
	wk = wk->next;
    }
    return NULL;
}

SDP::Connection * SDP::addConnection(L2CAPChannel * ch)
{
    Connection *c = new Connection(this, ch);
    c->next = _conn;
    _conn = c;
    _conn_num++;
    return c;
}

void SDP::removeConnection(SDP::Connection * c)
{
    if (c == _conn) {
	_conn = _conn->next;
	delete c;
	return;
    }

    Connection *par = _conn;
    Connection *wk = _conn->next;
    while (wk) {
	if (wk == c) {
	    par->next = wk->next;
	    delete c;
	    return;
	}
	par = wk;
	wk = wk->next;
    }
}

void SDP::channel_setup_complete(L2CAPChannel * ch)
{
    Connection *c = lookupConnection(ch->_bd_addr);
    if (!c) {
	c = addConnection(ch);
    }
    c->_ready = 1;
    c->send();
}

SDP::Connection * SDP::connect(bd_addr_t addr)
{
    Connection *c;
    if ((c = lookupConnection(addr))) {
	return c;
    }

    L2CAPChannel *ch = l2cap_->L2CA_ConnectReq(addr, PSM_SDP);

    c = addConnection(ch);
    if (ch->_ready) {
	c->_ready = 1;
    }

    return c;
}

void SDP::setup(bd_addr_t ad, LMP * l, L2CAP * l2, BTNode * node)
{
    bd_addr_ = ad;
    l2cap_ = l2;
    lmp_ = l;
    btnode_ = node;
}

int SDP::command(int argc, const char *const *argv)
{
    if (argc == 2) {
	if (!strcmp(argv[1], "test")) {
	    uchar req[128];
	    int len = 1;
	    SDP_serviceSearchReq(req, len);
	    return (TCL_OK);
	}
    }
    return Connector::command(argc, argv);
}

void SDP::inquiry()
{
    printf("%d SDP start inquiry().\n", bd_addr_);
    lmp_->HCI_Inquiry(lmp_->giac_, 4, 7);	// 4x1.28s
    _in_inquiry = 1;
    lmp_->addInqCallback(&_inqCallback);
}

void SDP::inqAndSend(Packet * p)
{
    Scheduler & s = Scheduler::instance();
    double now = s.clock();

    _q.enque(p);
    if (_lastInqT + inqTShred_ < now && !_in_inquiry) {
	inquiry();		// start inquiry process
	return;
    }

    int i;
    if (_conn_num < connNumShred) {
	Bd_info *wk = nb_;
	for (i = 0; i < nb_num - 1; i++) {
	    connect(wk->bd_addr);
	}
    }
    sendToAll();
}

void SDP::sendToAll()
{
    assert(_conn);

    Connection *c;
    Packet *p;

    while ((p = _q.deque())) {
	c = _conn;
	for (int i = 0; i < _conn_num - 1; i++) {	// bcast
	    c->cid->enque(p->copy());
	    c = c->next;
	}
	c->cid->enque(p);
    }
}

Packet *SDP::gen_sdp_pkt(uchar * req, int len)
{
    Packet *p = Packet::alloc(len);
    hdr_sdp *sh = HDR_SDP(p);
    hdr_cmn *ch = HDR_CMN(p);
    ch->size() = len + 5;
    // sh->transid = _transid++;
    sh->paramLen = len;
    memcpy(p->accessdata(), req, len);

    return p;
}

void SDP::recv(Packet * p, L2CAPChannel * ch)
{
    hdr_sdp *sh = HDR_SDP(p);

    char buf[1024];
    int len = 1024;
    printf("%d SDP::recv() from %d - %s\n", bd_addr_, ch->remote(),
	   sh->dump(buf, len));

    switch (sh->pduid) {
    case SDP_ErrorResp:
	break;

    case SDP_ServSrchReq:
	printf("Got SDP_ServSrchReq.\n");
	SDP_serviceSearchRsp(p, ch);
	break;

    case SDP_ServSrchRes:
	printf("Got SDP_ServSrchResp.\n");
	break;

    case SDP_ServAttrReq:
	printf("Got SDP_ServAttrReq.\n");
	SDP_serviceAttributeRsp(p, ch);
	break;

    case SDP_ServAttrRes:
	printf("Got SDP_ServAttrResp.\n");
	break;

    case SDP_ServSrchAttrReq:
	printf("Got SDP_ServSrchAttrReq.\n");
	SDP_serviceSearchAttributeRsp(p, ch);
	break;

    case SDP_ServSrchAttrRes:
	printf("Got SDP_ServSrchAttrResp.\n");
	break;

    default:
	fprintf(stderr, "%d SDP recv invalid PDUID %d\n",
		lmp_->bb_->bd_addr_, sh->pduid);
	abort();
    }

    Packet::free(p);
}

void SDP::SDP_serviceSearchReq(uchar * req, int len)
{
    Packet *p = gen_sdp_pkt(req, len);
    hdr_sdp *sh = HDR_SDP(p);
    sh->pduid = SDP_ServSrchReq;
    sh->transid = _transid++;
    inqAndSend(p);
}

void SDP::SDP_serviceSearchRsp(Packet * p, L2CAPChannel * ch)
{
    hdr_sdp *sh = HDR_SDP(p);

    uchar rep[128];
    int len = 1;

    Packet *resp = gen_sdp_pkt(rep, len);
    hdr_sdp *sh_resp = HDR_SDP(resp);
    sh_resp->pduid = SDP_ServSrchRes;
    sh_resp->transid = sh->transid;

    ch->enque(resp);
}

void SDP::SDP_serviceAttributeReq(uchar * req, int len)
{
    Packet *p = gen_sdp_pkt(req, len);
    hdr_sdp *sh = HDR_SDP(p);
    sh->pduid = SDP_ServAttrReq;
    sh->transid = _transid++;
    inqAndSend(p);
}

void SDP::SDP_serviceAttributeRsp(Packet * p, L2CAPChannel * ch)
{
    hdr_sdp *sh = HDR_SDP(p);

    uchar rep[128];
    int len = 1;

    Packet *resp = gen_sdp_pkt(rep, len);
    hdr_sdp *sh_resp = HDR_SDP(resp);
    sh_resp->pduid = SDP_ServAttrRes;
    sh_resp->transid = sh->transid;

    ch->enque(resp);
}

void SDP::SDP_serviceSearchAttributeReq(uchar * req, int len)
{
    Packet *p = gen_sdp_pkt(req, len);
    hdr_sdp *sh = HDR_SDP(p);
    sh->pduid = SDP_ServSrchAttrReq;
    sh->transid = _transid++;
    inqAndSend(p);
}

void SDP::SDP_serviceSearchAttributeRsp(Packet * p, L2CAPChannel * ch)
{
    hdr_sdp *sh = HDR_SDP(p);

    uchar rep[128];
    int len = 1;

    Packet *resp = gen_sdp_pkt(rep, len);
    hdr_sdp *sh_resp = HDR_SDP(resp);
    sh_resp->pduid = SDP_ServSrchAttrRes;
    sh_resp->transid = sh->transid;

    ch->enque(resp);
}
