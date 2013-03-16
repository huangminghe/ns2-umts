/*
 * Include file for Handover module
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
 * NIST ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" COEITION AND
 * DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING
 * FROM THE USE OF THIS SOFTWARE.
 * </PRE></P>
 * @author  rouil
 * @version 1.0
 * <P><PRE>
 * VERSION CONTROL<BR>
 * -------------------------------------------------------------------------<BR>
 * Name  - YYYY/MM/DD - VERSION - ACTION<BR>
 * rouil - 2005/05/20 - 1.0     - Source created<BR>
 * <BR>
 * <BR>
 * </PRE><P>
 */

#ifndef handover_h
#define handover_h

#include "mip6.h"

#define REGISTRATION_BACKOFF 0.001

class RegistrationTimer;
class DetectionTimer;

/*
 * Defines the Handover module
 */
class Handover : public MIPV6Agent {

 public:
  Handover();

  //processing events
  virtual void process_scan_response (mih_scan_response_s*);
  virtual void process_no_link_detected (void *);
  virtual void process_get_status_response (mih_get_status_s*);

  //utils
  mih_get_status_s *get_mac_info (int);

  virtual void register_mih ();
  virtual void register_remote_mih (Mac *);

 protected:
  /*
   * Copmute the new address using the new prefix received
   * @param prefix The new prefix received
   * @param interface The interface to update
   */
  int compute_new_address (int prefix, Node *interface);

  RegistrationTimer *regTimer_; //timer to call registration
  DetectionTimer *detectionTimer_; //timer for scan timeout
  double detectionTimeout_;
};

/* 
 * Mac list timer
 * Timer to register events to the MAC
 */
class RegistrationTimer : public TimerHandler {
 public:
        RegistrationTimer(Handover *a) : TimerHandler() 
	  { a_ = a; }
 protected:
        inline void expire(Event *) { a_->register_mih(); }
        Handover *a_;
};

/* 
 * Detection timeout
 * Used for protection if we don't hear answer from MAC in time.
 */
class DetectionTimer : public TimerHandler {
 public:
  DetectionTimer(Handover *a) : TimerHandler() { a_ = a;}
  inline void expire(Event *) { a_->process_no_link_detected(e_); }
  inline void attach_event(void *e) { e_ = e; }
  protected:
  Handover *a_;
  void *e_; //can be any type of event (link down, link going down...)
};


#endif 
