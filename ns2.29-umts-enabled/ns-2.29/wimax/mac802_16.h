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

#ifndef MAC802_16_H
#define MAC802_16_H

#include "sduclassifier.h"
#include "connectionmanager.h"
#include "serviceflowhandler.h"
#include "serviceflowqos.h"
#include "peernode.h"
#include "mac.h"
#include "mac802_16pkt.h"
#include "mac802_16timer.h"

//Define new debug function for cleaner code
#ifdef DEBUG_WIMAX
#define debug2 printf 
#else
#define debug2(arg1,...) 
#endif

#define BS_NOT_CONNECTED -1 //bs_id when MN is not connected

#define DL_PREAMBLE 3  //preamble+fch
#define INIT_RNG_PREAMBLE 2
#define BW_REQ_PREAMBLE 1


/** Defines different types of nodes */
enum station_type_t {
  STA_UNKNOWN,
  STA_MN,
  STA_BS
};

/** Defines the state of the MAC */
enum Mac802_16State {
  MAC802_16_DISCONNECTED,
  MAC802_16_WAIT_DL_SYNCH,
  MAC802_16_WAIT_DL_SYNCH_DCD,
  MAC802_16_UL_PARAM,
  MAC802_16_RANGING,
  MAC802_16_WAIT_RNG_RSP,
  MAC802_16_REGISTER,
  MAC802_16_SCANNING,
  MAC802_16_CONNECTED
};

/** Data structure to store MAC state */
struct state_info {
  Mac802_16State state; 
  int bs_id;
  double frameduration;
  int frame_number;
  int channel;
  ConnectionManager * connectionManager;
  ServiceFlowHandler * serviceFlowHandler;
  struct peerNode *peer_list;
};

/** Defines profiles */
struct phyprofile {
  int nb_channel; //number of valid channel in the array
  int current; //index of the channel currently used
  double freq[]; //list of channel frequencies
};

/** MAC MIB */
class Mac802_16MIB {
 public: 
  Mac802_16MIB (Mac802_16 *parent);
 
  int queue_length;
  double frame_duration;

  double dcd_interval;
  double ucd_interval;
  double init_rng_interval;
  double lost_dlmap_interval;
  double lost_ulmap_interval;
  
  double t1_timeout;
  double t2_timeout;
  double t3_timeout;
  double t6_timeout;
  double t12_timeout;
  double t16_timeout;
  double t17_timeout;
  double t21_timeout;
  double t44_timeout;

  u_int32_t contention_rng_retry;
  u_int32_t invited_rng_retry;
  u_int32_t request_retry;
  u_int32_t reg_req_retry;
  double    tproc;
  u_int32_t dsx_req_retry;
  u_int32_t dsx_rsp_retry;

  u_int32_t rng_backoff_start;
  u_int32_t rng_backoff_stop;
  u_int32_t bw_backoff_start;
  u_int32_t bw_backoff_stop;

  //mobility extension
  u_int32_t scan_duration;
  u_int32_t interleaving;
  u_int32_t scan_iteration;
  u_int32_t max_dir_scan_time;
  double    nbr_adv_interval;
  u_int32_t scan_req_retry;

  //miscalleous
  double rxp_avg_alpha;  //for measurements
  double lgd_factor_; 
  double RXThreshold_;
  double client_timeout; //used to clear information on BS side
};

/** PHY MIB */
class Phy802_16MIB {
 public: 
  Phy802_16MIB (Mac802_16 *parent);
 
  int channel; //current channel
  double fbandwidth;
  u_int32_t ttg; 
  u_int32_t rtg;
};

class WimaxScheduler;
class FrameMap;
class StatTimer;
/**
 * Class implementing IEEE 802_16
 */ 
class Mac802_16 : public Mac {

  friend class PeerNode;
  friend class SDUClassifier;
  friend class WimaxFrameTimer;
  friend class FrameMap;
  friend class WimaxScheduler;
  friend class BSScheduler;
  friend class SSscheduler;
  friend class ServiceFlowHandler;
  friend class Connection;
  friend class StatTimer;
 public:

  Mac802_16();

  /**
   * Return the connection manager
   * @return the connection manager
   */
  inline ConnectionManager *  getCManager () { return connectionManager_; }
  
  /**
   * Return The Service Flow handler
   * @return The Service Flow handler
   */
  inline ServiceFlowHandler *  getServiceHandler () { return serviceFlowHandler_; }
  
  /**
   * Return the Scheduler
   * @return the Scheduler
   */
  inline WimaxScheduler * getScheduler () { return scheduler_; }

  /**
   * Return the frame duration (in s)
   * @return the frame duration (in s)
   */
  double  getFrameDuration () { return macmib_.frame_duration; }
  
  /**
   * Set the frame duration
   * @param duration The frame duration (in s)
   */
  void  setFrameDuration (double duration) { macmib_.frame_duration = duration; }
  
  /**
   * Return the current frame number
   * @return the current frame number
   */
  int getFrameNumber ();

  /**
   * Add a flow
   * @param qos The QoS required
   * @param handler The entity that requires to add a flow
   */
  void  addFlow (ServiceFlowQoS * qos, void * handler);

  /**
   * Return the head of the peer nodes list
   * @return the head of the peer nodes list
   */
  PeerNode * getPeerNode_head () { return peer_list_->lh_first; }

  /**
   * Return the peer node that has the given address
   * @param index The address of the peer
   * @return The peer node that has the given address
   */
  PeerNode *getPeerNode (int index);

  /**
   * Add the peer node
   * @param The peer node to add
   */
  void addPeerNode (PeerNode *node);

  /**
   * Remove a peer node
   * @param The peer node to remove
   */
  void removePeerNode (PeerNode *node);

  /**
   * Interface with the TCL script
   * @param argc The number of parameter
   * @param argv The list of parameters
   */
  int command(int argc, const char*const* argv);

  /**
   * Set the mac state
   * @param state The new mac state
   */  
  void setMacState (Mac802_16State state);

  /**
   * Return the mac state
   * @return The new mac state
   */  
  Mac802_16State getMacState ();

  /**
   * Change the channel
   * @param channel The new channel
   */
  void setChannel (int channel);

  /**
   * Return the channel index
   * @return The channel
   */
  int getChannel ();

  /**
   * Return the channel number for the given frequency
   * @param freq The frequency
   * @return The channel number of -1 if the frequency does not match
   */
  int getChannel (double freq);

  /**
   * Set the channel to the next from the list
   * Used at initialisation and when loosing signal
   */
  void nextChannel ();

  /**
   * Process packets going out
   * @param p The packet to transmit
   */
  void sendDown(Packet *p);

  /**
   * Process packets going out
   * @param p The packet to transmit
   */
  void transmit(Packet *p);
        
  /**
   * Process incoming packets 
   * @param p The received packet
   */
  void sendUp(Packet *p);

  /**
   * Process the packet after receiving last bit
   */
  void receive();

  /**
   * Creates a snapshot of the MAC's state and reset it
   * @return The snapshot of the MAC's state
   */
  state_info *backup_state ();

  /**
   * Restore the state of the Mac
   * @param state The state to restore
   */
  void restore_state (state_info *state);  

  /**
   * Set the variable used to find out if upper layers
   * must be notified to send packets. During scanning we
   * do not want upper layers to send packet to the mac.
   * @param notify Value indicating if we want to receive packets 
   * from upper layers
   */
  void setNotify_upper (bool notify);

  /**
   * Return the PHY layer
   * @return The physical layer
   */
  OFDMPhy* getPhy ();

  /**
   * The MAC MIB
   */
   Mac802_16MIB macmib_;

   /**
    * The Physical layer MIB
    */
   Phy802_16MIB phymib_;

#ifdef USE_802_21 //Switch to activate when using 802.21 modules (external package)
   /* 
    * Configure/Request configuration
    * The upper layer sends a config object with the required 
    * new values for the parameters (or PARAMETER_UNKNOWN_VALUE).
    * The MAC tries to set the values and return the new setting.
    * For examples if a MAC does not support a parameter it will
    * return  PARAMETER_UNKNOWN_VALUE
    * @param config The configuration object
    */ 
   void link_configure (link_parameter_config_t* config);

   /* 
    * Configure the threshold values for the given parameters
    * @param numLinkParameter number of parameter configured
    * @param linkThresholds list of parameters and thresholds
    */
   struct link_param_th_status * link_configure_thresholds (int numLinkParameter, struct link_param_th *linkThresholds); //configure threshold
        
   /*
    * Disconnect from the PoA
    */
   void link_disconnect ();

   /*
    * Connect to the PoA
    * @param poa The address of PoA
    */
   void link_connect (int poa);
   
   /*
    * Scan channel
    * @param req the scan request information
    */
   void link_scan (void *req);

   /*
    * Set the operation mode
    * @param mode The new operation mode
    * @return true if transaction succeded
    */
   bool set_mode (mih_operation_mode_t mode); 
#endif
   
 protected:
   /**
    * Initialize default connection
    */
   void init_default_connections ();

   /**
    * The packet scheduler
    */
   WimaxScheduler * scheduler_;
   
   /**
    * Return a new allocated packet
    * @return A newly allocated packet 
    */
   Packet * getPacket();
   
   /*
    * Return the code for the frame duration
    * @return the code for the frame duration
    */
   int getFrameDurationCode ();
   
   /*
    * Set the frame duration using code
    * @param code The frame duration code
    */
   void setFrameDurationCode (int code);
   
   /**
    * Current frame number
    */
   int frame_number_;
   
   /**
    * Statistics for queueing delay
    */
   StatWatch delay_watch_; 
   
   /**
    * Delay for last packet
    */
   double last_tx_delay_;

   /**
    * Statistics for delay jitter 
    */
   StatWatch jitter_watch_;
   
   /**
    * Stats for packet loss
    */
   StatWatch loss_watch_;

   /**
    * Stats for incoming data throughput
    */
   ThroughputWatch rx_data_watch_;

   /**
    * Stats for incoming traffic throughput (data+management)
    */
   ThroughputWatch rx_traffic_watch_;


   /**
    * Stats for outgoing data throughput
    */
   ThroughputWatch tx_data_watch_;

   /**
    * Stats for outgoing traffic throughput (data+management)
    */
   ThroughputWatch tx_traffic_watch_;

   /**
    * Timers to continuously poll stats in case it is not updated by
    * sending or receiving packets
    */
   StatTimer *rx_data_timer_;
   StatTimer *rx_traffic_timer_;
   StatTimer *tx_data_timer_;
   StatTimer *tx_traffic_timer_;

   /**
    * Indicates if the stats must be printed
    */
   int print_stats_;
   
   /**
    * Update the given timer and check if thresholds are crossed
    * @param watch the stat watch to update
    * @param value the stat value
    */
   void update_watch (StatWatch *watch, double value);

   /**
    * Update the given timer and check if thresholds are crossed
    * @param watch the stat watch to update
    * @param size the size of packet received
    */
   void update_throughput (ThroughputWatch *watch, double size);

#ifdef USE_802_21 //Switch to activate when using 802.21 modules (external package)
   /**
    * Poll the given stat variable to check status
    * @param type The link parameter type
    */
   void poll_stat (link_parameter_type_s type);
#endif

   /** 
    * Log the packet. Private in Mac so we need to redefine it
    * @param p The received packet
    */
   inline void mac_log(Packet *p) {
     logtarget_->recv(p, (Handler*) 0);
   }
   
   /** 
    * Object to log received packets. Private in Mac so we need to redefine it
    */
   NsObject*	logtarget_;

 private:
   /**
    * The list of classifier
    */
   struct sduClassifier classifier_list_;
   
   /**
    * List of connected peer nodes. Only one for SSs.
    */
   struct peerNode *peer_list_;
   
   /**
    * The class to handle connections
    */
   ConnectionManager * connectionManager_;
   
   /**
    * The module that handles flow requests
    */
   ServiceFlowHandler * serviceFlowHandler_;

   /**
    * Packet being received
    */
   Packet *pktRx_;

   /**
    * A packet buffer used to temporary store a packet 
    * received by upper layer. Used during scanning
    */
   Packet *pktBuf_;

   /**
    * Add a classifier
    */
   void addClassifier (SDUClassifier *);

   /**
    * Set the node type
    * @param type The station type
    */
   void setStationType (station_type_t type);

   /*
    * The type of station (MN or BS) 
    */
   station_type_t type_;

   /*
    * Address of the Base Station. If STA is BS then equal index_
    */
   int bs_id_;

   /*
    * The state of the MAC
    */
   Mac802_16State state_;

   /**
    * Receiving timer
    */
   WimaxRxTimer rxTimer_;

   /**
    * Indicates if a collision occured
    */
   bool collision_;

   /**
    * Indicate if upper layer must be notified to send more packets
    */
   bool notify_upper_;

   /**
    * Last time a packet was sent
    */
   double last_tx_time_;

   /**
    * Last transmission duration
    */
   double last_tx_duration_;

};

/** Class to poll stats */
class StatTimer : public TimerHandler {
 public:
  StatTimer (Mac802_16 *mac, ThroughputWatch *watch) : TimerHandler() {
    mac_ = mac;
    watch_ = watch;
    timer_interval_ = 0.100000000001; //default 100ms+a little off to avoid synch
    resched (timer_interval_);
  }
    void expire (Event *) {
      mac_->update_throughput (watch_, 0);
      //double tmp = watch_->get_timer_interval();
      //resched(tmp > 0? tmp: timer_interval_);
    }
    inline void set_timer_interval(double ti) { timer_interval_ = ti; }
 private:
    Mac802_16 *mac_;
    ThroughputWatch *watch_;
    double timer_interval_;
};

#endif //MAC802_16_H

