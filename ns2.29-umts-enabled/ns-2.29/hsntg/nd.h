/*
 * Include file for neighbor discovery protocol
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

#ifndef nd_h
#define nd_h

#include "agent.h"
#include "tclcl.h"
#include "packet.h"
#include "address.h"
#include "ip.h"
#include "node.h"
#include "random.h"

#define IPv6_ADDR_SIZE 		16
#define IPv6_HEADER_SIZE 	2 * IPv6_ADDR_SIZE + 8 
#define DEST_HDR_PREFIX_SIZE	2
#ifdef IP_HEADER_SIZE
#undef IP_HEADER_SIZE
#endif
#define IP_HEADER_SIZE IPv6_HEADER_SIZE
#define UDP_HDR_SIZE		8

//defines the type of timers. Timer ID >= 0 are reserved for prefix
#define IPv6_TIMER_RA -1
#define IPv6_TIMER_RS -2

/* 
 * Packet structure for router advertisements
 */
#define HDR_RTADS(p)    ((struct hdr_rtads*)(p)->access(hdr_rtads::offset_))

struct hdr_rtads {
  uint8_t cur_hop_limit_;   //current hop limit: not used
  //uint16_t router_lftm_;  //router lifetime in seconds
  double router_lftm_;     //router lifetime in seconds
  uint32_t reachable_time_;//time is ms that a node assumes a neighbor is 
                           //reachable after receiving the RA
  uint32_t retrans_timer_; //Time in ms between retransmitted neighbor solicitations
  
  //Prefix information: our RA contains one prefix information
  uint32_t valid_lifetime_;
  uint32_t preferred_lifetime_;
  uint32_t prefix_;             //router prefix: default is node address. Size is 128 bits

  //Advertisement Interval Option
  uint32_t advertisement_interval_; //the maximum time in millisecond between successive 
                               //unsolicited Router Advertisement

  static int offset_;
  inline static int& offset() { return offset_; }
  inline static hdr_rtads* access(Packet* p) {
    return (hdr_rtads*) p->access(offset_);
  }
  //data access methods
  inline uint8_t& cur_hop_limit() { return cur_hop_limit_; }
  //inline uint16_t& router_lifetime() { return router_lftm_; }
  inline double& router_lifetime() { return router_lftm_; }
  inline uint32_t& reachable_time () { return reachable_time_; }
  inline uint32_t& retrans_timer () { return retrans_timer_; }
  inline uint32_t& valid_lifetime () { return valid_lifetime_; }
  inline uint32_t& preferred_lifetime () { return preferred_lifetime_; }
  inline uint32_t& prefix() { return prefix_; }
  inline uint32_t& advertisement_interval () { return advertisement_interval_; }
};

/* 
 * Packet structure for router solicitations
 */
#define HDR_RTSOL(p)    ((struct hdr_rtsol*)(p)->access(hdr_rtsol::offset_))

struct hdr_rtsol {
  //place holder for packet content.

  static int offset_;
  inline static int& offset() { return offset_; }
  inline static hdr_rtsol* access(Packet* p) {
    return (hdr_rtsol*) p->access(offset_);
  }
};

// Define packet size
#define PT_RSOL_SIZE		IPv6_HEADER_SIZE + 8
#define PT_RADS_SIZE		IPv6_HEADER_SIZE + 56

//defines the types of events the ND can produce
#define ND_NEW_PREFIX 0
#define ND_PREFIX_EXPIRED 1

/*
 * Define structure for new prefix
 */
struct new_prefix {
  int prefix;      //the new prefix
  Node *interface; //the interface (the node)
};

/*
 * Define structure for expired prefix
 */
struct exp_prefix {
  int prefix;      //the prefix that expired
  Node *interface; //address of the interface (i.e. interface node address)
};


/* 
 * Neighbor discovery RA timer
 * Used for sending recurrent RAs 
 */
class RATimer : public TimerHandler {
 public:
        RATimer(Agent *a) : TimerHandler() { a_ = a; }
 protected:
        inline void expire(Event *) { a_->timeout(IPv6_TIMER_RA); }
        Agent *a_;
};

/* 
 * Neighbor discovery RS timer
 * Used for sending recurrent RAs 
 */
class RSTimer : public TimerHandler {
 public:
        RSTimer(Agent *a) : TimerHandler() { a_ = a; }
 protected:
	inline void expire(Event *) { a_->timeout(IPv6_TIMER_RS); }
        Agent *a_;
};

/* 
 * Neighbor discovery prefix timer
 * One timer is associated with each entry in the neighbor list.
 */
class PrefixTimer : public TimerHandler {
 public:
        PrefixTimer(Agent *a, int prefix) : TimerHandler() 
	  { a_ = a; prefix_ = prefix;}
 protected:
        inline void expire(Event *) { a_->timeout(prefix_); }
        Agent *a_;
	int prefix_;
};



/*
 * Neighbor list
 * Store information about a neighbor: prefix, lifetime. 
 */
class Entry;
LIST_HEAD(entry, Entry);

class Entry {
 public:
  Entry(int prefix, double time, double lifetime, Agent *a): timer_(a, prefix)  { //constructor
    prefix_ = prefix;	
    lftm_ = lifetime;
    time_ = time;
    timer_.resched (lifetime);
  }
  ~Entry() { ; } //destructor
  
  // Chain element to the list
  inline void insert_entry(struct entry *head) {
    LIST_INSERT_HEAD(head, this, link);
  }
  
  // Update entry informatino
  inline void update_entry(double time, double lifetime) {
    time_ = time;
    lftm_ = lifetime;
    timer_.resched (lifetime);
  }
  // Return the expiration
  inline double expire() { return (time_ + lftm_) ; }
  // Return the router lifetime
  inline double& lifetime() { return lftm_; }
  // Return the prefix information
  inline int& prefix() {return prefix_; }
  // Return next element in the chained list
  Entry* next_entry(void) const { return link.le_next; }
  // Remove the entry from the list
  inline void remove_entry() { 
    LIST_REMOVE(this, link); 
  }

  //neighbor discovery fields
  int prefix_;            // router prefix
  double lftm_; 	  // lifetime of this entry 
  double time_; 	  // time at which entry was updated 
  PrefixTimer timer_;     // expiration timer

 protected:
  LIST_ENTRY(Entry) link; 
};

/*
 * List used for storing destination of RA/RS when broadcast 
 * cannot be used (Ethernet, UMTS...). 
 */
class Target;
LIST_HEAD(target, Target);

class Target {
 public:
  Target(int destination) { //constructor
    destination_ = destination;	
  }
  ~Target() { ; } //destructor
  
  // Chain element to the list
  inline void insert_entry(struct target *head) {
    LIST_INSERT_HEAD(head, this, link);
  }
  
  // Return the destination
  inline int destination() { return destination_; }
  // Return next element in the chained list
  Target* next_entry(void) const { return link.le_next; }
  // Remove the entry from the list
  inline void remove_entry() { 
    LIST_REMOVE(this, link); 
  }

  int destination_;

 protected:
  LIST_ENTRY(Target) link; 
};

/*
 * List used for storing RS Reply timers
 */
class RSreplyTimer;
class RSReplyEntry;
LIST_HEAD(rsReplyEntry, RSReplyEntry);

class RSReplyEntry {
 public:
  RSReplyEntry(RSreplyTimer *t) { //constructor
    timer_ = t;	
  }
  ~RSReplyEntry() { ; } //destructor
  
  // Chain element to the list
  inline void insert_entry(struct rsReplyEntry *head) {
    LIST_INSERT_HEAD(head, this, link);
  }
  
  // Return the timer
  inline RSreplyTimer* timer() { return timer_; }
  // Return next element in the chained list
  RSReplyEntry* next_entry(void) const { return link.le_next; }
  // Remove the entry from the list
  inline void remove_entry() { 
    LIST_REMOVE(this, link); 
  }

  RSreplyTimer *timer_;

 protected:
  LIST_ENTRY(RSReplyEntry) link; 
};


/* 
 * Neighbor discovery agent
 */
class IFMNGMTAgent;

class NDAgent : public Agent {

  friend class RSreplyTimer; 

 public:
  NDAgent();
  int command(int argc, const char*const* argv);
  void recv(Packet*, Handler*);
  void timeout(int);
  void register_ifmodule (IFMNGMTAgent *);
  void send_rs ();
  struct entry nlist_head_;    //list of neighbors

 protected:
  Node *node_;                 //my node
  double rtrlftm_;             //router lifetime in seconds
  uint32_t prefix_lifetime_;   //prefix lifetime in seconds
  int useAdvInterval_;         //1 to use Advertisement interval, 0 to use router lifetime
  int router_;                 //to enable/disable Advertising interface
  double minRtrAdvInterval_;   //minimal time allowed between sending RA
  double maxRtrAdvInterval_;   //maximal time allowed between sending RA
  double minDelayBetweenRA_;   //minimum delay between 2 RAs
  double maxRADelay_;          //maximum delay for RS answer
  int default_port_;           //default port number for ND
  double rs_timeout_;          //timeout if no RA has been received after sending RS
  int max_rtr_solicitation_;   //max number of RS transmissions
  int nb_rtr_sol_;             //current number of transmission

  RATimer timer_;              //RA timer
  RSTimer rsTimer_;            //RS timer
 double lastRA_time_;         //when was the last RA sent
  double nextRA_time_;         //when is the next RA scheduled.
  int broadcast_;              //enable broadcast RAs
  struct target ra_daddrs_;     //list of the address to send RA. Used when Broadcast
                               // is not supported by the technology (i.e UMTS, Ethernet)
  struct target rs_daddrs_;     //list of the address to send RS. Used when Broadcast
                               // is not supported by the technology (i.e UMTS, Ethernet)

  struct rsReplyEntry rs_list_head_; //Store the list of pending RS replies
 
  //the interface management module
  IFMNGMTAgent *iMngmnt_;

  // List Management
  Entry* lookup_entry(int, Entry *);	
  Entry* head(struct entry head) { return head.lh_first; }
  Entry* add_prefix(int, double);

  // Internal functions
  int add_ra_target (int);     //Add target for RAs
  int remove_ra_target (int);  //Remove target for RAs
  int add_rs_target (int);     //Add target for RSs
  int remove_rs_target (int);  //Remove target for RSs
  void send_ads(int);          //Send a RA
  void recv_ads(Packet *);     //Process RA
  void send_rs(int);           //Send RS
  void recv_rs(Packet *);      //Process RS
  void process_rsreply (RSreplyTimer *);

  //debug
  void dump_table ();
};

/* 
 * Neighbor discovery RS_reply timer
 * Used for sending a RA in response to an RA
 */
class RSreplyTimer : public TimerHandler {
 public:
        RSreplyTimer(NDAgent *a, int daddr) : TimerHandler() 
	  { 
	    a_ = a; 
	    daddr_ = daddr;
	  }
	inline int& address() { return daddr_; }
 protected:
        inline void expire(Event *) { a_->process_rsreply(this); }
        NDAgent *a_;
	int daddr_;
};


#endif //nd_h
