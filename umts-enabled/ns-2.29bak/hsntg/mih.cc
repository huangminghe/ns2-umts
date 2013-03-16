/*
 * Implementation of Media Independant Handover Function
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
 * rouil - 2005/05/18 - 1.0     - Source created<BR>
 * <BR>
 * <BR>
 * </PRE><P>
 */

#include "mih.h"
#include "mih-iface.h"
#include "mac.h"
#include "node.h"
#include "mih-user.h"
#include "mih-scan.h"
#include "mac-802_11.h"
#include "mac802_16.h"

#define MYNUM	Address::instance().print_nodeaddr(addr())

/**
 * Packet definitions
 */
int hdr_mih::offset_;
static class MIHHeaderClass : public PacketHeaderClass {
public:
	MIHHeaderClass() : PacketHeaderClass("PacketHeader/MIH",
					     sizeof(hdr_mih)) {
	    bind_offset(&hdr_mih::offset_);
        }
} class_mihhdr;


/**
 * Static constructor for TCL
 */
static class MIHAgentClass : public TclClass {
public:
	MIHAgentClass() : TclClass("Agent/MIH") {}
	TclObject* create(int, const char*const*) {
		return (new MIHAgent());
	}
} class_ndagent;

/**
 * Timer handler for pending request
 * @param e The event information
 */
void MIHRequestTimer::expire (Event *e)
{
  a_->process_request_timer (tid_);
}

/**
 * Create the MIH MIB
 * @param parent The MIHAgent 
 */
MIH_MIB::MIH_MIB(MIHAgent *parent)
{
  parent->bind("supported_events_", &supported_events_);
  parent->bind("supported_commands_", &supported_commands_);
  parent->bind("supported_transport_", &supported_transport_);
  parent->bind("supported_query_type_", &supported_query_type_);

  parent->bind("es_timeout_", &es_timeout_);
  parent->bind("cs_timeout_", &cs_timeout_);
  parent->bind("is_timeout_", &is_timeout_);

  parent->bind("retryLimit_", &retryLimit_);
}

static int MIHIndex = 0;

/**
 * Creates a neighbor discovery agent
 * Initializes the agent and bind variable to be accessible in TCL
 */
MIHAgent::MIHAgent() : Agent(PT_UDP), mihmib_(this), transactionId_(0)
{
  mihfId_ = MIHIndex++;
  //bind tcl attributes
  bind("default_port_", &default_port_);
}

/**
 * Interface with TCL interpreter
 * @param argc The number of elements in argv
 * @param argv The list of arguments
 * @return TCL_OK if everything went well else TCL_ERROR
 */
int MIHAgent::command(int argc, const char*const* argv)
{
  if (argc == 2) {
    if (strcmp(argv[1], "send_cap_req") == 0) {
      send_cap_disc_req(NULL,ifaces_.at(0)->getMac());
      return TCL_OK;
    }
  }
  
  else if (argc == 3) {
    //add a mac interface
    if (strcmp(argv[1], "add-mac")==0) {
      Mac* mac = (Mac*) TclObject::lookup(argv[2]);
      if (mac){
	ifaces_.push_back (new MIHInterfaceInfo (mac));
	debug ("At %f in %s MIH is adding mac %d\n", NOW, MYNUM, mac->addr());
	return TCL_OK;
      }
      return TCL_ERROR;
    }
  }
 
  return (Agent::command(argc, argv));
}

/** 
 * Process received packet from a remote MIH agent.
 * @param p The packet received
 * @param h The handler that sent the packet
 */
void MIHAgent::recv(Packet* p, Handler *h)
{
  //hdr_cmn *hdrc = HDR_CMN(p);
  hdr_mih *mh = HDR_MIH(p);

  switch (mh->hdr.mih_aid) {
  case MIH_MSG_CAPABILITY_DISCOVER:
    if (mh->hdr.mih_opcode == MIH_REQUEST)
      recv_cap_disc_req ( p);
    else if (mh->hdr.mih_opcode == MIH_RESPONSE)
      recv_cap_disc_rsp ( p);
    else {
      fprintf (stderr, "recv in MIHAgent: Invalid opcode for MIH_MSG_CAPABILITY_DISCOVER\n");
      exit (1);
    }
    break;
  case MIH_MSG_REGISTRATION:
    if (mh->hdr.mih_opcode == MIH_REQUEST)
      recv_reg_req ( p);
    else if (mh->hdr.mih_opcode == MIH_RESPONSE)
      recv_reg_rsp ( p);
    else {
      fprintf (stderr, "recv in MIHAgent: Invalid opcode for MIH_MSG_REGISTRATION\n");
      exit (1);
    }
    break;
  case MIH_MSG_DEREGISTRATION:
    if (mh->hdr.mih_opcode == MIH_REQUEST)
      recv_dereg_req ( p);
    else if (mh->hdr.mih_opcode == MIH_RESPONSE)
      recv_dereg_rsp ( p);
    else {
      fprintf (stderr, "recv in MIHAgent: Invalid opcode for MIH_MSG_DEREGISTRATION\n");
      exit (1);
    }
    break;
  case MIH_MSG_EVENT_SUBSCRIBE:
    if (mh->hdr.mih_opcode == MIH_REQUEST)
      recv_event_sub_req ( p);
    else if (mh->hdr.mih_opcode == MIH_RESPONSE)
      recv_event_sub_rsp ( p);
    else {
      fprintf (stderr, "recv in MIHAgent: Invalid opcode for MIH_MSG_EVENT_SUBSCRIBE\n");
      exit (1);
    }
    break;
  case MIH_MSG_EVENT_UNSUBSCRIBE:
    if (mh->hdr.mih_opcode == MIH_REQUEST)
      recv_event_unsub_req ( p);
    else if (mh->hdr.mih_opcode == MIH_RESPONSE)
      recv_event_unsub_rsp ( p);
    else {
      fprintf (stderr, "recv in MIHAgent: Invalid opcode for MIH_MSG_EVENT_UNSUBSCRIBE\n");
      exit (1);
    }
    break;
  case MIH_MSG_LINK_UP:
    assert (mh->hdr.mih_service_id==MIH_EVENT_SERVICE);
    if (mh->hdr.mih_opcode == MIH_INDICATION)
      recv_link_up_ind ( p);
    else {
      fprintf (stderr, "recv in MIHAgent: Invalid opcode for MIH_MSG_LINK_UP\n");
      exit (1);
    }
    break;  
  case MIH_MSG_LINK_DOWN:
    assert (mh->hdr.mih_service_id==MIH_EVENT_SERVICE);
    if (mh->hdr.mih_opcode == MIH_INDICATION)
      recv_link_down_ind ( p);
    else {
      fprintf (stderr, "recv in MIHAgent: Invalid opcode for MIH_MSG_LINK_DOWN\n");
      exit (1);
    }
    break;  
  case MIH_MSG_LINK_GOING_DOWN:
    assert (mh->hdr.mih_service_id==MIH_EVENT_SERVICE);
    if (mh->hdr.mih_opcode == MIH_INDICATION)
      recv_link_going_down_ind ( p);
    else {
      fprintf (stderr, "recv in MIHAgent: Invalid opcode for MIH_MSG_LINK_GOING_DOWN\n");
      exit (1);
    }
    break;  
  case MIH_MSG_LINK_EVENT_ROLLBACK:
    assert (mh->hdr.mih_service_id==MIH_EVENT_SERVICE);
    if (mh->hdr.mih_opcode == MIH_INDICATION)
      recv_link_rollback_ind ( p);
    else {
      fprintf (stderr, "recv in MIHAgent: Invalid opcode for MIH_MSG_LINK_ROLLBACK\n");
      exit (1);
    }
    break;  
  case MIH_MSG_HANDOVER_INITIATE:
    assert (mh->hdr.mih_service_id==MIH_COMMAND_SERVICE);
    if (mh->hdr.mih_opcode == MIH_REQUEST)
      recv_handover_initiate_req ( p);
    else if (mh->hdr.mih_opcode == MIH_RESPONSE)
      recv_handover_initiate_res ( p);
    else {
      fprintf (stderr, "recv in MIHAgent: Invalid opcode for MIH_MSG_HANDOVER_INITIATE\n");
      exit (1);
    }
    break;
  case MIH_MSG_NETWORK_ADDRESS_INFORMATION:
    assert (mh->hdr.mih_service_id==MIH_COMMAND_SERVICE);
    if (mh->hdr.mih_opcode == MIH_REQUEST)
      recv_network_address_info_req ( p);
    else if (mh->hdr.mih_opcode == MIH_RESPONSE)
      recv_network_address_info_res ( p);
    else {
      fprintf (stderr, "recv in MIHAgent: Invalid opcode for MIH_MSG_NETWORK_ADDRESS_INFORMATION\n");
      exit (1);
    }
    break;     
  case MIH_MSG_GET_STATUS:
    assert (mh->hdr.mih_service_id==MIH_COMMAND_SERVICE);
    if (mh->hdr.mih_opcode == MIH_REQUEST)
      recv_mih_get_status_req ( p);
    else if (mh->hdr.mih_opcode == MIH_RESPONSE)
      recv_mih_get_status_res ( p);
    else {
      fprintf (stderr, "recv in MIHAgent: Invalid opcode for MIH_MSG_GET_STATUS\n");
      exit (1);
    }
    break;   
  case MIH_MSG_SCAN:
    assert (mh->hdr.mih_service_id==MIH_COMMAND_SERVICE);
    if (mh->hdr.mih_opcode == MIH_REQUEST)
      recv_mih_scan_req ( p);
    else if (mh->hdr.mih_opcode == MIH_RESPONSE)
      recv_mih_scan_res ( p);
    else {
      fprintf (stderr, "recv in MIHAgent: Invalid opcode for MIH_MSG_SCAN\n");
      exit (1);
    }
    break;       
    
  case MIH_MSG_SWITCH:
    assert (mh->hdr.mih_service_id==MIH_COMMAND_SERVICE);
    if (mh->hdr.mih_opcode == MIH_REQUEST)
      recv_mih_switch_req ( p);
    else if (mh->hdr.mih_opcode == MIH_RESPONSE)
      recv_mih_switch_res ( p);
    else {
      fprintf (stderr, "recv in MIHAgent: Invalid opcode for MIH_MSG_SCAN\n");
      exit (1);
    }
    break;

  default:
    fprintf (stderr, "recv in MIHAgent: Invalid/Unknown message ID\n");
    exit (1);
  }
  Packet::free (p);
}

/** 
 * Process received packet
 * @param e The type of event received
 * @param data The data associated with the event
 */
void MIHAgent::recv_event(link_event_t e, void * data)
{
  assert (data);

  switch (e) {
  case MIH_LINK_UP:
    process_link_up ((link_up_t *)data);
    break;
  case MIH_LINK_DOWN:
    process_link_down ((link_down_t*)data);
    break;        
  case MIH_LINK_GOING_DOWN:
    process_link_going_down ((link_going_down_t*)data);
    break;
  case MIH_LINK_DETECTED:
    process_link_detected ((link_detected_t*)data);
    break;
  case MIH_LINK_PARAMETERS_REPORT:
    process_link_parameters_report ((link_parameters_report_t*)data);
    break;
  case MIH_LINK_EVENT_ROLLBACK:
    process_link_rollback ((link_rollback_t*)data);
    break;
  case MIH_LINK_HANDOVER_IMMINENT:
    process_link_handover_imminent ((link_handover_imminent_t*)data);
    break;
  case MIH_LINK_HANDOVER_COMPLETE:
    process_link_handover_complete ((link_handover_complete_t*)data);
    break;
    /*
  case MIH_SCAN_RESPONSE:
    process_scan_response ((mih_scan_response_t*)data);
    break;
    */
  default:
    debug ("At %f in %s MIH Agent received unknown trigger of type %d\n", \
	   NOW, MYNUM, e) ;
  }

  free (data);
}

/**
 * Return the ID of this MIHF 
 * @return the MIHF ID
 */
u_int32_t MIHAgent::getID ()
{
  return mihfId_;
}


/** 
 * Find the interface information for the given MAC
 * @param macAddr The MAC address used by the interface
 */
MIHInterfaceInfo* MIHAgent::get_interface (int macAddr)
{
  for(int i=0;i < (int)ifaces_.size(); i++)
    {
      if ( ifaces_.at(i)->getMacAddr()==macAddr)
	{
	  return ifaces_.at(i);
	}
    }
  return NULL;
}

/**
 * Return an array containing the interfaces information
 * @return An array containing the interfaces information
 */
MIHInterfaceInfo** MIHAgent::get_interfaces ()
{
  MIHInterfaceInfo** res = 
    (MIHInterfaceInfo**) malloc ((int)ifaces_.size()*sizeof (MIHInterfaceInfo*));
  for(int i=0;i < (int)ifaces_.size(); i++){
    res[i] = ifaces_.at(i);
  }
  return res;
}

/**
 * Check if the given Mac is local
 * @param macAddr The MAC address
 * @return true of the Mac is local, otherwise return false
 */
bool MIHAgent::is_local_mac (int macAddr)
{
  MIHInterfaceInfo* info = get_interface (macAddr);
  if (info == NULL)
    return false;
  else
    return (info->isLocal());
}

/**
 * Return pointer to the given MAC
 * @param macAddr The MAC address
 * @return Mac if local, otherwise return NULL
 */
Mac* MIHAgent::get_mac (int macAddr)
{
  return (get_interface (macAddr)->getMac());
}

/**
 * Return the list of local Macs
 * @return The list of local Macs
 */
vector <Mac *> MIHAgent::get_local_mac ()
{
  vector <Mac *> macs;
  for (int i = 0 ; i < (int)ifaces_.size() ; i++)
    if (ifaces_.at(i)->isLocal()) {
      macs.push_back (ifaces_.at(i)->getMac());
    }
  return macs;
}


/**
 * Return the MAC for the node with the given address
 * @return The MAC for the node with the given address
 */
Mac * MIHAgent::get_mac_for_node (int addr)
{
  Mac *mac;
  Node *node;
  Tcl& tcl = Tcl::instance();
  for (int i = 0 ; i < (int)ifaces_.size() ; i++) {
    if (ifaces_.at(i)->isLocal()) {
      mac = ifaces_.at(i)->getMac();
      tcl.evalf ("%s get-node", mac->name()); 
      node = (Node*) TclObject::lookup(tcl.result());
      //printf ("Checking %s\n",Address::instance().print_nodeaddr(node->address()));
      if (node->address()==addr)
	return mac;
    }  
  }
  return NULL;
}

/**
 * Return the MIHF information for the given MIHF ID
 * @param mihfid The MIHF ID
 * @return the MIHF information or NULL
 */
mihf_info * MIHAgent::get_mihf_info (int mihfid)
{
  for (int i=0 ; i < (int) peer_mihfs_.size() ; i++) {
    if (peer_mihfs_.at(i)->id == mihfid)
      return peer_mihfs_.at(i);
  }
  return NULL;
}

/**
 * Return the session for the given session ID
 * @param sessionid The session ID
 * @return the session information or NULL
 */
session_info * MIHAgent::get_session (int sessionid)
{
  for (int i=0 ; i < (int) sessions_.size() ; i++) {
    if (sessions_.at(i)->id == sessionid)
      return sessions_.at(i);
  }
  return NULL;
}

/**
 * Delete the session and clean all registrations
 * @param sessionid The session ID
 */
void MIHAgent::delete_session (int sessionid) {
  for (int i=0 ; i < (int) sessions_.size() ; i++) {
    if (sessions_.at(i)->id == sessionid) {
      //delete registrations
      for(int j=0;j < (int)ifaces_.size(); j++){
	ifaces_.at(j)->remote_event_unsubscribe (sessions_.at(i), 0xFFFFFFFF);
      }
      sessions_.erase (sessions_.begin()+i);
      break;
    }
  }
}


/**
 * Return the MIHF ID for the given session
 * @param sessionid The session ID
 * @return the MIHF ID for the session
 */
u_int32_t MIHAgent::getMihfId (u_int32_t sessionid)
{
  return get_session (sessionid)->mihf->id;
}


/**
 * Convert the MAC address from int to 48bits 
 * @param the MAC address as int
 * @param array to store converted value
 */
void MIHAgent::int_to_eth (int macInt, u_char mac[ETHER_ADDR_LEN])
{  
  for (int i = 0 ; i <4; i++)
    mac[i] = (macInt >> 8*i) & 0xFF;
  mac[4]=0;
  mac[5]=0;
}

/**
 * Convert the MAC address from 48 bits to int
 * @param mac The MAC address as 48bits
 * @return the MAC address as int
 */
int MIHAgent::eth_to_int (u_char mac[ETHER_ADDR_LEN])
{
  int result=0;
  for (int i = 0 ; i <4; i++)
    result = result | (mac[i]<< 8*i);
  return result;
}

/**
 * Clear the pending request. 
 * @param req The request to clear
 */
void MIHAgent::clear_request (mih_pending_req *req)
{
  Packet::free (req->pkt);
  if (req->timer->status() == TIMER_PENDING)
    req->timer->cancel();
  delete (req->timer);
  pending_req_.erase (pending_req_.find (req->tid));
  free (req);
}

/**
 * Execute a link scan
 * @param linkId The link identifier
 * @param cmd The scan command
 */
void MIHAgent::link_scan (link_identifier_t linkId, void *cmd)
{
  debug ("At %f in %s MIH Agent sending link scan.\n", NOW, MYNUM);
  //Which end point is local? MN or PoA
  if (is_local_mac (linkId.macMobileTerminal))
    get_mac (linkId.macMobileTerminal)->link_scan (cmd);
  else
    get_mac (linkId.macPoA)->link_scan (cmd); 
}

/***** Event processing *****/

/**
 * Process the link up event
 * Create data structure for new interface and update status
 * @param e The event information
 */
void MIHAgent::process_link_up (link_up_t *e)
{
  MIHInterfaceInfo *info;
  vector <MIHUserAgent *> *local_sub;
  vector <session_info *> *remote_sub;
  int localMac;

  debug ("At %f in %s MIH Agent received LINK UP trigger.\n", NOW, MYNUM);

  //Which end point is local? MN or PoA
  if (is_local_mac (e->linkIdentifier.macMobileTerminal))
    localMac = e->linkIdentifier.macMobileTerminal;
  else
    localMac = e->linkIdentifier.macPoA;

  info = get_interface (localMac);
  assert (info!=NULL);
  info->setStatus (LINK_STATUS_UP);

  //look which interface manager must receive events
  local_sub = info->getRegisteredLocalUser (MIH_LINK_UP);
  for (int i = 0 ; i < (int)local_sub->size() ; i++) {
    //send a copy of the event
    link_up_t *ne = (link_up_t*) malloc (sizeof (link_up_t));
    ne->linkIdentifier.type = e->linkIdentifier.type;
    ne->linkIdentifier.macMobileTerminal = e->linkIdentifier.macMobileTerminal;
    ne->linkIdentifier.macPoA = e->linkIdentifier.macPoA;
    ne->mobilityManagement = e->mobilityManagement;
    ne->macOldAccessRouter = e->macOldAccessRouter;
    ne->macNewAccessRouter = e->macNewAccessRouter;
    ne->ipRenewal = e->ipRenewal;
    local_sub->at(i)->process_mih_event(MIH_LINK_UP,ne);
  }
  
  //Notify remote MIHF
  remote_sub = info->getRegisteredRemoteUser (MIH_LINK_UP);
  for (int i = 0 ; i < (int)remote_sub->size() ; i++) {
    //send message
    Packet *p = allocpkt();
    hdr_mih *mihh = HDR_MIH(p);
    hdr_cmn *hdrc = HDR_CMN(p);
    
    hdrc->ptype() = PT_MIH;
    hdrc->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_link_up_ind); 

    //fill up mih packet information
    mihh->hdr.protocol_version = 1;
    mihh->hdr.mih_service_id = MIH_EVENT_SERVICE;
    mihh->hdr.mih_opcode = MIH_INDICATION;
    mihh->hdr.mih_aid = MIH_MSG_LINK_UP; 
    mihh->hdr.transaction_id = transactionId_++;
    mihh->hdr.session_id = remote_sub->at(i)->id;

    (mihh->payload.mih_link_up_ind_).linkType = e->linkIdentifier.type;
    int_to_eth (e->linkIdentifier.macMobileTerminal, (mihh->payload.mih_link_up_ind_).macMobileTerminal);
    int_to_eth (e->linkIdentifier.macPoA, (mihh->payload.mih_link_up_ind_).macNewPoA);

    debug ("\tSending event to node %s\n",Address::instance().print_nodeaddr(remote_sub->at(i)->mihf->address));

    send_mih_message (NULL, p, remote_sub->at(i));    
  }
  
}

/**
 * Process the link down event
 * A link down should only occurs after a link up (which mean the data must be here)
 * @param e The event information
 */
void MIHAgent::process_link_down (link_down_t *e)
{
  MIHInterfaceInfo *info;
  vector <MIHUserAgent *> *local_sub;
  vector <session_info *> *remote_sub;
  int localMac;
  
  debug ("At %f in %s MIH Agent received LINK DOWN trigger.\n", NOW, MYNUM);

  //Which end point is local? MN or PoA
  if (is_local_mac (e->linkIdentifier.macMobileTerminal))
    localMac = e->linkIdentifier.macMobileTerminal;
  else
    localMac = e->linkIdentifier.macPoA;

  info = get_interface (localMac);
  assert (info!=NULL);
  info->setStatus (LINK_STATUS_DOWN);

  //look which interface manager must receive events
  local_sub = info->getRegisteredLocalUser (MIH_LINK_DOWN);
  for (int i = 0 ; i < (int)local_sub->size() ; i++) {
    link_down_t *ne = (link_down_t*) malloc (sizeof (link_down_t));
    ne->linkIdentifier.type = e->linkIdentifier.type;
    ne->linkIdentifier.macMobileTerminal = e->linkIdentifier.macMobileTerminal;
    ne->linkIdentifier.macPoA = e->linkIdentifier.macPoA;
    ne->macOldAccessRouter = e->macOldAccessRouter;
    ne->reason = e->reason;
    local_sub->at(i)->process_mih_event(MIH_LINK_DOWN,ne);
  }

  //Notify remote MIHF
  remote_sub = info->getRegisteredRemoteUser (MIH_LINK_DOWN);
  for (int i = 0 ; i < (int)remote_sub->size() ; i++) {
    //send message
    Packet *p = allocpkt();
    hdr_mih *mihh = HDR_MIH(p);
    hdr_cmn *hdrc = HDR_CMN(p);
    
    hdrc->ptype() = PT_MIH;
    hdrc->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_link_down_ind); 

    //fill up mih packet information
    mihh->hdr.protocol_version = 1;
    mihh->hdr.mih_service_id = MIH_EVENT_SERVICE;
    mihh->hdr.mih_opcode = MIH_INDICATION;
    mihh->hdr.mih_aid = MIH_MSG_LINK_DOWN; 
    mihh->hdr.transaction_id = transactionId_++;
    mihh->hdr.session_id = remote_sub->at(i)->id;

    (mihh->payload.mih_link_down_ind_).linkType = e->linkIdentifier.type;
    int_to_eth (e->linkIdentifier.macMobileTerminal, (mihh->payload.mih_link_down_ind_).macMobileTerminal);
    int_to_eth (e->linkIdentifier.macPoA, (mihh->payload.mih_link_down_ind_).macPoA);
    int_to_eth (e->macOldAccessRouter, (mihh->payload.mih_link_down_ind_).macOldAccessRouter);
    (mihh->payload.mih_link_down_ind_).reasonCode = e->reason;

    debug ("\tSending event to node %s\n",Address::instance().print_nodeaddr(remote_sub->at(i)->mihf->address));

    send_mih_message (NULL, p, remote_sub->at(i));    
  }
 

}

/**
 * Process the link going down event
 * A link down should only occurs after a link up (which mean the data must be here)
 * @param e The event information
 */
void MIHAgent::process_link_going_down (link_going_down_t *e)
{
  MIHInterfaceInfo *info;
  vector <MIHUserAgent *> *local_sub;
  vector <session_info *> *remote_sub;
  int localMac;
  
  debug ("At %f in %s MIH Agent received LINK GOING DOWN trigger.\n", NOW, MYNUM);

  //Which end point is local? MN or PoA
  if (is_local_mac (e->linkIdentifier.macMobileTerminal))
    localMac = e->linkIdentifier.macMobileTerminal;
  else
    localMac = e->linkIdentifier.macPoA;

  info = get_interface (localMac);
  assert (info!=NULL);
  info->setStatus (LINK_STATUS_GOING_DOWN);

  //look which interface manager must receive events
  local_sub = info->getRegisteredLocalUser (MIH_LINK_GOING_DOWN);
  for (int i = 0 ; i < (int)local_sub->size() ; i++) {
    link_going_down_t *ne = (link_going_down_t*) malloc (sizeof (link_going_down_t));
    ne->linkIdentifier.type = e->linkIdentifier.type;
    ne->linkIdentifier.macMobileTerminal = e->linkIdentifier.macMobileTerminal;
    ne->linkIdentifier.macPoA = e->linkIdentifier.macPoA;
    ne->timeInterval = e->timeInterval;
    ne->confidenceLevel = e->confidenceLevel;
    ne->reasonCode = e->reasonCode;
    ne->uniqueEventIdentifier = e->uniqueEventIdentifier;
    local_sub->at(i)->process_mih_event(MIH_LINK_GOING_DOWN,ne);
  }

  //Notify remote MIHF
  remote_sub = info->getRegisteredRemoteUser (MIH_LINK_GOING_DOWN);
  for (int i = 0 ; i < (int)remote_sub->size() ; i++) {
    //send message
    Packet *p = allocpkt();
    hdr_mih *mihh = HDR_MIH(p);
    hdr_cmn *hdrc = HDR_CMN(p);
    
    hdrc->ptype() = PT_MIH;
    hdrc->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_link_going_down_ind); 

    //fill up mih packet information
    mihh->hdr.protocol_version = 1;
    mihh->hdr.mih_service_id = MIH_EVENT_SERVICE;
    mihh->hdr.mih_opcode = MIH_INDICATION;
    mihh->hdr.mih_aid = MIH_MSG_LINK_GOING_DOWN; 
    mihh->hdr.transaction_id = transactionId_++;
    mihh->hdr.session_id = remote_sub->at(i)->id;

    (mihh->payload.mih_link_going_down_ind_).linkType = e->linkIdentifier.type;
    int_to_eth (e->linkIdentifier.macMobileTerminal, (mihh->payload.mih_link_going_down_ind_).macMobileTerminal);
    int_to_eth (e->linkIdentifier.macPoA, (mihh->payload.mih_link_going_down_ind_).macCurrentPoA);
    (mihh->payload.mih_link_going_down_ind_).timeInterval = (u_int16_t)(e->timeInterval/1000); //in ms
    (mihh->payload.mih_link_going_down_ind_).confidenceLevel = e->confidenceLevel;
    (mihh->payload.mih_link_going_down_ind_).uniqueEventIdentifier = e->uniqueEventIdentifier;  

    debug ("\tSending going down event to node %s\n",Address::instance().print_nodeaddr(remote_sub->at(i)->mihf->address));

    send_mih_message (NULL, p, remote_sub->at(i));
  }
}

/**
 * Process the link detected event
 * @param e The event information
 */
void MIHAgent::process_link_detected (link_detected_t *e)
{
  MIHInterfaceInfo *info;
  vector <MIHUserAgent *> *local_sub;
  vector <session_info *> *remote_sub;
  int localMac;

  debug ("At %f in %s MIH Agent received LINK DETECTED trigger.\n", NOW, MYNUM);
  
  //Which end point is local? MN or PoA
  if (is_local_mac (e->linkIdentifier.macMobileTerminal))
    localMac = e->linkIdentifier.macMobileTerminal;
  else
    localMac = e->linkIdentifier.macPoA;

  info = get_interface (localMac);
  assert (info!=NULL);
  info->setStatus (LINK_STATUS_DOWN); //detected is not up yet


  //look which interface manager must receive events
  local_sub = info->getRegisteredLocalUser (MIH_LINK_DETECTED);
  for (int i = 0 ; i < (int)local_sub->size() ; i++) {
    link_detected_t *ne = (link_detected_t*) malloc (sizeof (link_detected_t));
    ne->linkIdentifier.type = e->linkIdentifier.type;
    ne->linkIdentifier.macMobileTerminal = e->linkIdentifier.macMobileTerminal;
    ne->linkIdentifier.macPoA = e->linkIdentifier.macPoA;
    ne->mihCapability = e->mihCapability; 
    local_sub->at(i)->process_mih_event(MIH_LINK_DETECTED,ne);
  }

  //Notify remote MIHF
  remote_sub = info->getRegisteredRemoteUser (MIH_LINK_DETECTED);
  for (int i = 0 ; i < (int)remote_sub->size() ; i++) {
    //send message
    Packet *p = allocpkt();
    hdr_mih *mihh = HDR_MIH(p);
    hdr_cmn *hdrc = HDR_CMN(p);
    
    hdrc->ptype() = PT_MIH;
    hdrc->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_link_up_ind); 

    //fill up mih packet information
    mihh->hdr.protocol_version = 1;
    mihh->hdr.mih_service_id = MIH_EVENT_SERVICE;
    mihh->hdr.mih_opcode = MIH_INDICATION;
    mihh->hdr.mih_aid = MIH_MSG_LINK_DETECTED; 
    mihh->hdr.transaction_id = transactionId_++;
    mihh->hdr.session_id = remote_sub->at(i)->id;

    (mihh->payload.mih_link_detected_ind_).linkType = e->linkIdentifier.type;
    int_to_eth (e->linkIdentifier.macMobileTerminal, (mihh->payload.mih_link_detected_ind_).macMobileTerminal);
    int_to_eth (e->linkIdentifier.macPoA, (mihh->payload.mih_link_detected_ind_).macPoA);
    (mihh->payload.mih_link_detected_ind_).capability = e->mihCapability;

    send_mih_message (NULL, p, remote_sub->at(i));    

    debug ("\tSending event to node %s\n",Address::instance().print_nodeaddr(remote_sub->at(i)->mihf->address));
  }

}

/**
 * Process the link parameter change event
 * TBD: implement functionnality
 * @param e The event information
 */
void MIHAgent::process_link_parameters_report (link_parameters_report_t *e)
{
  MIHInterfaceInfo *info;
  vector <MIHUserAgent *> *local_sub;
  vector <session_info *> *remote_sub;
  int localMac;

  debug ("At %f in %s MIH Agent received LINK PARAMETER CHANGE trigger.\n", NOW, MYNUM);
  
  //Which end point is local? MN or PoA
  if (is_local_mac (e->linkIdentifier.macMobileTerminal))
    localMac = e->linkIdentifier.macMobileTerminal;
  else
    localMac = e->linkIdentifier.macPoA;

  info = get_interface (localMac);
  assert (info!=NULL);

  //look which interface manager must receive events
  local_sub = info->getRegisteredLocalUser (MIH_LINK_GOING_DOWN);
  for (int i = 0 ; i < (int)local_sub->size() ; i++) {
    link_parameters_report_t *ne = (link_parameters_report_t*) malloc (sizeof (link_parameters_report_t));
    ne->linkIdentifier.type = e->linkIdentifier.type;
    ne->linkIdentifier.macMobileTerminal = e->linkIdentifier.macMobileTerminal;
    ne->numLinkParameters = e->numLinkParameters;
    for (int j=0;j<e->numLinkParameters; j++) {
      ne->linkParameterList[j].parameter = e->linkParameterList[j].parameter;
      ne->linkParameterList[j].old_value = e->linkParameterList[j].old_value;
      ne->linkParameterList[j].new_value = e->linkParameterList[j].new_value;
    }
    local_sub->at(i)->process_mih_event(MIH_LINK_PARAMETERS_REPORT,ne);
  }

  //Notify remote MIHF
  remote_sub = info->getRegisteredRemoteUser (MIH_LINK_PARAMETERS_REPORT);
  for (int i = 0 ; i < (int)remote_sub->size() ; i++) {
      //send message
      Packet *p = allocpkt();
      hdr_mih *mihh = HDR_MIH(p);
      hdr_cmn *hdrc = HDR_CMN(p);
          
      hdrc->ptype() = PT_MIH;
      hdrc->size() = 66; //for test only
      
      //fill up mih packet information
      mihh->hdr.protocol_version = 1;
      mihh->hdr.mih_service_id = MIH_EVENT_SERVICE;
      mihh->hdr.mih_opcode = MIH_INDICATION;
      mihh->hdr.mih_aid = MIH_MSG_LINK_PARAMETERS_REPORT; 
      int_to_eth (e->linkIdentifier.macMobileTerminal, (mihh->payload.mih_link_going_down_ind_).macMobileTerminal);
      (mihh->payload.mih_link_parameters_report_ind_).nbParam = e->numLinkParameters;
      for (int j=0;j<e->numLinkParameters;j++) {
	(mihh->payload.mih_link_parameters_report_ind_).linkParameterList[j].parameter = e->linkParameterList[j].parameter;
	(mihh->payload.mih_link_parameters_report_ind_).linkParameterList[j].old_value = e->linkParameterList[j].old_value;
	(mihh->payload.mih_link_parameters_report_ind_).linkParameterList[j].new_value = e->linkParameterList[j].new_value;
      }
      
      send_mih_message (NULL, p, remote_sub->at(i)); 
      
      debug ("At %f Sending remote link parameters change event to node %s\n",NOW,Address::instance().print_nodeaddr(remote_sub->at(i)->mihf->address));
  }
}

/**
 * Process link handover imminent event
 * @param e The event information
 */
void MIHAgent::process_link_handover_imminent (link_handover_imminent_t *e)
{
  debug ("At %f in %s MIH Agent received LINK HANDOVER IMMINENT trigger.\n", NOW, MYNUM);
  MIHInterfaceInfo *info;
  vector <MIHUserAgent *> *local_sub;
  vector <session_info *> *remote_sub;
  int localMac;

  //Which end point is local? MN or PoA
  if (is_local_mac (e->linkIdentifier.macMobileTerminal))
    localMac = e->linkIdentifier.macMobileTerminal;
  else
    localMac = e->linkIdentifier.macPoA;

  info = get_interface (localMac);
  assert (info!=NULL);
  info->setStatus (LINK_STATUS_HANDOVER_IMMINENT);

  //look which interface manager must receive events
  local_sub = info->getRegisteredLocalUser (MIH_LINK_HANDOVER_IMMINENT);
  for (int i = 0 ; i < (int)local_sub->size() ; i++) {
    //send a copy of the event
    link_handover_imminent_t *ne = (link_handover_imminent_t*) malloc (sizeof (link_handover_imminent_t));
    ne->linkIdentifier.type = e->linkIdentifier.type;
    ne->linkIdentifier.macMobileTerminal = e->linkIdentifier.macMobileTerminal;
    ne->linkIdentifier.macPoA = e->linkIdentifier.macPoA;
    ne->newLinkIdentifier.type = e->newLinkIdentifier.type;
    ne->newLinkIdentifier.macMobileTerminal = e->newLinkIdentifier.macMobileTerminal;
    ne->newLinkIdentifier.macPoA = e->newLinkIdentifier.macPoA;
    ne->macOldAccessRouter = e->macOldAccessRouter;
    ne->macNewAccessRouter = e->macNewAccessRouter;
    local_sub->at(i)->process_mih_event(MIH_LINK_HANDOVER_IMMINENT,ne);
  }
  
  //Notify remote MIHF
  remote_sub = info->getRegisteredRemoteUser (MIH_LINK_HANDOVER_IMMINENT);
  for (int i = 0 ; i < (int)remote_sub->size() ; i++) {
    //send message
    Packet *p = allocpkt();
    hdr_mih *mihh = HDR_MIH(p);
    hdr_cmn *hdrc = HDR_CMN(p);
    
    hdrc->ptype() = PT_MIH;
    hdrc->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_link_handover_imminent_ind); 

    //fill up mih packet information
    mihh->hdr.protocol_version = 1;
    mihh->hdr.mih_service_id = MIH_EVENT_SERVICE;
    mihh->hdr.mih_opcode = MIH_INDICATION;
    mihh->hdr.mih_aid = MIH_MSG_LINK_HANDOVER_IMMINENT; 

    (mihh->payload.mih_link_handover_imminent_ind_).linkType = e->linkIdentifier.type;
    int_to_eth (e->linkIdentifier.macMobileTerminal, (mihh->payload.mih_link_handover_imminent_ind_).macMobileTerminal);
    int_to_eth (e->linkIdentifier.macPoA, (mihh->payload.mih_link_handover_imminent_ind_).macPoA);

    (mihh->payload.mih_link_handover_imminent_ind_).new_link_type = e->newLinkIdentifier.type;
    int_to_eth (e->newLinkIdentifier.macPoA, (mihh->payload.mih_link_handover_imminent_ind_).new_macPoA);
    int_to_eth (e->newLinkIdentifier.macMobileTerminal, (mihh->payload.mih_link_handover_imminent_ind_).new_macMobileTerminal);
    int_to_eth (e->macOldAccessRouter, (mihh->payload.mih_link_handover_imminent_ind_).macOldAccessRouter);
    int_to_eth (e->macNewAccessRouter, (mihh->payload.mih_link_handover_imminent_ind_).macNewAccessRouter);

    send_mih_message (NULL, p, remote_sub->at(i));
    
    debug ("\tSending event to node %s\n",Address::instance().print_nodeaddr(remote_sub->at(i)->mihf->address));
    send(p, 0);
  }

}

/**
 * Process link handoff complete event
 * @param e The event information
 */
void MIHAgent::process_link_handover_complete (link_handover_complete_t *e)
{
  debug ("At %f in %s MIH Agent received LINK HANDOVER COMPLETE trigger.\n", NOW, MYNUM);
  MIHInterfaceInfo *info;
  vector <MIHUserAgent *> *local_sub;
  vector <session_info *> *remote_sub;
  int localMac;

  //Which end point is local? MN or PoA
  if (is_local_mac (e->linkIdentifier.macMobileTerminal))
    localMac = e->linkIdentifier.macMobileTerminal;
  else
    localMac = e->linkIdentifier.macPoA;

  info = get_interface (localMac);
  assert (info!=NULL);
  info->setStatus (LINK_STATUS_HANDOVER_COMPLETE);

  //look which interface manager must receive events
  local_sub = info->getRegisteredLocalUser (MIH_LINK_HANDOVER_COMPLETE);
  for (int i = 0 ; i < (int)local_sub->size() ; i++) {
    //send a copy of the event
    link_handover_complete_t *ne = (link_handover_complete_t*) malloc (sizeof (link_handover_complete_t));
    ne->linkIdentifier.type = e->linkIdentifier.type;
    ne->linkIdentifier.macMobileTerminal = e->linkIdentifier.macMobileTerminal;
    ne->linkIdentifier.macPoA = e->linkIdentifier.macPoA;
    ne->macOldAccessRouter = e->macOldAccessRouter;
    ne->macNewAccessRouter = e->macNewAccessRouter;
    local_sub->at(i)->process_mih_event(MIH_LINK_HANDOVER_COMPLETE,ne);
  }
  
  //Notify remote MIHF
  remote_sub = info->getRegisteredRemoteUser (MIH_LINK_HANDOVER_COMPLETE);
  for (int i = 0 ; i < (int)remote_sub->size() ; i++) {
    //send message
    Packet *p = allocpkt();
    hdr_mih *mihh = HDR_MIH(p);
    hdr_cmn *hdrc = HDR_CMN(p);
    
    hdrc->ptype() = PT_MIH;
    hdrc->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_link_handover_complete_ind); 

    //fill up mih packet information
    mihh->hdr.protocol_version = 1;
    mihh->hdr.mih_service_id = MIH_EVENT_SERVICE;
    mihh->hdr.mih_opcode = MIH_INDICATION;
    mihh->hdr.mih_aid = MIH_MSG_LINK_HANDOVER_COMPLETE; 
    (mihh->payload.mih_link_handover_complete_ind_).new_linkType = e->linkIdentifier.type;
    int_to_eth (e->linkIdentifier.macMobileTerminal, (mihh->payload.mih_link_handover_complete_ind_).new_macMobileTerminal);
    int_to_eth (e->linkIdentifier.macPoA, (mihh->payload.mih_link_handover_complete_ind_).new_macPoA);
    int_to_eth (e->macOldAccessRouter, (mihh->payload.mih_link_handover_complete_ind_).macOldAccessRouter);
    int_to_eth (e->macNewAccessRouter, (mihh->payload.mih_link_handover_complete_ind_).macNewAccessRouter);
    send_mih_message (NULL, p, remote_sub->at(i));
    
    debug ("\tSending event to node %s\n",Address::instance().print_nodeaddr(remote_sub->at(i)->mihf->address));
    send(p, 0);
  }
}

/**
 * Process the link rollback event
 * TBD: implement functionnality
 * @param e The event information
 */
void MIHAgent::process_link_rollback (link_rollback_t *e)
{
  MIHInterfaceInfo *info;
  vector <MIHUserAgent *> *local_sub;
  vector <session_info *> *remote_sub;
  int localMac;
  
  debug ("At %f in %s MIH Agent received LINK ROLLBACK trigger.\n", NOW, MYNUM);
  //printf ("MacMN=%d, MacPoA=%d\n", e->linkIdentifier.macMobileTerminal, e->linkIdentifier.macPoA);

  //Which end point is local? MN or PoA
  if (is_local_mac (e->linkIdentifier.macMobileTerminal))
    localMac = e->linkIdentifier.macMobileTerminal;
  else
    localMac = e->linkIdentifier.macPoA;

  info = get_interface (localMac);
  assert (info!=NULL);
  info->setStatus (LINK_STATUS_UP);

  //look which interface manager must receive events
  local_sub = info->getRegisteredLocalUser (MIH_LINK_EVENT_ROLLBACK);
  for (int i = 0 ; i < (int)local_sub->size() ; i++) {
    link_rollback_t *ne = (link_rollback_t*) malloc (sizeof (link_rollback_t));
    ne->linkIdentifier.type = e->linkIdentifier.type;
    ne->linkIdentifier.macMobileTerminal = e->linkIdentifier.macMobileTerminal;
    ne->linkIdentifier.macPoA = e->linkIdentifier.macPoA;
    ne->uniqueEventIdentifier = e->uniqueEventIdentifier;
    local_sub->at(i)->process_mih_event(MIH_LINK_EVENT_ROLLBACK,ne);
  }

  //Notify remote MIHF
  remote_sub = info->getRegisteredRemoteUser (MIH_LINK_EVENT_ROLLBACK);
  for (int i = 0 ; i < (int)remote_sub->size() ; i++) {
    //send message
    Packet *p = allocpkt();
    hdr_mih *mihh = HDR_MIH(p);
    hdr_cmn *hdrc = HDR_CMN(p);
    
    hdrc->ptype() = PT_MIH;
    hdrc->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_link_rollback_ind); 

    //fill up mih packet information
    mihh->hdr.protocol_version = 1;
    mihh->hdr.mih_service_id = MIH_EVENT_SERVICE;
    mihh->hdr.mih_opcode = MIH_INDICATION;
    mihh->hdr.mih_aid = MIH_MSG_LINK_EVENT_ROLLBACK; 
    mihh->hdr.transaction_id = transactionId_++;
    mihh->hdr.session_id = remote_sub->at(i)->id;

    (mihh->payload.mih_link_rollback_ind_).linkType = e->linkIdentifier.type;
    int_to_eth (e->linkIdentifier.macMobileTerminal, (mihh->payload.mih_link_rollback_ind_).macMobileTerminal);
    int_to_eth (e->linkIdentifier.macPoA, (mihh->payload.mih_link_rollback_ind_).macCurrentPoA);
    (mihh->payload.mih_link_rollback_ind_).uniqueEventIdentifier = e->uniqueEventIdentifier;  

    debug ("\tSending event to node %s\n",Address::instance().print_nodeaddr(remote_sub->at(i)->mihf->address));

    send_mih_message (NULL, p, remote_sub->at(i));    
  }

}

/**
 * Process a scan response from lower layer
 * @param mac The lower layer
 * @param rsp The response
 */
void MIHAgent::process_scan_response (Mac *mac, void *rsp, int size) {

  debug ("At %f in %s MIH Agent received scan response.(pending requests=%d)\n", NOW, MYNUM,scan_req_.size());

  //put result in pending scan requests
  for (int i = 0 ; i < (int) scan_req_.size(); i++) {
    if (scan_req_.at(i)->process_scan_response (mac->addr(), rsp, size)) {
      if (scan_req_.at(i)->isLocalRequest()) {
	//forward to upper layer
	//fprintf (stderr, "Scan completed\n");
	scan_req_.at(i)->getUser ()->process_scan_conf (scan_req_.at(i)->getResponse());
      } else {
	//fprintf (stderr, "Scan completed\n");
	session_info* session = get_session(scan_req_.at(i)->getSession());
	//send response back
	Packet *p = allocpkt();
	hdr_mih *mihh = HDR_MIH(p);
	hdr_cmn *hdrc = HDR_CMN(p);
	
	hdrc->ptype() = PT_MIH;
	hdrc->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_link_rollback_ind); 
	
	//fill up mih packet information
	mihh->hdr.protocol_version = 1;
	mihh->hdr.mih_service_id = MIH_COMMAND_SERVICE;
	mihh->hdr.mih_opcode = MIH_RESPONSE;
	mihh->hdr.mih_aid = MIH_MSG_SCAN; 
	mihh->hdr.transaction_id = scan_req_.at(i)->getTransaction();
	mihh->hdr.session_id = scan_req_.at(i)->getSession();
	
	memcpy (&mihh->payload.mih_scan_rsp_, scan_req_.at(i)->getResponse(), sizeof (mih_scan_rsp));
	debug ("\tSending scan response to node %s\n",Address::instance().print_nodeaddr(session->mihf->address));

	send_mih_message (NULL, p, session);    
	
      }
      //pb: if I remove it and receive a request (resend) how can I find
      //about it?
      scan_req_.erase (scan_req_.begin()+i);
      i--; //adjust the index since we deleted an item
    }
  }
}

/********** MIH COMMANDS ****************/

/**
 * Register this MIH User to receive System Management messages
 * @param user The MIH User
 */
void MIHAgent::register_management_user (MIHUserAgent *user)
{
  assert (user);
  management_users_.push_back (user);
}


/**
 * Discover MIH Capability of link layer
 * @param user The MIH user
 * @param mihfid The ID of destination MIHF
 * @param linkId The Link identifier
 */
mih_cap_disc_conf_s *MIHAgent::mih_capability_discover (MIHUserAgent *user, u_int32_t mihfid, link_identifier_t *linkId)
{
  mih_cap_disc_conf_s * result = NULL;
  capability_list_s * linkCapability = NULL;

  if (mihfid == mihfId_) {
    //local MIHF. Check link capability on the local mac
    result = (mih_cap_disc_conf_s *) malloc (sizeof (mih_cap_disc_conf_s));
    result->linkIdentifier = *linkId;
    if (is_local_mac (linkId->macMobileTerminal)) {
      
      linkCapability = get_interface (linkId->macMobileTerminal)->getMac()->link_capability_discover ();
      result->capability = *linkCapability;
      free (linkCapability);
    } else if (is_local_mac (linkId->macPoA)) {      
      linkCapability = get_interface (linkId->macPoA)->getMac()->link_capability_discover ();
      result->capability = *linkCapability;
      free (linkCapability);
    } else {
      fprintf (stderr, "At %f in %s, This link identifier is unknown\n", NOW, MYNUM);
    }

  } else {
    //remote MIHF
  }
  return result;
}

/**
 * Request scanning for one or more links
 * @param user The MIH User
 * @param mihfid The ID of destination MIHF
 * @param req The scan request
 */
void MIHAgent::mih_scan (MIHUserAgent *user, u_int32_t mihfid, struct mih_scan_request_s *req)
{
  if (mihfid == mihfId_) {
    //local request
    MIHScan *scan= new MIHScan (this, user, req);
    scan_req_.push_back (scan);
  } else {
    //remote destination
    Packet *p = allocpkt();
    hdr_mih *mihh = HDR_MIH(p);
    hdr_cmn *hdrc = HDR_CMN(p);
    
    hdrc->ptype() = PT_MIH;
    hdrc->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_event_sub_req); //for test only

    //fill up mih packet information
    mihh->hdr.protocol_version = 1;
    mihh->hdr.mih_service_id = MIH_COMMAND_SERVICE;
    mihh->hdr.mih_opcode = MIH_REQUEST;
    mihh->hdr.mih_aid = MIH_MSG_SCAN;
    mihh->hdr.transaction_id = transactionId_++;
    mihh->hdr.session_id = get_mihf_info (mihfid)->session->id;

    (mihh->payload.mih_scan_req_).nbElement = req->nbElement;
    for (int i=0; i < req->nbElement; i++) {
      (mihh->payload.mih_scan_req_).requestSet[i].linkType = req->requestSet[i].linkIdentifier.type;
      int_to_eth (req->requestSet[i].linkIdentifier.macMobileTerminal, (mihh->payload.mih_scan_req_).requestSet[i].macMobileTerminal);
      int_to_eth (req->requestSet[i].linkIdentifier.macPoA, (mihh->payload.mih_scan_req_).requestSet[i].macPoA);
      // (mihh->payload.mih_scan_req_).requestSet[i].commandSize = req->requestSet[i].commandSize;
      //memcpy ((mihh->payload.mih_scan_req_).requestSet[i].scanCommand,req->requestSet[i].command , req->requestSet[i].commandSize);
    }

    debug ("At %f in %s, MIH sending scan request\n", NOW, MYNUM);
    send_mih_message (user, p, get_mihf_info (mihfid)->session);
  }
}

/**
 * Configure the mac using the given configuration
 * @param user The MIH User
 * @param mihfid The destination MIHF
 * @param config The configuration to apply
 */
mih_config_response_s * MIHAgent::mih_configure_link (MIHUserAgent *user, u_int32_t mihfid, mih_configure_req_s *config)
{
  if (mihfid == mihfId_) {
    //local request
    //compute how many elements are requested
    int nbElement = 0;
    int index = 0;
    for (int i = 0 ; i < 32 ; i++) {
      if ((config->information >> i)&0x1) {
	nbElement++;
      }
    }
    //allocate response
    struct mih_configure_conf_s *rsp = (mih_configure_conf_s*) malloc (sizeof(mih_configure_conf_s));
    rsp->responseSet = (mih_config_response_s*) malloc (nbElement * sizeof (mih_config_response_s));
    rsp->status = false;
    //check operation mode
    if ( (config->information >> CONFIG_OPERATION_MODE) & 0X1 ){
      rsp->status = get_mac (config->linkIdentifier.macMobileTerminal)->set_mode (config->operationMode);
      rsp->responseSet[index].type = CONFIG_OPERATION_MODE;
      rsp->responseSet[index++].resultCode = rsp->status;
    }
    if ( (config->information >> CONFIG_DISABLE_TRANSMITTER) & 0X1 ){
      //not supported
      rsp->status = false | rsp->status;
      rsp->responseSet[index].type = CONFIG_DISABLE_TRANSMITTER;
      rsp->responseSet[index++].resultCode = false;
    }
    if ( (config->information >> CONFIG_LINK_ID) & 0X1 ){
      //supported. If connect must be executed, the response
      //will be send upon receiving the link UP on the link
      //TBD
      rsp->status = true | rsp->status;
      if (config->linkIdentifier.macPoA != -1)
	get_mac (config->linkIdentifier.macMobileTerminal)->link_connect (config->linkIdentifier.macPoA);
      else
	get_mac (config->linkIdentifier.macMobileTerminal)->link_disconnect ();
      rsp->responseSet[index].type = CONFIG_LINK_ID;
      rsp->responseSet[index++].resultCode = true;
    }
    if ( (config->information >> CONFIG_CURRENT_ADDRESS) & 0x1 ) {
      //not supported since NS-2 does not allow it
      rsp->status = false | rsp->status;
      rsp->responseSet[index].type = CONFIG_CURRENT_ADDRESS;
      rsp->responseSet[index++].resultCode = false;
    }
    if ( (config->information >> CONFIG_SUSPEND_DRIVER) & 0x1 ) {
      //not supported
      rsp->status = false | rsp->status;
      rsp->responseSet[index].type = CONFIG_SUSPEND_DRIVER;
      rsp->responseSet[index++].resultCode = false;
    }
    if ( (config->information >> CONFIG_QOS_PARAMETER_LIST) & 0x1 ) {
      //execute mapping
      rsp->status = true | rsp->status;
      rsp->responseSet[index].type = CONFIG_QOS_PARAMETER_LIST;
      rsp->responseSet[index++].resultCode = false;
    }
  } else {
    //remote MIH, sends packet
  }
  return NULL;
}

/**
 * Subscribe to events on the given link
 * @param user The MIH user
 * @param mihfId The destination MIH ID 
 * @param linkId The Link identifier
 * @param events The events to subscribe
 * @return The status if the link is local or NULL
 */
link_event_status * MIHAgent::mih_event_subscribe ( MIHUserAgent *user, u_int32_t mihfId, link_identifier_t linkId, link_event_list events)
{
  MIHInterfaceInfo* iface_info;
  int localMac;

  assert (user);

  if (mihfId_ == mihfId) {
    //Which end point is local? MN or PoA
    if (is_local_mac (linkId.macMobileTerminal))
      localMac = linkId.macMobileTerminal;
    else
      localMac = linkId.macPoA;
    iface_info = get_interface (localMac);
      //add information
    debug ("MIH User subscribing for events %x on Mac %d\n",events, localMac);
    iface_info->event_subscribe (user, events);
    return iface_info->getMac()->link_event_subscribe (events);
  } else {
    //here we consider that the network wants to subscribe to MN. Can we do the opposite?
    //it seems that more checking is necessary. The interface might already exist
    //furthermore, the new interface is not added to any list. Needs to be checked.
    iface_info = new MIHInterfaceInfo (linkId);
    iface_info->setMIHSession (get_mihf_info(mihfId)->session);
  
    debug ("At %f in %s, registration for event on remote link (mac=%d)\n", NOW, MYNUM, linkId.macMobileTerminal);
    //find the remote MIH and send the request
    Packet *p = allocpkt();
    hdr_mih *mihh = HDR_MIH(p);
    hdr_cmn *hdrc = HDR_CMN(p);
    
    hdrc->ptype() = PT_MIH;
    hdrc->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_event_sub_req); //for test only

    //fill up mih packet information
    mihh->hdr.protocol_version = 1;
    mihh->hdr.mih_service_id = MIH_EVENT_SERVICE;
    mihh->hdr.mih_opcode = MIH_REQUEST;
    mihh->hdr.mih_aid = MIH_MSG_EVENT_SUBSCRIBE;
    mihh->hdr.transaction_id = transactionId_++;
    mihh->hdr.session_id = iface_info->getSession()->id;

    (mihh->payload.mih_event_sub_req_).linkType = linkId.type;
    int_to_eth (linkId.macMobileTerminal, (mihh->payload.mih_event_sub_req_).macMobileTerminal);
    int_to_eth (linkId.macPoA, (mihh->payload.mih_event_sub_req_).macPoA);
    (mihh->payload.mih_event_sub_req_).req_eventList = events;

    send_mih_message (user, p, iface_info->getSession());

    return NULL;
  }
}

/**
 * Unsubscribe to events on the given link
 * @param user The MIH user
 * @param mihfId The destination MIH ID 
 * @param events The events to unsubscribe
 * @return The status if the link is local or NULL
 */
link_event_status * MIHAgent::mih_event_unsubscribe (MIHUserAgent *user, u_int32_t mihfId, link_identifier_t linkId, link_event_list events)
{
  MIHInterfaceInfo* iface_info;
  int localMac;
  
  assert (user);
  
  if (mihfId_ == mihfId) {
    //Which end point is local? MN or PoA
    if (is_local_mac (linkId.macMobileTerminal))
      localMac = linkId.macMobileTerminal;
    else
      localMac = linkId.macPoA;
    iface_info = get_interface (localMac);
      //add information
    debug ("MIH User subscribing for events %x on Mac %d\n",events, localMac);
    iface_info->event_unsubscribe (user, events);
    //easy implementation for now. Normally we should check if there are more users
    //requesting information about the event if not, unregister to link layer.
    link_event_status * status = (link_event_status *) malloc (sizeof (link_event_status));
    *status=events;
    return status; //we assume you can always unregister so we return request
  } else {
    //here we consider that the network wants to subscribe to MN. Can we do the opposite?
    iface_info = new MIHInterfaceInfo (linkId);
    iface_info->setMIHSession (get_mihf_info(mihfId)->session);
  
    //printf ("at %f in %s, registration for event on remote link (mac=%d)\n", NOW, MYNUM, linkId.macMobileTerminal);
    //find the remote MIH and send the request
    Packet *p = allocpkt();
    hdr_mih *mihh = HDR_MIH(p);
    hdr_cmn *hdrc = HDR_CMN(p);
    
    hdrc->ptype() = PT_MIH;
    hdrc->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_event_sub_req); //for test only

    //fill up mih packet information
    mihh->hdr.protocol_version = 1;
    mihh->hdr.mih_service_id = MIH_EVENT_SERVICE;
    mihh->hdr.mih_opcode = MIH_REQUEST;
    mihh->hdr.mih_aid = MIH_MSG_EVENT_UNSUBSCRIBE;
    mihh->hdr.transaction_id = transactionId_++;
    mihh->hdr.session_id = iface_info->getSession()->id;

    (mihh->payload.mih_event_unsub_req_).linkType = linkId.type;
    int_to_eth (linkId.macMobileTerminal, (mihh->payload.mih_event_unsub_req_).macMobileTerminal);
    int_to_eth (linkId.macPoA, (mihh->payload.mih_event_unsub_req_).macPoA);
    (mihh->payload.mih_event_unsub_req_).req_eventList = events;

    send_mih_message (user, p, iface_info->getSession());

    return NULL;
  }
}

/**
 * Request the MIH to handle a Poll on the lower layers
 * It will collect the detected/available networks
 * @param poll_request The Poll request containin parameters
 * @return the link status if local, or NULL if remote
 */
mih_get_status_s *MIHAgent::mih_get_status (MIHUserAgent *user, u_int32_t mihfId, u_int32_t status_request)
{

  if (mihfId == mihfId_) {
    debug ("At %f in %s, MIH receives get_status for local MIH\n",NOW, MYNUM);
    struct mih_get_status_s *status = (mih_get_status_s *) malloc (sizeof (mih_get_status_s));
    status->information = status_request & 0x6; //currently only returns operation mode and link ID
    status->nbInterface = 0;
    //local MIHF
    for (int i = 0 ; i < (int)ifaces_.size() ; i++) {
      if (ifaces_.at(i)->isLocal()) {
	//Currently all MACs work in normal mode
	status->status[status->nbInterface].operationMode = NORMAL_MODE;	  
	status->status[status->nbInterface].linkId.macMobileTerminal = ifaces_.at(i)->getMacAddr ();
	status->status[status->nbInterface].linkId.macPoA = ifaces_.at(i)->getMacPoA ();
	status->status[status->nbInterface].linkId.type = ifaces_.at(i)->getType();
	//get the QoS
	status->nbInterface++;
      }
    }
    return status;
  } else {
    //Tcl& tcl = Tcl::instance();
    mihf_info *mihf = get_mihf_info (mihfId);
    //remote MIHF
    debug ("At %f in %s, MIH receives get_status for remote MIH\n",NOW, MYNUM);

    //request packet
    Packet *p = allocpkt();
    hdr_mih *mihh = HDR_MIH(p);
    hdr_cmn *hdrc = HDR_CMN(p);
    
    hdrc->ptype() = PT_MIH;
    hdrc->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_reg_req); 
 
    //fill up mih packet information
    mihh->hdr.protocol_version = 1;
    mihh->hdr.mih_service_id = MIH_COMMAND_SERVICE;
    mihh->hdr.mih_opcode = MIH_REQUEST;
    mihh->hdr.mih_aid = MIH_MSG_GET_STATUS;
    mihh->hdr.transaction_id = transactionId_++;
    mihh->hdr.session_id = mihf->session->id;
 
    (mihh->payload.mih_get_status_req_).link_status_parameter_type = status_request;

    send_mih_message (user, p, mihf->session);
  }
  return NULL;
}

/**
 * Switch an active session from one link to another
 * @param mode The handover mode
 * @param oldLink Old link identifier
 * @param newLink New link identifier
 * @param oldLinkAction Action for old link
 */
void MIHAgent::mih_switch (u_char mode, link_identifier_t oldLink, link_identifier_t newLink, u_char oldLinkAction)
{

}

/**
 * Send an Network Address Infomration request
 * @param user The MIH User
 * @param mihf Target MIHF
 * @param link Current link identifier
 * @param targetPoA Mac address of target PoA
 */
void MIHAgent::mih_network_address_information (MIHUserAgent *user, int mihfid, link_identifier_t link, int targetPoA)
{}

/**
 * Request information from Information Server
 * @param user The MIH User
 * To complete
 */
void MIHAgent::mih_get_information (MIHUserAgent *user)
{}



/***** Packet processing *****/

/**
 * Broadcast a capability request
 * @param mac The MAC to use for the request
 */
void MIHAgent::send_cap_disc_req (MIHUserAgent *user, Mac *mac) {

  /*
    I need a way of commanding an MIH agent
    to send a capability request packet.
    This seems like as good a way as any
    to do that.
  */

  /* the code here is lifted from the code
     responsible for sending a reply when
     the node gets a request 
  */

  /* mh->hdr.mih_aid */

  Tcl& tcl = Tcl::instance();
  //response packet
  Packet *p = allocpkt();
  hdr_ip *iph = HDR_IP(p);
  hdr_mih *mihh = HDR_MIH(p);
  hdr_cmn *hdrc = HDR_CMN(p);

  tcl.evalf ("%s get-node", mac->name()); 
  Node *node = (Node*) TclObject::lookup(tcl.result());
  
  iph->saddr()= node->address();
  iph->daddr() = 0xFFFFFFFF; // broadcast;
  iph->dport() = port(); // whatever the right port is
  hdrc->ptype() = PT_MIH;
  hdrc->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_cap_disc_req); 
  
  //fill up mih packet information
  mihh->hdr.protocol_version = 1;
  mihh->hdr.mih_service_id = MIH_COMMAND_SERVICE;
  mihh->hdr.mih_opcode = MIH_REQUEST;
  mihh->hdr.mih_aid = MIH_MSG_CAPABILITY_DISCOVER;
  
  //(mihh->payload.mih_cap_disc_req_).mihfId = mihfId_;
  mihh->hdr.mihf_id = mihfId_;

  (mihh->payload.mih_cap_disc_req_).supported_event_list = mihmib_.supported_events_;
  (mihh->payload.mih_cap_disc_req_).supported_command_list = mihmib_.supported_commands_;
  (mihh->payload.mih_cap_disc_req_).supported_transport_list = mihmib_.supported_transport_;
  (mihh->payload.mih_cap_disc_req_).supported_query_type_list = mihmib_.supported_query_type_;

  //Set transaction ID
  mihh->hdr.transaction_id = transactionId_++;
  //Create pending request to get response
  struct mih_pending_req * new_req = (mih_pending_req*) malloc (sizeof(mih_pending_req));
  new_req->user = user;
  new_req->pkt = p;
  new_req->retx = 0;
  new_req->timeout = mihmib_.cs_timeout_;
  new_req->waitRsp = true;
  new_req->l2transport = false;
  new_req->mac = mac;
  new_req->tid = mihh->hdr.transaction_id;
  //scheduler timer
  new_req->timer = new MIHRequestTimer (this, mihh->hdr.transaction_id);
  new_req->timer->sched (mihmib_.cs_timeout_);
  pending_req_.insert (make_pair (mihh->hdr.transaction_id, new_req));

  /* now that this is done, p is updated */
  /* now we send the request: */
  debug("At %f in %s MIH Agent sending capability discovery request\n", NOW, MYNUM);
  //fprintf(stderr, "sending capability request\n");

  //Change the MIH target and send message--Deprecated
  //tcl.evalf ("%s target [%s entry]",this->name(),node->name());
  //send(p->copy(), 0);

  //Most likely when a new network is detected we want to redirect 
  //the traffic and also execute MIH discovery. The ARP can only have
  //one packet on hold. Therefore, let's delay the discovery. Since 
  //this is done after the handover, it should not impact performances.
  tcl.evalf ("%s entry", node->name()); 
  NsObject* obj = (NsObject*) TclObject::lookup(tcl.result());
  Scheduler::instance().schedule(obj, p->copy(), MIH_CAP_DELAY);
}

/**
 * Processing of a Capability discover request
 * @param p The received packet
 */
void MIHAgent::recv_cap_disc_req (Packet* p)
{
  /* 
     what this does: gets a message from
     another peer MIHF and stores the
     data located in it
  */
  debug("At %f in %s MIH Agent received capability discovery request\n", NOW, MYNUM);
  //fprintf(stderr, "received mih capability request\n");
  //received packet
  hdr_ip *iph = HDR_IP(p);
  hdr_mih *mihh = HDR_MIH(p);
  //hdr_cmn *hdrc = HDR_CMN(p);
  //response packet
  Packet *p_res = allocpkt();
  hdr_ip *iph_res = HDR_IP(p_res);
  hdr_mih *mihh_res = HDR_MIH(p_res);
  hdr_cmn *hdrc_res = HDR_CMN(p_res);

  // this is how we extract the data: mihh1->payload.mih_cap_disc_req_;
  mihf_info *mihf_sender = (mihf_info*) malloc (sizeof (mihf_info));
  mihf_sender->id = mihh->hdr.mihf_id; //(u_int32_t)mihh->payload.mih_cap_disc_req_.mihfId;
  mihf_sender->address = iph->saddr();
  mihf_sender->supported_events = (link_event_list)mihh->payload.mih_cap_disc_req_.supported_event_list;
  mihf_sender->supported_commands = (link_command_list)mihh->payload.mih_cap_disc_req_.supported_command_list;
  mihf_sender->transport =  (transport_list)mihh->payload.mih_cap_disc_req_.supported_transport_list;
  mihf_sender->query_type = (is_query_type_list)mihh->payload.mih_cap_disc_req_.supported_query_type_list;
  //we are lucky. The AP/BS only has one interface so we can find interface to use
  mihf_sender->local_mac = get_mac_for_node(iph_res->saddr());
  /* what about macAddr? */


  /* anyway, now that i've completed this, i need to put it somewhere: */
  peer_mihfs_.push_back(mihf_sender);

  /* now, send a response: */
  //send reply
  iph_res->daddr() = iph->saddr();
  iph_res->dport() = iph->sport();
  hdrc_res->ptype() = PT_MIH;
  hdrc_res->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_cap_disc_res); 
  
  //fill up mih packet information
  mihh_res->hdr.protocol_version = 1;
  mihh_res->hdr.mih_service_id = MIH_COMMAND_SERVICE;
  mihh_res->hdr.mih_opcode = MIH_RESPONSE;
  mihh_res->hdr.mih_aid = MIH_MSG_CAPABILITY_DISCOVER;
  //Set transaction ID
  mihh_res->hdr.transaction_id = mihh->hdr.transaction_id;
  
  //(mihh_res->payload.mih_cap_disc_res_).mihfId = mihfId_;
  mihh_res->hdr.mihf_id = mihfId_;

  (mihh_res->payload.mih_cap_disc_res_).supported_event_list = mihmib_.supported_events_;
  (mihh_res->payload.mih_cap_disc_res_).supported_command_list = mihmib_.supported_commands_;
  (mihh_res->payload.mih_cap_disc_res_).supported_transport_list = mihmib_.supported_transport_;
  (mihh_res->payload.mih_cap_disc_res_).supported_query_type_list = mihmib_.supported_query_type_;
  /* now that this is done, p_res is updated */
  /* now we send a reply: */
  debug("At %f in %s MIH Agent sending capability discovery response\n", NOW, MYNUM);
  //fprintf(stderr, "sending capability response\n");
  send(p_res, 0);

}

/**
 * Processing of a Capability discover response
 * @param p The received packet
 */
void MIHAgent::recv_cap_disc_rsp (Packet* p)
{
  /* 
     what this does: gets a message from
     another peer MIHF and stores the
     data located in it
  */
  debug("At %f in %s MIH Agent received capability discovery response\n", NOW, MYNUM);

  // fprintf(stderr, "received capability response packet\n");
  //received packet
  hdr_mih *mihh = HDR_MIH(p);
  hdr_ip *iph = HDR_IP(p);

  mihf_info *mihf_sender = get_mihf_info (mihh->hdr.mihf_id);

  // this is how we extract the data: mihh1->payload.mih_cap_disc_req_;
  if (mihf_sender == NULL) {
    mihf_sender = (mihf_info *) malloc (sizeof (mihf_info));
    peer_mihfs_.push_back(mihf_sender);
  }
  mihf_sender->id = mihh->hdr.mihf_id; //(u_int32_t)mihh->payload.mih_cap_disc_res_.mihfId;
  mihf_sender->address = iph->saddr();
  mihf_sender->supported_events = (link_event_list)mihh->payload.mih_cap_disc_res_.supported_event_list;
  mihf_sender->supported_commands = (link_command_list)mihh->payload.mih_cap_disc_res_.supported_command_list;
  mihf_sender->transport =  (transport_list)mihh->payload.mih_cap_disc_res_.supported_transport_list;
  mihf_sender->query_type = (is_query_type_list)mihh->payload.mih_cap_disc_res_.supported_query_type_list;
  mihf_sender->local_mac = get_mac_for_node(iph->daddr());
  //printf ("Address = %s\n", Address::instance().print_nodeaddr(iph->daddr()));
  assert (mihf_sender->local_mac != NULL);
  /* what about macAddr? */

  /* simpler than recv_cap_disc_req because I don't have to respond to it */
  //send it to MIH User
  struct mih_pending_req * req= (*pending_req_.find (mihh->hdr.transaction_id)).second;
  if (req != NULL) { //if NULL, then duplicate response
    req->user->process_cap_disc_conf (mihh);
    clear_request (req);
  }
  
  Packet::free (p);
}

/**
 * Send a registration request to the given MIHF
 * @param user The MIH User
 * @param mihf The remote MIHF
 */
void MIHAgent::send_reg_request (MIHUserAgent *user, int mihfId) 
{
  Tcl& tcl = Tcl::instance();
  mihf_info *mihf = get_mihf_info (mihfId);
  //request packet
  Packet *p = allocpkt();
  hdr_ip *iph = HDR_IP(p);
  hdr_mih *mihh = HDR_MIH(p);
  hdr_cmn *hdrc = HDR_CMN(p);

  tcl.evalf ("%s get-node", mihf->local_mac->name()); 
  Node *node = (Node*) TclObject::lookup(tcl.result());
  
  iph->saddr()= node->address();
  iph->daddr() = mihf->address; 
  iph->dport() = port(); // whatever the right port is
  hdrc->ptype() = PT_MIH;
  hdrc->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_reg_req); 
  
  //fill up mih packet information
  mihh->hdr.protocol_version = 1;
  mihh->hdr.mih_service_id = MIH_COMMAND_SERVICE;
  mihh->hdr.mih_opcode = MIH_REQUEST;
  mihh->hdr.mih_aid = MIH_MSG_REGISTRATION;
  
  //(mihh->payload.mih_reg_req_).sourceID = mihfId_;
  mihh->hdr.mihf_id = mihfId_;
  (mihh->payload.mih_reg_req_).destID = mihf->id;
  (mihh->payload.mih_reg_req_).req_code = 0; //registration
  (mihh->payload.mih_reg_req_).msb_session = mihfId_; //should be random
  /* now that this is done, p is updated */

  //Set transaction ID
  mihh->hdr.transaction_id = transactionId_++;
  //Create pending request to get response
  struct mih_pending_req * new_req = (mih_pending_req*) malloc (sizeof(mih_pending_req));
  new_req->user = user;
  new_req->pkt = p;
  new_req->retx = 0;
  new_req->timeout = mihmib_.cs_timeout_;
  new_req->waitRsp = true;
  new_req->l2transport = false;
  new_req->mac = mihf->local_mac;
  new_req->tid = mihh->hdr.transaction_id;
  //scheduler timer
  new_req->timer = new MIHRequestTimer (this, mihh->hdr.transaction_id);
  new_req->timer->sched (mihmib_.cs_timeout_);
  pending_req_.insert (make_pair (mihh->hdr.transaction_id, new_req));

  /* now we send the request: */
  debug("At %f in %s MIH Agent sending registration request\n", NOW, MYNUM);
  //fprintf(stderr, "sending registration request\n");

  tcl.evalf ("%s target [%s entry]",this->name(),node->name());
  send(p->copy(), 0);  
}

/**
 * Send a registration respose to the given MIHF
 * @param user The MIH User
 * @param mihf The remote MIHF
 * @param reg_result The registration result
 * @param failure_code If it fails, this is the reason
 * @param validity If it succeeded, how long does it last
 * @param tid Transaction ID to use
 */
void MIHAgent::send_reg_response (MIHUserAgent *user, int mihfId, u_char reg_result, u_char failure_code, u_int32_t validity, u_int16_t tid) 
{
  Tcl& tcl = Tcl::instance();
  mihf_info *mihf = get_mihf_info (mihfId);

  tcl.evalf ("%s get-node", mihf->local_mac->name()); 
  Node *node = (Node*) TclObject::lookup(tcl.result());

  //if the registration failed, remove session information previously created
  if (reg_result == 0) {
    delete_session (mihfId << 16 + mihfId_);
  }
  else {
    //set session for mihf
    mihf->session = get_session (mihfId << 16 + mihfId_);
  }

  //response packet
  Packet *p = allocpkt();
  hdr_ip *iph = HDR_IP(p);
  hdr_mih *mihh = HDR_MIH(p);
  hdr_cmn *hdrc = HDR_CMN(p);
 
  /* now, send a response: */
  //send reply
  iph->saddr() = node->address();
  iph->sport() = port();
  iph->daddr() = mihf->address;
  iph->dport() = port();
  hdrc->ptype() = PT_MIH;
  hdrc->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_reg_rsp); 
  
  //fill up mih packet information
  mihh->hdr.protocol_version = 1;
  mihh->hdr.mih_service_id = MIH_COMMAND_SERVICE;
  mihh->hdr.mih_opcode = MIH_RESPONSE;
  mihh->hdr.mih_aid = MIH_MSG_REGISTRATION;
  //Set transaction ID
  mihh->hdr.transaction_id = tid;
  
  //registration response
  //(mihh->payload.mih_reg_rsp_).sourceID = mihfId_;
  mihh->hdr.mihf_id = mihfId_;
  (mihh->payload.mih_reg_rsp_).destID = mihfId;
  (mihh->payload.mih_reg_rsp_).lsb_session = mihfId_; //should be random  
  (mihh->payload.mih_reg_rsp_).validity = validity;
  (mihh->payload.mih_reg_rsp_).reg_result = reg_result;
  (mihh->payload.mih_reg_rsp_).failure_code = failure_code;

  /* now that this is done, p_res is updated */
  /* now we send a reply: */
  debug("At %f in %s MIH Agent sending registration response\n", NOW, MYNUM);
  //fprintf(stderr, "sending registration response\n");
  send(p, 0);
}

/**
 * Processing of a Registration request
 * @param p The received packet
 */
void MIHAgent::recv_reg_req (Packet* p)
{
  debug("At %f in %s MIH Agent received registration request\n", NOW, MYNUM);
  
  //fprintf(stderr, "received registration request packet\n");

  //received packet
  hdr_ip *iph = HDR_IP(p);
  hdr_mih *mihh = HDR_MIH(p);

  /* Create session */
  session_info* new_session = (session_info*) malloc (sizeof (session_info));
  new_session->id = (mihh->payload.mih_reg_req_).msb_session << 16 + mihfId_;
  new_session->macAddr = get_mac_for_node(iph->daddr())->addr();
  new_session->mihf = get_mihf_info (mihh->hdr.mihf_id);
  new_session->lastTID = -1; //have not received packet yet
  sessions_.push_back (new_session);
  //printf ("Creating session = %d\n", new_session->id);

  //send indication to registered MIH Users
  for (int i=0; i < (int)management_users_.size (); i++) {
    management_users_.at(i)->process_register_ind (mihh);
  }
}

/**
 * Processing of a Registration response
 * @param p The received packet
 */
void MIHAgent::recv_reg_rsp (Packet* p)
{
  debug("At %f in %s MIH Agent received registration response\n", NOW, MYNUM);

  //fprintf(stderr, "received registration response packet\n");

  session_info* new_session = NULL;
  //received packet
  hdr_ip *iph = HDR_IP(p);
  hdr_mih *mihh = HDR_MIH(p);

  if ((mihh->payload.mih_reg_rsp_).reg_result == 1) {
    /* Create session */
    new_session = (session_info*) malloc (sizeof (session_info));
    new_session->id = mihfId_ << 16 +(mihh->payload.mih_reg_rsp_).lsb_session;
    new_session->macAddr = get_mac_for_node(iph->daddr())->addr();
    new_session->mihf = get_mihf_info (mihh->hdr.mihf_id);
    new_session->lastTID = -1; //have not received packet yet
    new_session->mihf->session = new_session;
    sessions_.push_back (new_session);
    //printf ("Creating session = %d\n", new_session->id);
  }

  //send it to MIH User
  struct mih_pending_req * req= (*pending_req_.find (mihh->hdr.transaction_id)).second;
  req->user->process_register_conf (&(mihh->payload.mih_reg_rsp_));

  clear_request (req);
}

/**
 * Send a registration request to the given MIHF
 * @param user The MIH User
 * @param mihf The remote MIHF
 */
void MIHAgent::send_dereg_request (MIHUserAgent *user, int mihfId) 
{
  mihf_info *mihf = get_mihf_info (mihfId);
  //request packet
  Packet *p = allocpkt();
  hdr_mih *mihh = HDR_MIH(p);
  hdr_cmn *hdrc = HDR_CMN(p);

  hdrc->ptype() = PT_MIH;
  hdrc->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_dereg_req); 

  //fill up mih packet information
  mihh->hdr.protocol_version = 1;
  mihh->hdr.mih_service_id = MIH_COMMAND_SERVICE;
  mihh->hdr.mih_opcode = MIH_REQUEST;
  mihh->hdr.mih_aid = MIH_MSG_DEREGISTRATION;
  mihh->hdr.transaction_id = transactionId_++;
  mihh->hdr.session_id = mihf->session->id;

  mihh->hdr.mihf_id = mihfId_;
  (mihh->payload.mih_dereg_req_).destID = mihf->id;

  /* now we send the request: */
  debug ("At %f in %s MIH sending deregistration request\n", NOW, MYNUM);
  send_mih_message (user, p, mihf->session);  
}

/**
 * Send a registration respose to the given MIHF
 * @param user The MIH User
 * @param mihf The remote MIHF
 * @param dereg_result The deregistration result
 * @param failure_code If it fails, this is the reason
 * @param tid Transaction ID to use
 */
void MIHAgent::send_dereg_response (MIHUserAgent *user, int mihfId, u_char dereg_result, u_char failure_code, u_int16_t tid) 
{
  mihf_info *mihf = get_mihf_info (mihfId);

  int i; //index of session in the vector
  for (i=0; i < (int) sessions_.size() && sessions_.at (i)!= mihf->session;i++);
  //if the registration failed, remove session information previously created
  if (dereg_result == 1) {
    delete_session (mihf->session->id);
  }

  //response packet
  Packet *p = allocpkt();
  hdr_mih *mihh = HDR_MIH(p);
  hdr_cmn *hdrc = HDR_CMN(p);
 
  hdrc->ptype() = PT_MIH;
  hdrc->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_dereg_rsp); 
  
  //fill up mih packet information
  mihh->hdr.protocol_version = 1;
  mihh->hdr.mih_service_id = MIH_COMMAND_SERVICE;
  mihh->hdr.mih_opcode = MIH_RESPONSE;
  mihh->hdr.mih_aid = MIH_MSG_DEREGISTRATION;
  //Set transaction ID
  mihh->hdr.transaction_id = tid;
  mihh->hdr.session_id = mihf->session->id;

  //registration response
  mihh->hdr.mihf_id = mihfId_;
  (mihh->payload.mih_dereg_rsp_).destID = mihfId;
  (mihh->payload.mih_dereg_rsp_).result_code = dereg_result;
  (mihh->payload.mih_dereg_rsp_).failure_code = failure_code;

  debug ("At %f in %s MIH sending deregistration response\n", NOW, MYNUM);
  send_mih_message (user, p, mihf->session);  
}

/**
 * Processing of a Deregistration request
 * @param p The received packet
 */
void MIHAgent::recv_dereg_req (Packet* p)
{
  debug("At %f in %s MIH Agent received deregistration request\n", NOW, MYNUM);
  
  //received packet
  hdr_mih *mihh = HDR_MIH(p);

  //send indication to registered MIH Users
  for (int i=0; i < (int)management_users_.size (); i++) {
    management_users_.at(i)->process_deregister_ind (mihh);
  }
}

/**
 * Processing of a Deregistration response
 * @param p The received packet
 */
void MIHAgent::recv_dereg_rsp (Packet* p)
{
  debug("At %f in %s MIH Agent received deregistration response\n", NOW, MYNUM);
  
  //received packet
  hdr_mih *mihh = HDR_MIH(p);
  
  if ((mihh->payload.mih_dereg_rsp_).result_code == 1) {
    delete_session (mihh->hdr.session_id);
  }
  
  //send it to MIH User
  struct mih_pending_req * req= (*pending_req_.find (mihh->hdr.transaction_id)).second;
  req->user->process_deregister_conf (&(mihh->payload.mih_dereg_rsp_));
  clear_request (req);
}

/**
 * Process an remote event registration
 * @param p The packet containing registration information
 */
void MIHAgent::recv_event_sub_req (Packet* p)
{
  debug("At %f in %s MIH Agent received event subscription request\n", NOW, MYNUM);

  MIHInterfaceInfo* iface_info;
  int localMac;
  link_identifier_t linkId;
  hdr_mih *mihh = HDR_MIH(p);

  //Check session is valid
  session_info *session = get_session (mihh->hdr.session_id);
  if (session == NULL) {
    debug ("\tSession %d not valid\n", mihh->hdr.session_id);
    return;
  }

  linkId.type = (link_type_t)(mihh->payload.mih_event_sub_req_).linkType;
  linkId.macMobileTerminal = eth_to_int ((mihh->payload.mih_event_sub_req_).macMobileTerminal);
  linkId.macPoA = eth_to_int ((mihh->payload.mih_event_sub_req_).macPoA);

  //Which end point is local? MN or PoA
  if (is_local_mac (linkId.macMobileTerminal))
    localMac = linkId.macMobileTerminal;
  else
    localMac = linkId.macPoA;
  iface_info = get_interface (localMac);
  //add information
  debug ("Remote MIH User subscribing for events %x on Mac %d\n",(mihh->payload.mih_event_sub_req_).req_eventList, localMac);
  iface_info->remote_event_subscribe (get_session (mihh->hdr.session_id), (mihh->payload.mih_event_sub_req_).req_eventList);
  link_event_status *status = iface_info->getMac()->link_event_subscribe ((mihh->payload.mih_event_sub_req_).req_eventList);
  Packet *p_res = allocpkt();
  hdr_mih *mihh_res = HDR_MIH(p_res);
  hdr_cmn *hdrc_res = HDR_CMN(p_res);
  
  hdrc_res->ptype() = PT_MIH;
  hdrc_res->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_event_sub_rsp); //for test only
  
  //fill up mih packet information
  mihh_res->hdr.protocol_version = 1;
  mihh_res->hdr.mih_service_id = MIH_EVENT_SERVICE;
  mihh_res->hdr.mih_opcode = MIH_RESPONSE;
  mihh_res->hdr.mih_aid = MIH_MSG_EVENT_SUBSCRIBE;
  mihh_res->hdr.transaction_id = mihh->hdr.transaction_id;
  mihh_res->hdr.session_id = mihh->hdr.session_id;
  
  (mihh_res->payload.mih_event_sub_rsp_).linkType = linkId.type;
  int_to_eth (linkId.macMobileTerminal, (mihh_res->payload.mih_event_sub_rsp_).macMobileTerminal);
  int_to_eth (linkId.macPoA, (mihh_res->payload.mih_event_sub_rsp_).macPoA);
  (mihh_res->payload.mih_event_sub_rsp_).res_eventList = *status;
  send_mih_message (NULL, p_res, session);
}


/**
 * Processing of an Event subscription response
 * @param p The received packet
 */
void MIHAgent::recv_event_sub_rsp (Packet* p)
{
  hdr_mih *mihh = HDR_MIH(p);

  debug ("At %f in %s MIH Agent received MIH_Event_Subscription.response\n", NOW, MYNUM);

  struct mih_pending_req * req= (*pending_req_.find (mihh->hdr.transaction_id)).second;

  //Check session is valid
  session_info *session = get_session (mihh->hdr.session_id);
  if (session == NULL) {
    debug ("\tSession %d not valid\n", mihh->hdr.session_id);
  } else {
    //send it to MIH User
    req->user->process_event_sub_conf (&(mihh->payload.mih_event_sub_rsp_));
  }
  clear_request (req);
}

/**
 * Processing of an Event unsubscription request
 * @param p The received packet
 */
void MIHAgent::recv_event_unsub_req (Packet* p)
{
  debug("At %f in %s MIH Agent received event unsubscription request\n", NOW, MYNUM);

  MIHInterfaceInfo* iface_info;
  int localMac;
  link_identifier_t linkId;
  hdr_mih *mihh = HDR_MIH(p);

  //Check session is valid
  session_info *session = get_session (mihh->hdr.session_id);
  if (session == NULL) {
    debug ("\tSession %d not valid\n", mihh->hdr.session_id);
    return;
  }

  linkId.type = (link_type_t)(mihh->payload.mih_event_unsub_req_).linkType;
  linkId.macMobileTerminal = eth_to_int ((mihh->payload.mih_event_unsub_req_).macMobileTerminal);
  linkId.macPoA = eth_to_int ((mihh->payload.mih_event_unsub_req_).macPoA);

  //Which end point is local? MN or PoA
  if (is_local_mac (linkId.macMobileTerminal))
    localMac = linkId.macMobileTerminal;
  else
    localMac = linkId.macPoA;
  iface_info = get_interface (localMac);
  //add information
  debug ("Remote MIH User unsubscribing for events %x on Mac %d\n",(mihh->payload.mih_event_unsub_req_).req_eventList, localMac);
  iface_info->remote_event_unsubscribe (get_session (mihh->hdr.session_id), (mihh->payload.mih_event_unsub_req_).req_eventList);

  Packet *p_res = allocpkt();
  hdr_mih *mihh_res = HDR_MIH(p_res);
  hdr_cmn *hdrc_res = HDR_CMN(p_res);
  
  hdrc_res->ptype() = PT_MIH;
  hdrc_res->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_event_unsub_rsp); 
  
  //fill up mih packet information
  mihh_res->hdr.protocol_version = 1;
  mihh_res->hdr.mih_service_id = MIH_EVENT_SERVICE;
  mihh_res->hdr.mih_opcode = MIH_RESPONSE;
  mihh_res->hdr.mih_aid = MIH_MSG_EVENT_UNSUBSCRIBE;
  mihh_res->hdr.transaction_id = mihh->hdr.transaction_id;
  mihh_res->hdr.session_id = mihh->hdr.session_id;
  
  (mihh_res->payload.mih_event_unsub_rsp_).linkType = linkId.type;
  int_to_eth (linkId.macMobileTerminal, (mihh_res->payload.mih_event_unsub_rsp_).macMobileTerminal);
  int_to_eth (linkId.macPoA, (mihh_res->payload.mih_event_unsub_rsp_).macPoA);
  (mihh_res->payload.mih_event_unsub_rsp_).res_eventList = (mihh->payload.mih_event_unsub_req_).req_eventList;
  
  send_mih_message (NULL, p_res, session);
}


/**
 * Processing of an Event unsubscription response
 * @param p The received packet
 */
void MIHAgent::recv_event_unsub_rsp (Packet *p)
{
  hdr_mih *mihh = HDR_MIH(p);

  debug ("At %f in %s MIH Agent received MIH_Event_Unsubscription.response\n", NOW, MYNUM);

  struct mih_pending_req * req= (*pending_req_.find (mihh->hdr.transaction_id)).second;

  //Check session is valid
  session_info *session = get_session (mihh->hdr.session_id);
  if (session == NULL) {
    debug ("\tSession %d not valid\n", mihh->hdr.session_id);
  } else {
    //send it to MIH User
    req->user->process_event_unsub_conf (&(mihh->payload.mih_event_unsub_rsp_));
  }
  clear_request (req);
}

/**
 * Processing of a Link UP notification
 * @param p The received packet
 */
void MIHAgent::recv_link_up_ind (Packet* p)
{
  debug ("At %f in %s MIH Agent received remote link up\n", NOW, MYNUM);
  //Dispatch to upper protocol
  link_up_t *e = (link_up_t*) malloc (sizeof (link_up_t));
  hdr_mih *mihh = HDR_MIH(p);
  e->linkIdentifier.type = (link_type_t)(mihh->payload.mih_link_up_ind_).linkType;
  e->linkIdentifier.macMobileTerminal = eth_to_int ((mihh->payload.mih_link_up_ind_).macMobileTerminal);
  e->linkIdentifier.macPoA = eth_to_int ((mihh->payload.mih_link_up_ind_).macNewPoA);
  e->mobilityManagement = (mihh->payload.mih_link_up_ind_).ipConfigurationMethod;
  e->macOldAccessRouter = eth_to_int ((mihh->payload.mih_link_up_ind_).macOldAccessRouter);
  e->macNewAccessRouter = eth_to_int ((mihh->payload.mih_link_up_ind_).macNewAccessRouter);
  e->ipRenewal = (mihh->payload.mih_link_up_ind_).ipRenewalFlag;
  process_link_up (e);
}

/**
 * Processing of a Link DOWN notification
 * @param p The received packet
 */
void MIHAgent::recv_link_down_ind (Packet* p)
{
  debug ("At %f in %s MIH Agent received remote link down\n", NOW, MYNUM);
  //Dispatch to upper protocol
  link_down_t *e = (link_down_t*) malloc (sizeof (link_down_t));
  hdr_mih *mihh = HDR_MIH(p);  
  e->linkIdentifier.type = (link_type_t)(mihh->payload.mih_link_down_ind_).linkType;
  e->linkIdentifier.macMobileTerminal = eth_to_int ((mihh->payload.mih_link_down_ind_).macMobileTerminal);
  e->linkIdentifier.macPoA = eth_to_int ((mihh->payload.mih_link_down_ind_).macPoA);
  e->macOldAccessRouter = eth_to_int ((mihh->payload.mih_link_down_ind_).macOldAccessRouter);
  e->reason = (link_down_reason_t)(mihh->payload.mih_link_down_ind_).reasonCode;
  process_link_down (e);
}

/**
 * Processing of a link Going Down notification
 * @param p The received packet
 */
void MIHAgent::recv_link_going_down_ind (Packet* p)
{
  debug ("At %f in %s MIH Agent received remote link going down\n", NOW, MYNUM);
  //Dispatch to upper protocol
  link_going_down_t *e = (link_going_down_t*) malloc (sizeof (link_going_down_t));
  hdr_mih *mihh = HDR_MIH(p);
  e->linkIdentifier.type = (link_type_t)(mihh->payload.mih_link_going_down_ind_).linkType;
  e->linkIdentifier.macMobileTerminal = eth_to_int ((mihh->payload.mih_link_going_down_ind_).macMobileTerminal);
  e->timeInterval = (mihh->payload.mih_link_going_down_ind_).timeInterval;
  e->linkIdentifier.macPoA = eth_to_int ((mihh->payload.mih_link_going_down_ind_).macCurrentPoA);
  e->confidenceLevel = (mihh->payload.mih_link_going_down_ind_).confidenceLevel;
  e->uniqueEventIdentifier = (mihh->payload.mih_link_going_down_ind_).uniqueEventIdentifier;
  process_link_going_down (e);
}

/**
 * Processing of a link Rollback notification
 * @param p The received packet
 */
void MIHAgent::recv_link_rollback_ind (Packet* p)
{
  debug ("At %f in %s MIH Agent received remote link rollback\n", NOW, MYNUM);
  //Dispatch to upper protocol
  link_rollback_t *e = (link_rollback_t*) malloc (sizeof (link_rollback_t));
  hdr_mih *mihh = HDR_MIH(p);
  e->linkIdentifier.type = (link_type_t)(mihh->payload.mih_link_rollback_ind_).linkType;
  e->linkIdentifier.macMobileTerminal = eth_to_int ((mihh->payload.mih_link_rollback_ind_).macMobileTerminal);
  e->linkIdentifier.macPoA = eth_to_int ((mihh->payload.mih_link_rollback_ind_).macCurrentPoA);
  e->uniqueEventIdentifier = (mihh->payload.mih_link_rollback_ind_).uniqueEventIdentifier;
  process_link_rollback (e);
}

/**
 * Processing of a link Detected notification
 * @param p The received packet
 */
void MIHAgent::recv_link_detected_ind (Packet* p)
{
  debug ("At %f in %s MIH Agent received remote link detected\n", NOW, MYNUM);
  //Dispatch to upper protocol
  link_detected_t *e = (link_detected_t*) malloc (sizeof (link_detected_t));
  hdr_mih *mihh = HDR_MIH(p);
  e->linkIdentifier.type = (link_type_t)(mihh->payload.mih_link_detected_ind_).linkType;
  e->linkIdentifier.macMobileTerminal = eth_to_int ((mihh->payload.mih_link_detected_ind_).macMobileTerminal);
  e->linkIdentifier.macPoA = eth_to_int ((mihh->payload.mih_link_detected_ind_).macPoA);
  e->mihCapability = (mihh->payload.mih_link_detected_ind_).capability;
  process_link_detected (e);
}

/**
 * Processing of a link Parameters report notification
 * @param p The received packet
 */
void MIHAgent::recv_link_parameters_report_ind (Packet* p)
{
  debug("At %f in %s MIH Agent received remote link parameters report\n", NOW, MYNUM);
  link_parameters_report_t *e = (link_parameters_report_t*) malloc (sizeof (link_parameters_report_t));
  hdr_mih *mihh = HDR_MIH(p);
  e->linkIdentifier.type = (link_type_t)(mihh->payload.mih_link_parameters_report_ind_).linkType;
  e->linkIdentifier.macMobileTerminal = eth_to_int ((mihh->payload.mih_link_parameters_report_ind_).macMobileTerminal);
  e->linkIdentifier.macPoA = eth_to_int ((mihh->payload.mih_link_parameters_report_ind_).macAccessRouter);
  
  e->numLinkParameters = (mihh->payload.mih_link_parameters_report_ind_).nbParam;
  memcpy(e->linkParameterList, (mihh->payload.mih_link_parameters_report_ind_).linkParameterList, sizeof(link_parameter_s) * MAX_NB_PARAM);
  process_link_parameters_report(e);
 
}

/**
 * Processing of a link handover imminent notification
 * @param p The received packet
 */
void MIHAgent::recv_link_handover_imminent_ind (Packet* p)
{
  debug ("At %f in %s MIH Agent received remote link handover imminent\n", NOW, MYNUM);
  //Dispatch to upper protocol
  link_handover_imminent_t *e = (link_handover_imminent_t*) malloc (sizeof (link_handover_imminent_t));
  hdr_mih *mihh = HDR_MIH(p);

  e->linkIdentifier.type = (link_type_t)(mihh->payload.mih_link_handover_imminent_ind_).linkType;
  e->linkIdentifier.macMobileTerminal = eth_to_int ((mihh->payload.mih_link_handover_imminent_ind_).macMobileTerminal);
  e->linkIdentifier.macPoA = eth_to_int ((mihh->payload.mih_link_handover_imminent_ind_).macPoA);
  e->newLinkIdentifier.type = (link_type_t)(mihh->payload.mih_link_handover_imminent_ind_).new_link_type;
  e->newLinkIdentifier.macMobileTerminal = eth_to_int ((mihh->payload.mih_link_handover_imminent_ind_).new_macMobileTerminal);
  e->newLinkIdentifier.macPoA = eth_to_int ((mihh->payload.mih_link_handover_imminent_ind_).new_macPoA);
  e->macOldAccessRouter = eth_to_int ((mihh->payload.mih_link_handover_imminent_ind_).macOldAccessRouter);
  e->macNewAccessRouter = eth_to_int ((mihh->payload.mih_link_handover_imminent_ind_).macNewAccessRouter);
  process_link_handover_imminent (e);
}

/**
 * Processing of a link handover complete notification
 * @param p The received packet
 */
void MIHAgent::recv_link_handover_complete_ind (Packet* p)
{
  debug ("At %f in %s MIH Agent received remote link handover complete\n", NOW, MYNUM);
  //Dispatch to upper protocol
  link_handover_complete_t *e = (link_handover_complete_t*) malloc (sizeof (link_handover_complete_t));
  hdr_mih *mihh = HDR_MIH(p);

  e->linkIdentifier.type = (link_type_t)(mihh->payload.mih_link_handover_complete_ind_).new_linkType;
  e->linkIdentifier.macMobileTerminal = eth_to_int ((mihh->payload.mih_link_handover_complete_ind_).new_macMobileTerminal);
  e->linkIdentifier.macPoA = eth_to_int ((mihh->payload.mih_link_handover_complete_ind_).new_macPoA);
  e->macOldAccessRouter = eth_to_int ((mihh->payload.mih_link_handover_complete_ind_).macOldAccessRouter);
  e->macNewAccessRouter = eth_to_int ((mihh->payload.mih_link_handover_complete_ind_).macNewAccessRouter);
  process_link_handover_complete (e);
}

/**
 * Processing of a Network address information request
 * @param p The received packet
 */
void MIHAgent::recv_network_address_info_req (Packet* p)
{}

/**
 * Processing of a Network address information response
 * @param p The received packet
 */
void MIHAgent::recv_network_address_info_res (Packet* p)
{}

/**
 * Send an MIH_Handover_initiate.request
 * @param 
 */
void MIHAgent::send_handover_initiate (Mac *mac, int newPoA)
{

}

/**
 * Send an MIH_Handover_initiate.response
 * @param p The received packet
 */
void MIHAgent::send_handover_initiate_response (Packet* p_req, int ack)
{

}

/**
 * Processing of a handover initiate request
 * @param p The received packet
 */
void MIHAgent::recv_handover_initiate_req (Packet* p)
{
  debug ("At %f in %s MIH Agent received MIH_Handover_initiate.request\n", NOW, MYNUM);
}

/**
 * Processing of a handover initiate response
 * @param p The received packet
 */
void MIHAgent::recv_handover_initiate_res (Packet* p)
{
  debug ("At %f in %s MIH Agent received MIH_Handover_initiate.response\n", NOW, MYNUM);
}

/**
 * Send an MIH_Handover_prepare.request
 * @param 
 */
void MIHAgent::send_handover_prepare (Mac *mac, int newPoA)
{

}

/**
 * Send an MIH_Handover_prepare.response
 * @param p The received packet
 */
void MIHAgent::send_handover_prepare_response (Packet* p_req, int ack)
{

}

/**
 * Processing of a handover initiate request
 * @param p The received packet
 */
void MIHAgent::recv_handover_prepare_req (Packet* p)
{
  debug ("At %f in %s MIH Agent received MIH_Handover_prepare.request\n", NOW, MYNUM);
}

/**
 * Processing of a handover initiate response
 * @param p The received packet
 */
void MIHAgent::recv_handover_prepare_res (Packet* p)
{
  debug ("At %f in %s MIH Agent received MIH_Handover_prepare.response\n", NOW, MYNUM);
}

/**
 * Send an MIH_Handover_commit.request
 * @param 
 */
void MIHAgent::send_handover_commit (Mac *mac, int newPoA)
{

}

/**
 * Send an MIH_Handover_commit.response
 * @param p The received packet
 */
void MIHAgent::send_handover_commit_response (Packet* p_req, int ack)
{

}

/**
 * Processing of a handover initiate request
 * @param p The received packet
 */
void MIHAgent::recv_handover_commit_req (Packet* p)
{
  debug ("At %f in %s MIH Agent received MIH_Handover_commit.request\n", NOW, MYNUM);
}

/**
 * Processing of a handover initiate response
 * @param p The received packet
 */
void MIHAgent::recv_handover_commit_res (Packet* p)
{
  debug ("At %f in %s MIH Agent received MIH_Handover_commit.response\n", NOW, MYNUM);
}

/**
 * Send an MIH_Handover_complete.request
 * @param 
 */
void MIHAgent::send_handover_complete (Mac *mac, int newPoA)
{

}

/**
 * Send an MIH_Handover_complete.response
 * @param p The received packet
 */
void MIHAgent::send_handover_complete_response (Packet* p_req, int ack)
{

}

/**
 * Processing of a handover initiate request
 * @param p The received packet
 */
void MIHAgent::recv_handover_complete_req (Packet* p)
{
  debug ("At %f in %s MIH Agent received MIH_Handover_complete.request\n", NOW, MYNUM);
}

/**
 * Processing of a handover initiate response
 * @param p The received packet
 */
void MIHAgent::recv_handover_complete_res (Packet* p)
{
  debug ("At %f in %s MIH Agent received MIH_Handover_complete.response\n", NOW, MYNUM);
}


/**
 * Process a remote MIH_Get_Status request
 * Note: currently support LINK ID only. 
 * @param p The packet containing the request
 */
void MIHAgent::recv_mih_get_status_req (Packet *p)
{

  //here we could check the session/sender. Right now assume we're fine.
  hdr_mih *mihh = HDR_MIH(p);

  debug ("At %f in %s received MIH_Get_Status.request on session %d\n",NOW, MYNUM, mihh->hdr.session_id);

  //Check session is valid
  session_info *session = get_session (mihh->hdr.session_id);
  if (session == NULL) {
    debug ("\tSession %d not valid\n", mihh->hdr.session_id);
    return;
  }

  mihf_info *mihf = session->mihf;
  
  assert (mihf!=NULL);

  //response packet
  Packet *p_res = allocpkt();
  //hdr_ip *iph_res = HDR_IP(p_res);
  hdr_mih *mihh_res = HDR_MIH(p_res);
  hdr_cmn *hdrc_res = HDR_CMN(p_res);
  int nbInterface = 0;


  (mihh_res->payload.mih_get_status_rsp_).link_status_parameter_type = (mihh->payload.mih_get_status_req_).link_status_parameter_type & 0x6; //currently only returns operation mode and link ID
  //local MIHF
  for (int i = 0 ; i < (int)ifaces_.size() ; i++) {
    if (ifaces_.at(i)->isLocal()) {
      //Currently all MACs work in normal mode
      (mihh_res->payload.mih_get_status_rsp_).status[nbInterface].operationMode = NORMAL_MODE;	  
      int_to_eth (ifaces_.at(i)->getMacAddr (), (mihh_res->payload.mih_get_status_rsp_).status[nbInterface].macMobileTerminal);
      int_to_eth (ifaces_.at(i)->getMacPoA (), (mihh_res->payload.mih_get_status_rsp_).status[nbInterface].macPoA);
      (mihh_res->payload.mih_get_status_rsp_).status[nbInterface].linkType = ifaces_.at(i)->getType();
      nbInterface++;
    }
  }
  (mihh_res->payload.mih_get_status_rsp_).nbInterface = nbInterface;

  //fill up mih packet information
  mihh_res->hdr.protocol_version = 1;
  mihh_res->hdr.mih_service_id = MIH_COMMAND_SERVICE;
  mihh_res->hdr.mih_opcode = MIH_RESPONSE;
  mihh_res->hdr.mih_aid = MIH_MSG_GET_STATUS;
  mihh_res->hdr.session_id = mihh->hdr.session_id;
  mihh_res->hdr.transaction_id = mihh->hdr.transaction_id;
  
  hdrc_res->size() += sizeof (mih_get_status_rsp);

  /* now we send a reply: */
  //fprintf(stderr, "sending MIH_Get_Status.response\n");
  send_mih_message (NULL, p_res, session);
}

/**
 * Process an MIH_Get_Status response
 * @param p The packet containing the response
 */
void MIHAgent::recv_mih_get_status_res (Packet *p)
{
  debug ("At %f in %s received MIH_Get_Status.response\n",NOW, MYNUM);

  hdr_mih *mihh = HDR_MIH(p);

  struct mih_pending_req * req= (*pending_req_.find (mihh->hdr.transaction_id)).second;

  //Check session is valid
  session_info *session = get_session (mihh->hdr.session_id);
  if (session == NULL) {
    debug ("\tSession %d not valid\n", mihh->hdr.session_id);
  } else {
    //send it to MIH User
    assert (req!=NULL);
    req->user->process_get_status_conf (mihh);
  }
  clear_request (req); 
}

/**
 * Process a remote MIH_Scan request
 * @param p The packet containing the request
 */
void MIHAgent::recv_mih_scan_req (Packet *p)
{
  debug ("At %f in %s received MIH_Scan.request\n",NOW, MYNUM);

  hdr_mih *mihh = HDR_MIH(p);
  
  //check if duplicate
  for (int i=0 ; i < (int)scan_req_.size() ; i++) {
    if (scan_req_.at(i)->getSession ()== (int)mihh->hdr.session_id && scan_req_.at(i)->getTransaction() == mihh->hdr.transaction_id) {
      //duplicate
      debug ("\tDuplicate request.\n");
      return;
    }
  }
  //now check the TID..this should be added to all methods.
  session_info* session = get_session(mihh->hdr.session_id);
  if (session->lastTID >= mihh->hdr.transaction_id) {
    //we received other packets since then, assume retransmission
    return;
  }
  //update last TID message
  session->lastTID = mihh->hdr.transaction_id;

  MIHScan *scan= new MIHScan (this, mihh->hdr.session_id, mihh->hdr.transaction_id, &(mihh->payload.mih_scan_req_));
  scan_req_.push_back (scan);
  
  //reply with an ack. Note: the message will not be sent
  //if the interface has been asked to scan
  //send response back
  Packet *p_res = allocpkt();
  hdr_mih *mihh_res = HDR_MIH(p_res);
  hdr_cmn *hdrc_res = HDR_CMN(p_res);
  
  hdrc_res->ptype() = PT_MIH;
  hdrc_res->size() = sizeof (struct hdr_mih_common) + sizeof (struct mih_scan_req); 

  //fill up mih packet information
  mihh_res->hdr.protocol_version = 1;
  mihh_res->hdr.mih_service_id = MIH_COMMAND_SERVICE;
  mihh_res->hdr.mih_opcode = MIH_RESPONSE;
  mihh_res->hdr.mih_aid = MIH_MSG_SCAN; 
  mihh_res->hdr.transaction_id = mihh->hdr.transaction_id;
  mihh_res->hdr.session_id = mihh->hdr.session_id;
  mihh_res->hdr.ack_rsp = 1;

  send_mih_message (NULL, p_res, session);      
}

/**
 * Process an MIH_Scan response
 * @param p The packet containing the response
 */
void MIHAgent::recv_mih_scan_res (Packet *p)
{
  debug ("At %f in %s received MIH_Scan.response\n",NOW, MYNUM);

  hdr_mih *mihh = HDR_MIH(p);
  
  //check if this is an ack
  if (mihh->hdr.ack_rsp==1) {
    debug ("\t just an ack to the scan request\n");
    struct mih_pending_req * req= (*pending_req_.find (mihh->hdr.transaction_id)).second;
    req->waitAck = false;
    if (req->timer->status() == TIMER_PENDING)
      req->timer->cancel();
    //maybe we should reschedule timer to wait for response instead of ACK
    return; 
  }

  //not an ACK, process response
  struct mih_pending_req * req= (*pending_req_.find (mihh->hdr.transaction_id)).second;
  req->user->process_scan_conf (&(mihh->payload.mih_scan_rsp_));
  clear_request (req);
}

/**
 * Process an MIH_Switch request
 * @param p The packet containing the request
 */
void MIHAgent::recv_mih_switch_req (Packet *p)
{
  /* handover mode is "Make before break" or "break before make" */
  debug ("At %f in %s received MIH_Switch.request\n",NOW, MYNUM);

  hdr_mih *mihh = HDR_MIH(p);
  //Check session is valid
  session_info *session = get_session (mihh->hdr.session_id);
  if (session == NULL) {
    debug ("\tSession %d not valid\n", mihh->hdr.session_id);
    return;
  }

  mihf_info *mihf = session->mihf;
  
  assert (mihf!=NULL);

  // moving right along, everything should be contained in mihh->payload.mih_switch_req_
  

  //int oldPoA = eth_to_int(mihh->payload.mih_switch_req_.macPoA);
  int newPoA = eth_to_int(mihh->payload.mih_switch_req_.new_macPoA);
  int mobileTerminal = eth_to_int(mihh->payload.mih_switch_req_.macMobileTerminal);
  u_char handover_mode = mihh->payload.mih_switch_req_.handover_mode;

  Mac *localMac = get_mac(mobileTerminal);
  if (handover_mode == 0) {
    // make before break sequence of events:
    /* "The network allocates transport facilities to the target point of attachment prior to the occurrence
       of the link switch event"
    */
    
    // get_mac(macAddr_);
    get_mac(mobileTerminal)->link_connect(newPoA);
  } else if (handover_mode == 1) {
    // break before make sequence of events:
    get_mac(mobileTerminal)->link_disconnect();
    get_mac(mobileTerminal)->link_connect(newPoA);
  }

  /*    
	get_mac(macAddr_).old_bss_id_ = get_mac(macAddr).bss_id_;
	get_mac(macAddr_).handover_ = 1;
	get_mac(macAddr_).send_link_handover_imminent(get_mac(macAddr_).addr(), get_mac(macAddr_).bss_id_, intMacPoA);
  */

  //response packet
  Packet *p_res = allocpkt();
  //hdr_ip *iph_res = HDR_IP(p_res);
  hdr_mih *mihh_res = HDR_MIH(p_res);
  //hdr_cmn *hdrc_res = HDR_CMN(p_res);
  //int nbInterface = 0;

 //fill up mih packet information
  mihh_res->hdr.protocol_version = 1;
  mihh_res->hdr.mih_service_id = MIH_COMMAND_SERVICE;
  mihh_res->hdr.mih_opcode = MIH_RESPONSE;
  mihh_res->hdr.mih_aid = MIH_MSG_SWITCH;
  mihh_res->hdr.transaction_id = mihh->hdr.transaction_id;
  mihh_res->hdr.session_id = mihh->hdr.session_id;

  /* default, it succeeds: */
  int bss_id=1;
  int bss_disconnected_val=BCAST_ADDR;
  if ((localMac->getType() == LINK_802_11)) {
    Mac802_11 *mac802_11 = (Mac802_11 *)localMac;
    bss_id = mac802_11->bss_id();
    bss_disconnected_val = BCAST_ADDR;
  } else if ((localMac->getType() == LINK_802_16)) {
    /*
      Mac802_16 *mac802_16 = (Mac802_16 *)localMac;
      bss_id = mac802_16->bss_id;
      bss_disconnected_val = mac802_16->IBSS_ID;
    */
  }
  // 1 = success, 0 = failure
  if (bss_id == bss_disconnected_val) {
    (mihh_res->payload.mih_switch_rsp_).status = 0 ;
  } else {
    (mihh_res->payload.mih_switch_rsp_).status = 1;
  }

}

/**
 * Process an MIH_Switch response
 * @param p The packet containing the response
 */
void MIHAgent::recv_mih_switch_res (Packet *p)
{

}

/**
 * Process expiration of pending request.
 * The packet has not been acknoledge. Retransmit
 * @param tid The Transaction ID for the request
 */
void MIHAgent::process_request_timer (int tid)
{
  //check retransmit and resend
  //fprintf (stderr, "At %f in %s Request timeout for %d\n", NOW, MYNUM, tid);

  struct mih_pending_req * req= (*pending_req_.find (tid)).second;

  if (req->retx < mihmib_.retryLimit_) {
    //must resend packet
    //fprintf (stderr, "\tresend packet (retx=%d)\n", req->retx+1);
    if (req->l2transport) {

    } else {
      Tcl& tcl = Tcl::instance();
      //get the node containing the interface
      tcl.evalf ("%s get-node", req->mac->name()); 
      Node *node = (Node*) TclObject::lookup(tcl.result());
      //redirect this target output to use the interface
      tcl.evalf ("%s target [%s entry]",this->name(),node->name());
      
      send(req->pkt->copy(), 0);
      req->timer->resched (mihmib_.cs_timeout_);
    }
    req->retx++;
  } else {
    //failed, drop request
    //fprintf (stderr, "\tdrop packet\n");
    Packet::free (req->pkt);
    //cannot delete the time since it is the one being processed. Memory leak needs to be fixed
    //delete (req->timer);
    pending_req_.erase (pending_req_.find (tid));
    free (req);
  }
}

/**
 * Send the given packet to the remote MIHF via the given session
 * @param user The MIHUser. Can be NULL if no ACK or response required
 * @param p The packet to send (with MIH Header filled)
 * @param session The session to remote MIHF
 */
void MIHAgent::send_mih_message (MIHUserAgent *user, Packet *p, session_info *session)
{
  struct mihf_info *mihf;
  Mac *mac;
  
  assert (p);
  assert (session);
  mihf = session->mihf;
  assert (mihf);
  mac = get_mac (session->macAddr);
  assert (mac);

  /* This method does the following:
     - Check the transport layer to use
     - create timer if the packet needs acknowledgement
  */

  hdr_mih *mihh = HDR_MIH(p);
  hdr_cmn *hdrc = HDR_CMN(p);
  hdr_ip *iph = HDR_IP(p);
  
  bool l2transport = false; //indicate if we can use l2
  bool l3transport = false; //indicate if we can use l3
  double timeout = 0;

  switch (mihh->hdr.mih_service_id) {
  case MIH_SYSTEM_SERVICE:
    break;
  case MIH_EVENT_SERVICE:
    l2transport = ((mihf->transport & 0x1) != 0);
    l3transport = ((mihf->transport & 0x2) != 0);
    timeout = mihmib_.es_timeout_;
    break;
  case MIH_COMMAND_SERVICE:
    l2transport = ((mihf->transport>>8 & 0x1) != 0);
    l3transport = ((mihf->transport>>8 & 0x2) != 0);
    timeout = mihmib_.cs_timeout_;
    break;
  case MIH_INFORMATION_SERVICE:
    l2transport = ((mihf->transport>>16 & 0x1) != 0);
    l3transport = ((mihf->transport>>16 & 0x2) != 0);
    timeout = mihmib_.is_timeout_;
    break;
  }

  if (!l2transport && !l3transport) {
    fprintf (stderr, "There is no transport protocol for this session\n");
    return;
  }  
  
  //if ACK or request, create pending request
  if (mihh->hdr.ack_req || mihh->hdr.mih_opcode == MIH_REQUEST) {
    struct mih_pending_req * new_req = (mih_pending_req*) malloc (sizeof(mih_pending_req));
    assert (user !=NULL);
    new_req->user = user;
    new_req->pkt = p;
    new_req->retx = 0;
    new_req->timeout = timeout;
    new_req->session = session;
    if (mihh->hdr.ack_req)
      new_req->waitAck = true;
    if (mihh->hdr.mih_opcode == MIH_REQUEST)
      new_req->waitRsp = true;
    if (l2transport)
      new_req->l2transport = true;
    else
      new_req->l2transport = false;
    new_req->mac = mac;
    new_req->tid = mihh->hdr.transaction_id;
    //scheduler timer
    new_req->timer = new MIHRequestTimer (this, mihh->hdr.transaction_id);
    new_req->timer->sched (timeout);
    pending_req_.insert (make_pair (mihh->hdr.transaction_id, new_req));
    //printf ("Created new pending request request tid=%d %x, pkt=%x\n", mihh->hdr.transaction_id, (unsigned int)new_req, (unsigned int)new_req->pkt);
  }

  if (l2transport) {
    //pass it to the MAC layer
    
  } else {
    Tcl& tcl = Tcl::instance();

    //get the node containing the interface
    tcl.evalf ("%s get-node", mac->name()); 
    Node *node = (Node*) TclObject::lookup(tcl.result());

    //fill layer 3 information
    iph->saddr()= node->address(); //use the interface address
    iph->daddr() = mihf->address;  //remote MIHF address
    iph->dport() = port(); // whatever the right port is
    hdrc->ptype() = PT_MIH;
    hdrc->size() += 40; //Add layer 3 (and 4?)  

    
    //redirect this target output to use the interface
    tcl.evalf ("%s target [%s entry]",this->name(),node->name());
    send(p->copy(), 0);
  }
}

