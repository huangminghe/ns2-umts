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

#ifndef SSSCHEDULER_H
#define SSSCHEDULER_H

#include "wimaxscheduler.h"

/** The state of the scheduler */
enum ss_sched_state {
  NORMAL,           //Normal state
  SCAN_PENDING,     //Normal period but pending scanning to start/resume
  SCANNING,         //Currently scanning
  HANDOVER_PENDING, //Normal state but handover to start
  HANDOVER          //Executing handover
};


/** Data structure to store scanning information */
struct scanning_structure {
  struct mac802_16_mob_scn_rsp_frame *rsp; //response from BS
  struct sched_state_info scan_state;     //current scanning state
  struct sched_state_info normal_state;   //backup of normal state
  int iteration;                          //current iteration
  WimaxScanIntervalTimer *scn_timer_;     //timer to notify end of scanning period
  int count;                              //number of frame before switching to scanning  
  ss_sched_state substate;
  WimaxNeighborEntry *nbr; //current neighbor during scanning or handover
  //arrays of rdv timers
  WimaxRdvTimer *rdv_timers[2*MAX_NBR];
  int nb_rdv_timers;
  //handoff information
  int serving_bsid;
  int handoff_timeout; //number frame to wait before executing handoff
};

/**
 * Class SSscheduler
 * Scheduler for SSs
 */ 
class SSscheduler : public WimaxScheduler {
  friend class Mac802_16;
 public:
  /**
   * Create a scheduler
   */
  SSscheduler ();

  /**
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

 protected:

  /**
   * Called when lost synchronization
   */
  void lost_synch ();

  /**
   * Send a scanning message to the serving BS
   */
  void send_scan_request ();

 private:

  /**
   * Process a DL_MAP message
   * @param frame The dl_map information
   */
  void process_dl_map (mac802_16_dl_map_frame *frame);

  /**
   * Process a DCD message
   * @param frame The dcd information
   */
  void process_dcd (mac802_16_dcd_frame *frame);

  /**
   * Process a uL_MAP message
   * @param frame The ul_map information
   */
  void process_ul_map (mac802_16_ul_map_frame *frame);

  /**
   * Process a UCD message
   * @param frame The ucd information
   */
  void process_ucd (mac802_16_ucd_frame *frame);

  /**
   * Process a ranging response message 
   * @param frame The ranging response frame
   */
  void process_ranging_rsp (mac802_16_rng_rsp_frame *frame);
  
  /**
   * Process a registration response message 
   * @param frame The registration response frame
   */
  void process_reg_rsp (mac802_16_reg_rsp_frame *frame);

  /**
   * Schedule a ranging
   */
  void init_ranging ();

  /**
   * Create a request for the given connection
   * @param con The connection to check
   */
  void create_request (Connection *con);

  /**
   * Prepare to send a registration message
   */
  void send_registration ();

  /**
   * Process a scanning response message 
   * @param frame The scanning response frame
   */
  void process_scan_rsp (mac802_16_mob_scn_rsp_frame *frame);  

  /**
   * Process a BSHO-RSP message 
   * @param frame The handover response frame
   */
  void process_bsho_rsp (mac802_16_mob_bsho_rsp_frame *frame); 
  
  /*
   * Connect to the PoA
   */
  void link_connect(int poa);

  /**
   * Process a BSHO-RSP message 
   * @param frame The handover response frame
   */
  void process_nbr_adv (mac802_16_mob_nbr_adv_frame *frame);  

  /**
   * Start/Continue scanning
   */
  void resume_scanning ();

  /**
   * Pause scanning
   */
  void pause_scanning ();

  /**
   * Send a MSHO-REQ message to the BS
   */
  void send_msho_req ();

  /**
   * Check rdv point when scanning
   */
  void check_rdv ();

  /**
   * Set the scan flag to 1
   */
  void setScanFlag();

  /**
   * Reset the scan flag
   */
  void resetScanFlag();

  /**
   * return scan flag
   */
  bool isScanRunning();

  /**
   * Current number of registration retry
   */
  u_int32_t nb_reg_retry_;

  /**
   * Current number of scan request retry
   */
  u_int32_t nb_scan_req_;

  /**
   * Timers
   */
  WimaxT1Timer  *t1timer_;
  WimaxT2Timer  *t2timer_;
  WimaxT6Timer  *t6timer_;
  WimaxT12Timer *t12timer_;
  WimaxT21Timer *t21timer_;
  WimaxLostDLMAPTimer *lostDLMAPtimer_;
  WimaxLostULMAPTimer *lostULMAPtimer_;
  WimaxT44Timer *t44timer_;

  /** 
   * The scanning information
   */
  struct scanning_structure *scan_info_;

  /**
   * Indicate if the ranging response is for a fast ranging
   */
  bool fast_ranging_;

 private:
  bool scanFlag;
  
};

#endif //SSSCHEDULER_H

