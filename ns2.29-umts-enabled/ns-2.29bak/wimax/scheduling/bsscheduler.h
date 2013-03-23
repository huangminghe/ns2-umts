/* This software was developed at the National Institute of Standards and
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
 */

#ifndef BSSCHEDULER_H
#define BSSCHEDULER_H

#include "wimaxscheduler.h"
#include "scanningstation.h"

#define INIT_DL_DURATION 20 //enough for DL_MAP, UL_MAP, DCD, UCD and some RNG-RSP
#define MIN_CONTENTION_SIZE 5 //minimum number of opportunity for allocation

/** Information about a new client */
struct new_client_t {
  int cid; //primary cid of new client
  new_client_t *next;
};

class T17Element;
LIST_HEAD (t17element, T17Element);
/** Object to handle timer t17 */
class T17Element {
 public:
  T17Element (Mac802_16 *mac, int index) {
    index_ = index;
    timer = new WimaxT17Timer (mac, index);
    timer->start (mac->macmib_.t17_timeout);
  }

  ~T17Element () { delete (timer); }

  int index () { return index_; }
  int paused () { return timer->paused(); }
  void cancel () { timer->stop(); }

  // Chain element to the list
  inline void insert_entry(struct t17element *head) {
    LIST_INSERT_HEAD(head, this, link);
  }
  
  // Return next element in the chained list
  T17Element* next_entry(void) const { return link.le_next; }

  // Remove the entry from the list
  inline void remove_entry() { 
    LIST_REMOVE(this, link); 
  }
 protected:
  
  /*
   * Pointer to next in the list
   */
  LIST_ENTRY(T17Element) link;
  //LIST_ENTRY(T17Element); //for magic draw

 private:
  int index_;
  WimaxT17Timer *timer;
};

class FastRangingInfo;
LIST_HEAD (fastRangingInfo, FastRangingInfo);
/** Store information about a fast ranging request to send */
class FastRangingInfo {
 public:
  FastRangingInfo (int frame, int macAddr) {
    frame_ = frame;
    macAddr_ = macAddr;
  }

  int frame () { return frame_; }
  int macAddr () { return macAddr_; }

  // Chain element to the list
  inline void insert_entry(struct fastRangingInfo *head) {
    LIST_INSERT_HEAD(head, this, link);
  }
  
  // Return next element in the chained list
  FastRangingInfo* next_entry(void) const { return link.le_next; }

  // Remove the entry from the list
  inline void remove_entry() { 
    LIST_REMOVE(this, link); 
  }
 protected:
  
  /*
   * Pointer to next in the list
   */
  LIST_ENTRY(FastRangingInfo) link;
  //LIST_ENTRY(FastRangingInfo); //for magic draw

 private:
  int frame_;
  int macAddr_;
};

class WimaxCtrlAgent;
/**
 * Class BSScheduler
 * Implement the packet scheduler on the BS side
 */ 
class BSScheduler : public WimaxScheduler {
  //friend class SendTimer;
 public:
  /*
   * Create a scheduler
   */
  BSScheduler ();

  /*
   * Interface with the TCL script
   * @param argc The number of parameter
   * @param argv The list of parameters
   */
  int command(int argc, const char*const* argv);
    
  /**
   * Process a packet received by the Mac. Only scheduling related packets should be sent here (BW request, UL_MAP...)
   * @param p The packet to process
   */
  void  process (Packet * p);

  /**
   * Return the type of STA this scheduler is good for
   */
  virtual station_type_t getNodeType ();

  /**
   * Initializes the scheduler
   */
  virtual void init ();
 
  /**
   * Called when a timer expires
   * @param The timer ID
   */
  virtual void expire (timer_id id);

  /**
   * Start a new DL subframe
   */
  virtual void start_dlsubframe ();

  /**
   * Start a new UL subframe
   */
  virtual void start_ulsubframe ();

  /** 
   * Finds out if the given station is currently scanning
   * @param nodeid The MS id
   */
  bool isPeerScanning (int nodeid);

  /**
   * Set the control agent
   * @param agent The control agent
   */
  void setCtrlAgent (WimaxCtrlAgent *agent);

  /** Add a new Fast Ranging allocation
   * @param time The time when to allocate data
   * @param macAddr The MN address
   */
  void addNewFastRanging (double time, int macAddr);

  /**
   * Send a scan response to the MN
   * @param rsp The response from the control
   * @cid The CID for the MN
   */
  void send_scan_response (mac802_16_mob_scn_rsp_frame *rsp, int cid);

 protected:

  /**
   * Default modulation 
   */
  Ofdm_mod_rate default_mod_;

  /**
   * Number of transmission opportunity for initial ranging
   * and bw request (i.e contention slots)
   */
  int contention_size_; 

  /**
   * Compute and return the bandwidth request opportunity size
   * @return The bandwidth request opportunity size
   */
  int getBWopportunity ();

  /**
   * Compute and return the initial ranging opportunity size
   * @return The initial ranging opportunity size
   */
  int getInitRangingopportunity ();  

 private:

  /**
   * Process a RNG-REQ message
   * @param frame The ranging request information
   */
  void process_ranging_req (Packet *p);

  /**
   * Process bandwidth request
   * @param p The request
   */
  void process_bw_req (Packet *p);

  /**
   * Process bandwidth request
   * @param p The request
   */
  void process_reg_req (Packet *p);

  /**
   * Process handover request
   * @param req The request
   */
  void process_msho_req (Packet *req);
 
  /**
   * Process handover indication
   * @param p The indication
   */
  void process_ho_ind (Packet *p);
 
  /**
   * Send a neighbor advertisement message
   */
  void send_nbr_adv ();


  /**
   * Add a new timer 17 for client
   * @param index The client address
   */
  void addtimer17 (int index);
  
  /**
   * Cancel and remove the timer17 associated with the node
   * @param index The client address
   */
  void removetimer17 (int index);

  struct new_client_t *cl_head_;
  struct new_client_t *cl_tail_;

  struct t17element t17_head_;

  /**
   * The index of the last SS that had bandwidth allocation
   */
  int bw_node_index_;
  
  /**
   * The node that had the last bandwidth allocation
   */
  PeerNode *bw_peer_; 

  /**
   * Timer for DCD
   */
  WimaxDCDTimer *dcdtimer_; 

  /**
   * Timer for UCD
   */
  WimaxUCDTimer *ucdtimer_;

  /**
   * Timer for MOB-NBR_ADV messages
   */
  WimaxMobNbrAdvTimer *nbradvtimer_;

  /**
   * Indicates if it is time to send a DCD message
   */
  bool sendDCD;
  int dlccc_;
  bool sendUCD;
  int ulccc_;
  
  /** 
   * List of station in scanning 
   */
  struct scanningStation scan_stations_;

  /**
   * The Wimax control for BS synchronization
   */
  WimaxCtrlAgent *ctrlagent_;

  /**
   * List of the upcoming Fast Ranging allocation 
   */
  struct fastRangingInfo fast_ranging_head_;

};

#endif //BSSCHEDULER_H

