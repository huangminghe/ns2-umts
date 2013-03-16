/*
 * include file for MIH User abstract class
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
 * rouil - 2006/11/09 - 1.0     - Source created<BR>
 * <BR>
 * <BR>
 * </PRE><P>
 */ 

#ifndef mih_user_h
#define mih_user_h

#include "agent.h"
#include "tclcl.h"
#include "packet.h"
#include "address.h"
#include "ip.h"
#include "node.h"
#include "random.h"

#include "mih.h"

/* 
 * Interface management agent
 */
class MIHUserAgent : public Agent {
 public:
  /**
   * Create an MIH user
   */
  MIHUserAgent();

  /* 
   * Interface with TCL interpreter
   * @param argc The number of elements in argv
   * @param argv The list of arguments
   * @return TCL_OK if everything went well else TCL_ERROR
   */
  int command(int argc, const char*const* argv);

  /**
   * Process an incoming packet
   * @param p the packet received
   * @param h packet handler
   */
  virtual void recv(Packet*, Handler*);

  /**
   * process MIH events
   * @param type the mih event type 
   * @param data The event
   */
  void process_mih_event (mih_event_t type, void* data); 
  
  /**
   * Return the MIH function
   * @return the MIH function
   */
  MIHAgent* get_mih ();

  /**
   * Process MIH_Capability_Discover.confirm
   * @param mihh The MIH packet containing the response
   */
  virtual void process_cap_disc_conf (hdr_mih *mihh);

  /**
   * Process MIH_Register.indication
   * @param mihh MIH packet
   */
  virtual void process_register_ind (hdr_mih *mihh);

  /**
   * Process MIH_Register.confirm
   * @param rsp the result of the registration
   */
  virtual void process_register_conf (struct mih_reg_rsp *rsp);

  /**
   * Process MIH_DeRegister.indication
   * @param miih The MIH packet containing the deregistration request
   */
  virtual void process_deregister_ind (hdr_mih *mihh);

  /**
   * Process MIH_DeRegister.confirm
   * @param rsp the result of the registration
   */
  virtual void process_deregister_conf (struct mih_dereg_rsp *rsp);

  /**
   * Process MIH_Event_subscribe.confirm
   * @param rsp The result of subscription request
   */
  virtual void process_event_sub_conf (struct mih_event_sub_rsp *rsp);

  /**
   * Process MIH_Event_unsubscribe.confirm
   * @param rsp The result of unsubscription request
   */
  virtual void process_event_unsub_conf (struct mih_event_unsub_rsp *rsp);

  /**
   * Process MIH_Get_Status.confirm
   * @param mihh MIH packet
   */
  virtual void process_get_status_conf (hdr_mih *mihh);

  /**
   * Process MIH_Switch.confirm
   * @param rsp The result to switch request
   */
  virtual void process_switch_conf (struct mih_switch_rsp *rsp);

  /**
   * Process MIH_Configure.confirm
   * @param rsp The result to configure request
   */
  virtual void process_configure_conf (struct mih_configure_rsp *rsp);

  /**
   * Process MIH_Scan.confirm
   * @param rsp The result to scan request
   */
  virtual void process_scan_conf (struct mih_scan_rsp *rsp);

  /**
   * Process MIH_Handover_Initiate.indication
   * @param ind Indication of handover initiate from remote node
   */
  virtual void process_handover_init_ind (struct mih_handover_initiate_req *ind);

  /**
   * Process MIH_Handover_Initiate.confirm
   * @param rsp The result of handover initiate request
   */
  virtual void process_handover_init_conf (struct mih_handover_initiate_res *rsp);

  /**
   * Process MIH_Handover_Prepare.indication
   * @param ind Indication of handover prepare from remote node
   */
  virtual void process_handover_prepare_ind (struct mih_handover_prepare_req *ind);

  /**
   * Process MIH_Handover_Prepare.confirm
   * @param rsp The result of handover prepare request
  */
  virtual void process_handover_prepare_conf (struct mih_handover_prepare_res *rsp);

  /**
   * Process MIH_Handover_Commit.indication
   * @param ind Indication of handover commit from remote node
   */
  virtual void process_handover_commit_ind (struct mih_handover_commit_req *ind);

  /**
   * Process MIH_Handover_Commit.confirm
   * @param rsp The result of handover commit request
   */
  virtual void process_handover_commit_conf (struct mih_handover_commit_res *rsp);

  /**
   * Process MIH_Handover_Complete.indication
   * @param ind Indication of handover complete from remote node
   */
  virtual void process_handover_complete_ind (struct mih_handover_complete_req *ind);

  /**
   * Process MIH_Handover_Complete.confirm
   * @param rsp The result of handover complete request
   */
  virtual void process_handover_complete_conf (struct mih_handover_complete_res *rsp);

  /**
   * Process MIH_Network_Address_Information.indication
   * @param ind Indication of network address information request
   */
  virtual void process_network_info_ind (struct mih_network_address_info_req *ind);

  /**
   * Process MIH_Network_Address_Information.confirm
   * @param rsp The result of network address information request
   */
  virtual void process_network_info_conf (struct mih_network_address_info_res *rsp);

  /**
   * Process MIH_Get_Information.indication
   * @param ind Indication of information request 
   */
  virtual void process_get_info_ind (struct mih_get_information_req *ind);

  /**
   * Process MIH_Get_Information.confirm
   * @param rsp The result of information request
   */
  virtual void process_get_info_conf (struct mih_get_information_rsp *rsp);

 protected:
  /**
   * Return the MIH agent it is connected to
   * @return The MIH agent it is connected to
   */
  MIHAgent *mih_;              //its associated MIH agent

  /**
   * Process MIH_LINK_UP
   * @param e The Link UP event
   */
  virtual void process_link_up (struct link_up_t *e);

  /**
   * Process MIH_LINK_DOWN
   * @param e The Link DOWN event
   */
  virtual void process_link_down (struct link_down_t *e);

  /**
   * Process MIH_LINK_GOING_DOWN
   * @param e The Link Going Down event
   */
  virtual void process_link_going_down (struct link_going_down_t *e);

  /**
   * Process MIH_LINK_DETECTED
   * @param e The Link Detected event
   */
  virtual void process_link_detected (struct link_detected_t *e);

  /**
   * Process MIH_LINK_PARAMETERS_REPORT
   * @param e The Link Parameters report event
   */
  virtual void process_link_parameters_report (struct link_parameters_report_t *e);

  /**
   * Process MIH_LINK_ROLLBACK
   * @param e The Link Rollback event
   */
  virtual void process_link_rollback (struct link_rollback_t *e);

  /**
   * Process MIH_LINK_HANDOVER_IMMINENT
   * @param e The Link Handover imminent event
   */
  virtual void process_link_handover_imminent (struct link_handover_imminent_t *e);

  /**
   * Process MIH_LINK_HANDOVER_COMPLETE
   * @param e The Link Handover complete event
   */
  virtual void process_link_handover_complete (struct link_handover_complete_t *e);


};

#endif
