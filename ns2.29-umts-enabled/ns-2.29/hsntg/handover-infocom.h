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

class PrefixHandleTimer;
/*
 * Defines the Handover module
 */
class InfocomHandover : public Handover {

 public:
  InfocomHandover();

  void process_link_detected (link_detected_t *);
  void process_link_up (link_up_t *);
  void process_link_down (link_down_t *);
  void process_link_going_down (link_going_down_t *);

  void process_new_prefix (new_prefix*);
  void process_new_prefix2 (new_prefix*);
  void register_mih ();

  //Infocom
  void process_link_parameter_change (link_parameter_change_t *);

 protected:
  Mac *connectingMac_; //store the mac that is currently connecting.
  int compute_new_address(int, Node*);
  PrefixHandleTimer *prefix_timer_;
};

/* 
 * Mac list timer
 * Timer to register events to the MAC
 */
class PrefixHandleTimer : public TimerHandler {
 public:
        PrefixHandleTimer(InfocomHandover *a) : TimerHandler() 
	  { a_ = a; }
	inline void new_data (new_prefix *data) { data_ = data; }
 protected:
        inline void expire(Event *) {
	  if (data_) {a_->process_new_prefix2(data_);}
	}
        InfocomHandover *a_;
	new_prefix *data_;
};




#endif
