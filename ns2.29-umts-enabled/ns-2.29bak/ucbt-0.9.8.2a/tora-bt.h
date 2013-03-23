#ifndef __tora_bt_h__
#define __tora_bt_h__

#include "tora/tora.h"
#include "rt-bt.h"

class TORA_BT:public toraAgent, public BTRoutingIF {
  public:
    TORA_BT(nsaddr_t id):toraAgent(id), BTRoutingIF() {} 

    int command(int argc, const char *const *argv);
    virtual void recvReply(Packet *p);

    virtual nsaddr_t nextHop(nsaddr_t dst);
    virtual void sendInBuffer(nsaddr_t dst);
    // virtual void start();
    virtual void addRtEntry(nsaddr_t dst, nsaddr_t nexthop, int flag);
    virtual void delRtEntry(nsaddr_t nexthop);
};

#endif				// __tora_bt_h__
