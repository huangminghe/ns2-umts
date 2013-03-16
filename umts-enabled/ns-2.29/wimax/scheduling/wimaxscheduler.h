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

#ifndef WIMAX_SCHEDULER_H
#define WIMAX_SCHEDULER_H

#include "packet.h"
#include "mac802_16.h"
#include "framemap.h"
#include "neighbordb.h"

class WimaxScheduler;
/** Timer to indicate a new Downlink frame */
class DlTimer : public TimerHandler {
 public:
  DlTimer(WimaxScheduler *s) : TimerHandler() {s_=s;}
  
  void	expire(Event *e);
 private:
  WimaxScheduler *s_;
}; 

/** Timer to indicate a new uplink frame */
class UlTimer : public TimerHandler {
 public:
  UlTimer(WimaxScheduler *s) : TimerHandler() {s_=s;}
  
  void	expire(Event *e);
 private:
  WimaxScheduler *s_;
}; 


/**
 * Super class for schedulers (BS and MS schedulers)
 */ 
class WimaxScheduler : public TclObject 
{
  friend class FrameMap;
  friend class WimaxCtrlAgent;
public:
  /*
   * Create a scheduler
   */
  WimaxScheduler ();

  /*
   * Set the mac
   * @param mac The Mac where it is located
   */
  void setMac (Mac802_16 *mac);

  /**
   * Process a packet received by the Mac. Only scheduling related packets should be sent here (BW request, UL_MAP...)
   * @param p The packet to process
   */
  virtual void  process (Packet * p);

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
   * Return the Mac layer
   */
  inline Mac802_16 *  getMac () { return mac_;}
    
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
   * Transfert the packets from the given connection to the given burst
   * @param con The connection
   * @param b The burst
   * @param duration The current occupation of the burst
   * @return the new burst occupation
   */
  int transfer_packets (Connection *c, Burst *b, int duration);

  /**
   * The Mac layer
   */
  Mac802_16 * mac_;
  
  /**
   * Packets scheduled to be send this frame
   */
  PacketQueue queue_;

  /** 
   * The map of the frame
   */
  FrameMap *map_;

  /**
   * Timer used to mark the begining of downlink subframe (i.e new frame)
   */
  DlTimer *dl_timer_;

  /**
   * Timer used to mark the begining of uplink subframe
   */
  UlTimer *ul_timer_;

  /**
   * Database of neighboring BS
   */
  NeighborDB *nbr_db_;
private:



};
#endif //SCHEDULER_H

