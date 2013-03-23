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

#ifndef handover21_3_h
#define handover21_3_h

#include "handover.h"

/*
 * Defines the Handover module
 */
class Handover3 : public Handover {
public:
  Handover3();

  void process_link_detected (link_detected_t *);
  void process_link_up (link_up_t *);
  void process_link_down (link_down_t *);
  void process_link_going_down (link_going_down_t *);
  void process_scan_response (mih_scan_response_t *);
  void process_no_link_detected(void *);

  void process_new_prefix (new_prefix*);
  void process_exp_prefix (exp_prefix*);
  void register_mih ();

  int blacklist_[10];

 protected:
  int case_;  //scenario case (1, 2 or 3)
  Mac *connectingMac_; //store the mac that is currently connecting.
  int compute_new_address(int, Node*);
 };

#endif
