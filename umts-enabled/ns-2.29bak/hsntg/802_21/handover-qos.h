/*
 * Include file for Simple Handover module for MIPv6
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
 * rouil - 2005/05/22 - 1.0     - Source created<BR>
 * <BR>
 * <BR>
 * </PRE><P>
 */

#ifndef handover_qos_h
#define handover_qos_h

#include "handover.h"

class ConfTimer;

/*
 * Defines the Handover module
 */
class HandoverQos : public Handover {

 public:
  HandoverQos();

  void process_link_detected (link_detected_t *);
  void process_link_up (link_up_t *);
  void process_link_down (link_down_t *);
  void process_link_going_down (link_going_down_t *);
  void process_link_parameter_change (link_parameter_change_t *);

  void process_new_prefix (new_prefix*);
  void process_exp_prefix (exp_prefix*);
  void register_mih ();

  void set_link_thresholds (); //configure link thresholds

 protected:
  Mac *connectingMac_; //store the mac that is currently connecting.
  int compute_new_address(int, Node*);
  ConfTimer *ctimer_;
  double ptd_; //packet transfert delay requirement
  double pdv_; //packet jitter requirement
  double plr_; //packet loss requirement
  double per_; //packet error requirement
  double throughput_; //packet throughput requirement
  double app_start_; //start time of application
};

/** Class to poll stats */
class ConfTimer : public TimerHandler {
 public:
  ConfTimer (HandoverQos *hand, double start) : TimerHandler() {
    handover_ = hand;
    resched (start);
  }
    void expire (Event *) {
      handover_->set_link_thresholds ();
    }
 private:
    HandoverQos *handover_;
};


#endif
