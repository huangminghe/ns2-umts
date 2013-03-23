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

#ifndef ifmngmt_h
#define ifmngmt_h

#include "mih-user.h"
#include "nd.h"

//Default flow priority
#define DEFAULT_PRIORITY 0

/*
 * List of flows
 */
class FlowEntry;
LIST_HEAD(flowentry, FlowEntry);

class FlowEntry {
 public:
  FlowEntry(Agent *local, Agent *remote, int priority, float minBw,  Node* iface) { //constructor
    local_ = local;	
    remote_ = remote;
    priority_ = priority;
    minBw_ = minBw;
    iface_ = iface;
    //temp variables
    redirect_ = false;
  }
  ~FlowEntry() { ; } //destructor
  
  // Chain element to the list
  inline void insert_entry(struct flowentry *head) {
    LIST_INSERT_HEAD(head, this, link);
  }
  
  // Update priority information
  inline void update_priority(int prio) {
    priority_ = prio;
  }
  // Update interface information
  inline void update_interface(Node* iface) {
    iface_ = iface;
  }

  // Return the router lifetime
  inline Agent* local() { return local_; }
  // Return the prefix information
  inline Agent* remote() {return remote_; }
  // Return the priority information
  inline int& priority() {return priority_; }
  // Return the minimum bandwidth information
  inline float& minBw() {return minBw_; }
  // Return the used interface information
  inline Node* interface() {return iface_; }

  // Return next element in the chained list
  FlowEntry* next_entry(void) const { return link.le_next; }
  // Remove the entry from the list
  inline void remove_entry() { 
    LIST_REMOVE(this, link); 
  }

  //neighbor discovery fields
  Agent *local_;            // local agent
  Agent *remote_;           // remote agent
  int priority_; 	    // flow priority
  float minBw_;             // min bw requirements
  Node *iface_;             // interface used
  //tmp variables.
  bool redirect_;
  Node *tmp_iface;          // to store the temporary interface we may use

 protected:
  LIST_ENTRY(FlowEntry) link; 
};

class NDAgent;
/*
 * List used for storing ND/MAC relation
 */
class Nd_Mac;
LIST_HEAD(nd_mac, Nd_Mac);

class Nd_Mac {
 public:
  Nd_Mac(NDAgent *nd, Mac *mac) { //constructor
    mac_ = mac;	
    nd_ = nd;
  }
  ~Nd_Mac() { ; } //destructor
  
  // Chain element to the list
  inline void insert_entry(struct nd_mac *head) {
    LIST_INSERT_HEAD(head, this, link);
  }
  
  // Return the information
  inline Mac* mac() { return mac_; }
  inline NDAgent* nd() { return nd_; }
  // Return next element in the chained list
  Nd_Mac* next_entry(void) const { return link.le_next; }
  // Remove the entry from the list
  inline void remove_entry() { 
    LIST_REMOVE(this, link); 
  }

  Mac *mac_;
  NDAgent *nd_;

 protected:
  LIST_ENTRY(Nd_Mac) link; 
};

class MIHAgent;

/* 
 * Interface management agent
 */
class IFMNGMTAgent : public MIHUserAgent {
 public:
  IFMNGMTAgent();

  int command(int argc, const char*const* argv);
  virtual void recv(Packet*, Handler*);
  //process events from the neighbor discovery module
  virtual void process_nd_event (int, void*); 

  Packet *getPacket ();

  vector <FlowEntry*> get_flows ();

  struct flowentry fList_head_;    //list of flows

 protected:
  int default_port_;           //default port number for agent
  struct nd_mac nd_mac_;   //the list of ND agents running on the interfaces

  // Flow management
  FlowEntry* head(struct flowentry head) { return head.lh_first; }
  FlowEntry* get_flow (Agent *, Agent *);
  void add_flow (Agent *, Agent *, Node *, int, float);
  void delete_flow (Agent *, Agent *);
  
  NDAgent * get_nd_by_mac (Mac*); //find the ND associated with the mac.
  //void register_ns (); //register this module to the ND agent.
};

#endif
