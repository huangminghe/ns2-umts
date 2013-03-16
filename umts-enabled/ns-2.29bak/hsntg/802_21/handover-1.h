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

#ifndef handover_infocom_h
#define handover_infocom_h

#include "handover.h"
#include "simulator.h"
#include <map>


class PrefixHandleTimer;
class ScanTimer;
/*
 * Defines the Handover module
 */
class Handover1 : public Handover {
 private:
  ScanTimer *scanTimer_;
 public:
  Handover1();

  void process_link_detected (link_detected_t *);
  void process_link_up (link_up_t *);
  void process_link_down (link_down_t *);
  void process_link_going_down (link_going_down_t *);

  void process_new_prefix (new_prefix*);
  void process_exp_prefix (exp_prefix*);
  void register_mih ();

  /**
   * Process MIH_Capability_Discover.confirm
   * @param mihh MIH packet containing the response
   */
  void process_cap_disc_conf (hdr_mih *mihh);

  /**
   * Process MIH_Register.confirm
   * @param rsp the result of the registration
   */
  void process_register_conf (struct mih_reg_rsp *rsp);

  /**
   * Process MIH_Deregister.confirm
   * @param rsp the result of the deregistration
   */
  void process_deregister_conf (struct mih_dereg_rsp *rsp);

  /**
   * Process MIH_Scan.confirm
   * @param rsp The result to scan request
   */
  void process_scan_conf (struct mih_scan_rsp *rsp);

  void process_scanTimer(struct link_identifier_t linkIdentifier);

 protected:
  int case_;  //scenario case (1, 2 or 3)
  double confidence_th_;
  int connectingMac_; //store the mac that is currently connecting.
  map <int,int> pending_scan; //store the linkId of each interface 
};

/* 
 * Mac list timer
 * Timer to register events to the MAC
 */
class PrefixHandleTimer : public TimerHandler {
 public:
        PrefixHandleTimer(Handover1 *a) : TimerHandler() 
	  { a_ = a; }
	inline void new_data (new_prefix *data) { data_ = data; }
 protected:
        inline void expire(Event *) {
	  if (data_) {a_->process_new_prefix(data_);}
	}
        Handover1 *a_;
	new_prefix *data_;
};


/* 
 * Scan timeout
 * Used for protection if we don't hear answer from MAC in time.
 */
class ScanTimer : public TimerHandler {
 public:
  ScanTimer(Handover1 *a, struct link_identifier_t linkIdentifier) : TimerHandler() { a_ = a; linkIdentifier_ = linkIdentifier; }
  inline void expire(Event *) { a_->process_scanTimer(linkIdentifier_);}
  inline void update(Handover1 *a, struct link_identifier_t linkIdentifier) {a_ = a; linkIdentifier_ = linkIdentifier;}
  protected:
  Handover1 *a_;
  void *e_; //can be any type of event (link down, link going down...)
 private:
  struct link_identifier_t linkIdentifier_;
};


#endif
