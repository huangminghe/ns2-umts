/*
 * Implementation of Simple Handover module for MIPv6
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

#include "handover-simpleAP.h"
#include "mip6.h"
#include "mac-802_11.h"

static class SimpleHandoverAPClass : public TclClass {
public:
  SimpleHandoverAPClass() : TclClass("Agent/MIHUser/IFMNGMT/MIPV6/Handover/SimpleAP") {}
  TclObject* create(int, const char*const*) {
    return (new SimpleHandoverAP());
  }
} class_simplehandoverap;


/**
 * Creates a handover module
 */
SimpleHandoverAP::SimpleHandoverAP() : Handover()
{
  scan_ = true;
}

void SimpleHandoverAP::register_mih ()
{
  mih_get_status_s *status;

  //I want to be informed of System Management indications
  mih_->register_management_user (this);

  debug ("Calling MIH_Get_Status\n");
  
  status = mih_->mih_get_status (this, mih_->getID(), 0x4); //request linkID
  
  debug ("MIH_Get_Status results for %d interface(s):\n", status->nbInterface);
  for (int i=0; i < status->nbInterface ; i++) {
    debug ("\tLink ID: linkType=%d, macMobileTerminal=%d, macPoA=%d\n", status->status[i].linkId.type, status->status[i].linkId.macMobileTerminal, status->status[i].linkId.macPoA);
    //execute MIH_Capability_Discovery
    mih_cap_disc_conf_s * cap = mih_->mih_capability_discover (this, mih_->getID(), &(status->status[i].linkId));
    debug ("\tCapabilities are: commands:%x, events:%x\n", cap->capability.commands, cap->capability.events);
    //register events
    //Link Detected, UP, Down, Going Down, and rollback
    link_event_status * sub_status = mih_->mih_event_subscribe (this, mih_->getID(), status->status[i].linkId, 0x2F & cap->capability.events);
    assert (sub_status!=NULL); //since we register to local MAC we get instant response
    debug ("\tSubscription status: requested=%x, result=%x\n", 0x2F & cap->capability.events, *sub_status);
  }
  free (status);
}

/**
 * Process the link detected event
 * @param e The event information
 */
void SimpleHandoverAP::process_link_detected (link_detected_t *e)
{
  debug ("At %f in %s Handover AP received link detected \n\ttype %d, MacAddr=%d, PoA=%d, MIHCapable=%d\n", \
	 NOW, Address::instance().print_nodeaddr(this->addr()), \
	 e->linkIdentifier.type, e->linkIdentifier.macMobileTerminal,e->linkIdentifier.macPoA, e->mihCapability);

  if (e->linkIdentifier.type == LINK_802_11 || e->linkIdentifier.type == LINK_802_16) {
    debug ("\tI launch the connection on the link\n");
    mih_configure_req_s * req = (mih_configure_req_s *) malloc (sizeof (mih_configure_req_s));
    req->information = 1 << CONFIG_LINK_ID;
    req->linkIdentifier = e->linkIdentifier;
    mih_->mih_configure_link (this, mih_->getID(), req);
  }

  free (e);
}

/**
 * Process the link up event
 * @param e The event information
 */
void SimpleHandoverAP::process_link_up (link_up_t *e)
{
  debug ("At %f in %s Handover AP link up \n\ttype %d, MacAddr=%d, MacPoA=%d\n", \
	 NOW, Address::instance().print_nodeaddr(this->addr()),\
	 e->linkIdentifier.type, e->linkIdentifier.macMobileTerminal, e->linkIdentifier.macPoA);

  /* 
  if (e->linkIdentifier.type == LINK_802_11 || e->linkIdentifier.type == LINK_802_16) {
    //we need to check prefix first.
    ((MIPV6Agent*)this)->send_rs (this->get_mih()->get_mac(e->linkIdentifier.macMobileTerminal));
  }
  */

  free (e);
}

/**
 * Process the link down event
 * @param e The event information
 */
void SimpleHandoverAP::process_link_down (link_down_t *e)
{
  debug ("At %f in %s Handover AP received link down \n\ttype %d, MacAddr=%d, MacPoA=%d\n", \
	 NOW, Address::instance().print_nodeaddr(this->addr()),\
	 e->linkIdentifier.type, e->linkIdentifier.macMobileTerminal, e->linkIdentifier.macPoA);

  free (e);
}

/**
 * Process the link going down event
 * @param e The event information
 */
void SimpleHandoverAP::process_link_going_down (link_going_down_t *e)
{
  debug ("At %f in %s Handover AP received link going down \n\ttype %d, MacAddr=%d, MacPoA=%d\n", \
	 NOW, Address::instance().print_nodeaddr(this->addr()),\
	 e->linkIdentifier.type, e->linkIdentifier.macMobileTerminal, e->linkIdentifier.macPoA);

  if (e->linkIdentifier.type == LINK_802_11 & scan_) {
    struct mih_scan_request_s *req = (struct mih_scan_request_s *) malloc (sizeof (struct mih_scan_request_s)); 
    struct mih_scan_req_entry_s *entry = (struct mih_scan_req_entry_s *) malloc (sizeof (struct mih_scan_req_entry_s)); 
    req->nbElement = 1;
    req->requestSet= entry;
    req->requestSet[0].linkIdentifier = e->linkIdentifier;
    struct Mac802_11_scan_request_s *command = (struct Mac802_11_scan_request_s*) malloc (sizeof (struct Mac802_11_scan_request_s));
    command->scanType = SCAN_TYPE_ACTIVE;
    command->action = SCAN_ALL;
    command->nb_channels = 10;
    command->pref_ch = (int*) malloc (10*sizeof (int));
    for (int i=0 ; i <10 ; i++) {
      command->pref_ch[i] = i+1;
    }
    command->minChannelTime = 0.020;
    command->maxChannelTime = 0.060;
    command->probeDelay = 0.002;
    req->requestSet[0].commandSize = sizeof (struct Mac802_11_scan_request_s);
    req->requestSet[0].command = command;
    //here we hard code the remote mihf but normal we should have memorized it from the registration/capability exchange.
    mih_->mih_scan (this, 1, req);
    scan_ = false;
  }
  free (e);
}

/**
 * Process the link rollback event
 * @param e The event information
 */
void SimpleHandoverAP::process_link_rollback (link_rollback_t *e)
{
  debug ("At %f in %s Handover AP received link rollback \n\ttype %d, MacAddr=%d, MacPoA=%d\n", \
	 NOW, Address::instance().print_nodeaddr(this->addr()),\
	 e->linkIdentifier.type, e->linkIdentifier.macMobileTerminal, e->linkIdentifier.macPoA);

  //to be defined by subclass
  free (e);
}


/**
 * Process a new prefix entry
 * @param data The new prefix information 
 */
void SimpleHandoverAP::process_new_prefix (new_prefix* data)
{
  vector <FlowEntry*> flows;
  vector <FlowEntry*> flows_to_redirect;
  Mac *mac;
  Tcl& tcl = Tcl::instance();

  //Now we got the address of new PoA
  //get the MAC for which we received a new prefix.
  tcl.evalf ("%s set mac_(0)",data->interface->name());
  mac = (Mac*) TclObject::lookup(tcl.result());

  //check if that is a better interface and redirect flows.
  //We can use the interface, let's switch the flows to the new interface
  //Here we suppose that we only connect to the best interface
  
  char *str1 = Address::instance().print_nodeaddr(this->addr());
  char *str2 = Address::instance().print_nodeaddr(data->prefix);
  debug ("At %f in %s SimpleHandover AP received new prefix %s\n", NOW, str1, str2);
  delete []str1;
  delete []str2;

  compute_new_address (data->prefix, data->interface);

  //send capability request on new interface
  mih_->send_cap_disc_req (this, mac);

  free (data);
}

/**
 * Process MIH_Register.indication
 * @param ind The indication that someone is trying to register
 */
void SimpleHandoverAP::process_register_ind (hdr_mih *mihh)
{
  assert (mihh!=NULL);

  char *str1 = Address::instance().print_nodeaddr(this->addr());
  debug ("At %f in %s Simple Handover AP received MIH_Register.indication..accept\n", NOW, str1);
  delete []str1;
  //send response
  mih_->send_reg_response (this, mihh->hdr.mihf_id, 1, 0, 20, mihh->hdr.transaction_id);


  //now we know there is a remote node, let's see what they have
  mih_->mih_get_status (this, mihh->hdr.mihf_id, 0x4);
}

/**
 * Process MIH_Deregister.indication
 * @param ind The indication that someone is trying to deregister
 */
void SimpleHandoverAP::process_deregister_ind (hdr_mih *mihh)
{
  assert (mihh!=NULL);

  char *str1 = Address::instance().print_nodeaddr(this->addr());
  debug ("At %f in %s Simple Handover AP received MIH_Deregister.indication..accept\n", NOW, str1);
  delete []str1;
  //send response
  mih_->send_dereg_response (this, mihh->hdr.mihf_id, 1, 0, mihh->hdr.transaction_id);

}

/**
 * Process MIH_Get_Status.confirm
 * @param rsp The result to status request
 */
void SimpleHandoverAP::process_get_status_conf (hdr_mih *mihh)
{
  struct mih_get_status_rsp *rsp = &(mihh->payload.mih_get_status_rsp_);
  link_identifier_t linkId;

  char *str1 = Address::instance().print_nodeaddr(this->addr());
  debug ("At %f in %s Simple Handover AP received MIH_Get_Status.confirm\n", NOW, str1);
  delete []str1;

  //retrieve mih from the session id
  u_int32_t mihfId = mih_->getMihfId (mihh->hdr.session_id);

  for (int i=0; i < rsp->nbInterface ; i++) {
    debug ("\tLink ID: linkType=%d, macMobileTerminal=%d, macPoA=%d\n", rsp->status[i].linkType, mih_->eth_to_int(rsp->status[i].macMobileTerminal), mih_->eth_to_int (rsp->status[i].macPoA));
    linkId.type = (link_type_t)rsp->status[i].linkType;
    linkId.macMobileTerminal = mih_->eth_to_int(rsp->status[i].macMobileTerminal);
    linkId.macPoA = mih_->eth_to_int (rsp->status[i].macPoA);
    /*
    //execute MIH_Capability_Discovery
    mih_cap_disc_conf_s * cap = mih_->mih_capability_discover (this, mih_->getID(), &(status->status[i].linkId));
    debug ("\tCapabilities are: commands:%x, events:%x\n", cap->capability.commands, cap->capability.events);
    */
    //register events Link Going Down 
    mih_->mih_event_subscribe (this, mihfId, linkId, 0x1<<MIH_LINK_GOING_DOWN);

    //to test message exchange
    //mih_->mih_event_unsubscribe (this, mihfId, linkId, 0x1<<MIH_LINK_GOING_DOWN);
  }
}

/**
 * Process MIH_Event_subscribe.confirm
 * @param rsp The result of subscription request
 */
void SimpleHandoverAP::process_event_sub_conf (struct mih_event_sub_rsp *rsp)
{
  char *str1 = Address::instance().print_nodeaddr(this->addr());
  debug ("At %f in %s Simple Handover AP received MIH_Event_Subscribe.confirm\n", NOW, str1);
  debug ("\tstatus: %x\n", rsp->res_eventList);
  delete []str1;
}

/**
 * Process MIH_Event_unsubscribe.confirm
 * @param rsp The result of unsubscription request
 */
void SimpleHandoverAP::process_event_unsub_conf (struct mih_event_unsub_rsp *rsp)
{
  char *str1 = Address::instance().print_nodeaddr(this->addr());
  debug ("At %f in %s Simple Handover AP received MIH_Event_Unsubscribe.confirm\n", NOW, str1);
  debug ("\tstatus: %x\n", rsp->res_eventList);
  delete []str1;

}
