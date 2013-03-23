/*
 * include file for Interface management module
 *
 * This software was developed at the National Institute of Standards and
 * Technology by employees of the Federal Government in the course of
 * their official duties. Pursuant to title 17 Section 105 of the United
 * States Code this software is not subject to copyright protection and
 * is in the public domain.
 * NIST assumes no responsibility whatsoever for its use by other parties,
 * and makes no guarantees, expressed or implied, about its quality,
 * reliability, or any other characteristic.
 * <BR>
 * We would appreciate acknowledgement if the software is used.
 * <BR>
 * NIST ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION AND
 * DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING
 * FROM THE USE OF THIS SOFTWARE.
 * </PRE></P>
 * @author  rouil
 * @version 1.0
 * <P><PRE>
 * VERSION CONTROL<BR>
 * -------------------------------------------------------------------------<BR>
 * Name  - YYYY/MM/DD - VERSION - ACTION<BR>
 * rouil - 2005/05/01 - 1.0     - Source created<BR>
 * <BR>
 * <BR>
 * </PRE><P>
 */ 

#ifndef mip6_h
#define mip6_h

#include "if-mngmt.h"
#include <vector>

/* 
 * Packet structure for redirect messages
 */
#define HDR_RTRED(p)    ((struct hdr_rtred*)(p)->access(hdr_rtred::offset_))

struct hdr_rtred {

  vector <Agent*> agent_;  //agents to redirect
  int dest_;      //new interface to use
  char ack_;      //define if it is an ack message

  static int offset_;
  inline static int& offset() { return offset_; }
  inline static hdr_rtred* access(Packet* p) {
    return (hdr_rtred*) p->access(offset_);
  }
  inline vector<Agent*>& agent() { return agent_; }
  inline int& destination() { return dest_; }
  inline char& ack() { return ack_; }
};
#define PT_RRED_SIZE		IPv6_HEADER_SIZE + 8

/* 
 * Packet structure for flow request
 */
#define HDR_FREQ(p)    ((struct hdr_freq*)(p)->access(hdr_freq::offset_))

struct hdr_freq {
  vector <FlowEntry*> flow_info;
  int seqNumber_;

  static int offset_;
  inline static int& offset() { return offset_; }
  inline static hdr_freq* access(Packet* p) {
    return (hdr_freq*) p->access(offset_);
  }
};
#define PT_FREQ_SIZE		IPv6_HEADER_SIZE + 8

/* 
 * Packet structure for flow request
 */
#define HDR_FRES(p)    ((struct hdr_fres*)(p)->access(hdr_fres::offset_))

struct hdr_fres {
  vector <FlowEntry*> flow_info;
  int seqNumber_;

  static int offset_;
  inline static int& offset() { return offset_; }
  inline static hdr_fres* access(Packet* p) {
    return (hdr_fres*) p->access(offset_);
  }
};
#define PT_FRES_SIZE		IPv6_HEADER_SIZE + 8


/*
 * Packet structure for communicating between Handover modules
 */
#define HDR_HAND(p) ((struct hdr_hand*)(p)->access(hdr_hand::offset_))

struct prefered_network {
  int type;
  int mac;
  int poa;
  int channel;
};

struct hdr_hand {
  int subtype;  //for different content

  //the following are used for communication handover preferences
  //between PA and MN
  vector <prefered_network*> *networks;

  static int offset_;
  inline static int& offset() { return offset_; }
  inline static hdr_hand* access(Packet* p) {
    return (hdr_hand*) p->access(offset_);
  }
};
//define size containing only 1 entry in the vector. If more, redefine
#define PT_HAND_SIZE		IPv6_HEADER_SIZE + 40

/* Data structure to keep track of who is connected
 */
struct list_node {
  Node *node; //the node connected
  int port; //the port of the application
  float minBw;    //minimum bandwidth used by the application
};

/* 
 * NC flow resquest timer
 * Allow a flow request to be resent if lost
 */
class FlowRequestTimer;

/* 
 * MIPv6 agent
 */
class MIPV6Agent : public IFMNGMTAgent {
 public:
  MIPV6Agent();

  int command(int argc, const char*const* argv);
  void recv(Packet*, Handler*);
  //process events from the neighbor discovery module
  void process_nd_event (int, void*); 
  void process_get_status_response (mih_get_status_s*);
  void send_update_msg (Agent *, Node *);
  void send_update_msg (vector<Agent *>, Node *);
  void send_flow_request (vector<FlowEntry*>, Node *, int);
  //void process_client_going_down (int); //to remove
  void send_rs (Mac *); //send an RA message for the given interface
  //flow request timer
  FlowRequestTimer *flowRequestTimer_;
  double flowRequestTimeout_;
  int seqNumberFlowRequest_;
 protected:
  
  virtual void process_new_prefix (new_prefix*);
  virtual void process_exp_prefix (exp_prefix*);


  // Message processing
  void recv_redirect(Packet*);
  void recv_redirect_ack(Packet*);
  void recv_flow_request(Packet*);
  void recv_flow_response(Packet*);

 private:

  vector <list_node*> list_nodes;

  float get_used_bw();
  int nbAccepted_;
  int nbRefused_;

};
/* 
 * NC flow request timer
 * Allow a flow request to be resent if lost
 */
class FlowRequestTimer : public TimerHandler {
 public:
  FlowRequestTimer(MIPV6Agent *m) : TimerHandler() { m_ = m;}
  inline void expire(Event *){m_->send_flow_request (flows_, iface_, destination_);}
  inline void attach_event(vector<FlowEntry*> flows, Node *iface, int destination){
    flows_ = flows;
    iface_ = iface;
    destination_ = destination;
  }
  protected:
  MIPV6Agent *m_;
  vector<FlowEntry*> flows_;
  Node *iface_;
  int destination_;
};

#endif
