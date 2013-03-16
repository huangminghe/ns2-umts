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

#ifndef handover_simple_h
#define handover_simple_h

#include "handover.h"
#include "simulator.h"

/*
 * Defines the Handover module
 */
class SimpleHandover : public Handover {

 public:
  SimpleHandover();

  void process_link_detected (link_detected_t *);
  void process_link_up (link_up_t *);
  void process_link_down (link_down_t *);
  void process_link_going_down (link_going_down_t *);
  void process_link_rollback (link_rollback_t *e);
  void process_new_prefix (new_prefix*);
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

};


#endif
