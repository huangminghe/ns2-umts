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

#ifndef handover_simple_ap_h
#define handover_simple_ap_h

#include "handover.h"
#include "simulator.h"

/*
 * Defines the Handover module
 */
class SimpleHandoverAP : public Handover {

 public:
  SimpleHandoverAP();

  void process_link_detected (link_detected_t *);
  void process_link_up (link_up_t *);
  void process_link_down (link_down_t *);
  void process_link_going_down (link_going_down_t *);
  void process_link_rollback (link_rollback_t *e);
  void process_new_prefix (new_prefix*);
  void register_mih ();

  /**
   * Process MIH_Register.indication
   * @param mihh The MIH packet
   */
  void process_register_ind (hdr_mih *mihh);

  /**
   * Process MIH_Deregister.indication
   * @param mihh The MIH packet
   */
  void process_deregister_ind (hdr_mih *mihh);

  /**
   * Process MIH_Get_Status.confirm
   * @param mihh The MIH packet
   */
  void process_get_status_conf (hdr_mih *mihh);

  /**
   * Process MIH_Event_subscribe.confirm
   * @param rsp The result of subscription request
   */
  void process_event_sub_conf (struct mih_event_sub_rsp *rsp);

  /**
   * Process MIH_Event_unsubscribe.confirm
   * @param rsp The result of unsubscription request
   */
  void process_event_unsub_conf (struct mih_event_unsub_rsp *rsp);

 private:
  bool scan_;
};


#endif
