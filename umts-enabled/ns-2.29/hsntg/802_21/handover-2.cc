/*
 * Implementation of Handover module for MIPv6 
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

#include "handover-2.h"
#include "simulator.h"
#include "mip6.h"
#include "mih-iface.h"
#include "mac-802_11.h"
#include "mih.h"

static class Handover2Class : public TclClass {
public:
  Handover2Class() : TclClass("Agent/MIHUser/IFMNGMT/MIPV6/Handover/Handover2") {}
  TclObject* create(int, const char*const*) {
    return (new Handover2());
  }
} class_handover_2;

/*
 * Creates a handover module
 */
Handover2::Handover2() : Handover(), scanTimer_(0), redirectMac_(-1), connectingMac_(-1)
{
  //Was taken from Handover1  
  //bind ("case_", &case_);
  case_ = 3;
  bind ("confidence_th_", &confidence_th_); 
  bind ("shutdown_on_ack_", &shutdown_on_ack_);
}

void Handover2::register_mih ()
{
  mih_get_status_s *status;

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
    int events = 0;
    switch (case_) {
    case 2:
      events = 0x1FB;
      break;
    case 3:
      events = 0x1FF;
      break;
    default:
      events = 0x1F9;
    }
    link_event_status * sub_status = mih_->mih_event_subscribe (this, mih_->getID(), status->status[i].linkId, events);
    assert (sub_status!=NULL); //since we register to local MAC we get instant response
    debug ("\tSubscription status: requested=%x, result=%x\n", events, *sub_status);
  }
  free (status);
}

/*
 * Process the link detected event
 * @param e The event information
 */
void Handover2::process_link_detected (link_detected_t *e)
{
  MIHInterfaceInfo **info = mih_->get_interfaces ();
  int nb_info = mih_->get_nb_interfaces ();

  debug ("At %f in %s Handover2 link detected \n\ttype %d, MacAddr=%d, PoA=%d\n", \
	 NOW, Address::instance().print_nodeaddr(addr()), \
	 e->linkIdentifier.type, e->linkIdentifier.macMobileTerminal, e->linkIdentifier.macPoA);

  //CASE 1: we don't have any connection up, we want to use this one
  for(int i=0;i < nb_info; i++) {
    if (info[i]->getStatus() == LINK_STATUS_UP || connectingMac_ == info[i]->getMacAddr()) {
      //case 1: we already have a connection up. Is this one better?
      //compare to the new one
      if (info[i]->getType() == LINK_ETHERNET 
	  || (info[i]->getType() == LINK_802_11 && e->linkIdentifier.type != LINK_ETHERNET)) {
	  //we have a better interface already. don't connected
	  debug ("\twe have a better interface already. don't connect\n");
	goto done;
      }
    }  
  }
  if (nb_info>0 )
    debug ("\tThe new interface is better...connect\n");
  else
    debug ("\tThis is the first interface...connect\n");

  //case 2: there is no other connection up or better, we want to use this one
  //INFOCOM - no better, but check if the better was already refused (added the condition)
  if (e->linkIdentifier.type == LINK_802_11 || e->linkIdentifier.type == LINK_802_16 /*&& config->assoc == true*/) {
    debug ("\tI launch the connection on the link\n");
    connectingMac_ = e->linkIdentifier.macMobileTerminal;
    mih_configure_req_s * req = (mih_configure_req_s *) malloc (sizeof (mih_configure_req_s));
    req->information = 1 << CONFIG_LINK_ID;
    req->linkIdentifier = e->linkIdentifier;
    //req->linkIdentifier.macPoA = e->linkIdentifier.macPoA;
    mih_->mih_configure_link (this, mih_->getID(), req);
  }

 done:
  free (e);
  free (info);
}

/*
 * Process the link up event
 * @param e The event information
 */
void Handover2::process_link_up (link_up_t *e)
{
  //We can use the interface, let's switch the flows to the new interface
  //Here we suppose that we only connect to the best 
  vector <FlowEntry*> flows;
  vector <FlowEntry*> flows_to_redirect;

  //first clear the connectingMac_ if necessary
  if (e->linkIdentifier.macMobileTerminal  == connectingMac_) {
    connectingMac_ = -1; //we are not waiting for it anymore
  }

  debug ("At %f in %s Handover2 received link up \n\ttype %d, MacAddr=%d, MacPoA=%d\n", \
	 NOW, Address::instance().print_nodeaddr(addr()),\
	 e->linkIdentifier.type,  e->linkIdentifier.macMobileTerminal, e->linkIdentifier.macPoA);

  if (case_ == 2 || case_ == 3) {
    debug ("\tCase2: sending RS\n");
    ((MIPV6Agent*)this)->send_rs(this->get_mih()->get_mac(e->linkIdentifier.macMobileTerminal));
  }

  free (e);
}

/*
 * Process the link down event
 * @param e The event information
 */
void Handover2::process_link_down (link_down_t *e)
{
  //We look if there is any flow that uses this interface.
  //if yes, then we need to switch to another interface
  vector <FlowEntry*> flows;
  MIHInterfaceInfo **mac_infos;
  int nb_mac_infos;
  int best_index, best_type, found=0;

  debug ("At %f in %s Handover2 received link down \n\tMacAddr=%d\n", \
	 NOW, Address::instance().print_nodeaddr(addr()),\
	 e->linkIdentifier.macMobileTerminal);

  if (connectingMac_ != -1) {
    //another link went down but we know we are connecting to another one
    //it may also be the one from the handover
    debug ("\tWe are connecting to another interface, we don't look for another one\n");
    return;
  }

  //get the flows
  flows = get_flows ();
  //get list of interfaces
  mac_infos = mih_-> get_interfaces ();
  nb_mac_infos = mih_->get_nb_interfaces ();

  //get the first one 
  best_index = 0 ; 
  if (nb_mac_infos > 0) {
    while (best_index < nb_mac_infos && (mac_infos[best_index]->getMacAddr() == e->linkIdentifier.macMobileTerminal
	 || mac_infos[best_index]->getStatus() != LINK_STATUS_UP))
      best_index++ ;
 
    if (best_index < nb_mac_infos){
      found = 1;
      best_type = mac_infos[best_index]->getType();
    }
  }
  for (int mac_index = best_index+1 ; mac_index < nb_mac_infos ; mac_index++) {
    if (mac_infos[mac_index]->getMacAddr() != e->linkIdentifier.macMobileTerminal
	&& mac_infos[mac_index]->getStatus() == LINK_STATUS_UP) {
      //maybe we can use this one..
      if ( (best_type == LINK_802_11 && mac_infos[mac_index]->getType() == LINK_ETHERNET)
	   || (best_type == LINK_802_16 && (mac_infos[mac_index]->getType() == LINK_802_11 
					  || mac_infos[mac_index]->getType() == LINK_ETHERNET))
	   || (best_type == LINK_UMTS && (mac_infos[mac_index]->getType() == LINK_802_16 
					  || mac_infos[mac_index]->getType() == LINK_802_11
					  || mac_infos[mac_index]->getType() == LINK_ETHERNET))){
	best_type = mac_infos[mac_index]->getType();
	best_index = mac_index;
      }
    }
  }
  //check if we found another better one
  if (found) {
    Tcl& tcl = Tcl::instance();
    Node *node;
    
    tcl.evalf ("%s get-node",  mac_infos[best_index]->getMac()->name());

    node = (Node*) TclObject::lookup(tcl.result());

    //we transfert the flows 
    for (int i=0 ; i < (int)flows.size() ; i++) {
      debug ("Studying flow %d using interface %s\n", i, Address::instance().print_nodeaddr(flows.at(i)->interface()->address()));
      //find the interface the flow is using
      if ( flows.at(i)->interface() != node){
	//we redirect this flow to the new interface
	//Node *new_node;
	//tcl.evalf ("%s get-node", mac_infos[best_index]->getMac()->name());
	//new_node = (Node*) TclObject::lookup(tcl.result());
	debug ("\tMust redirect this flow to use interface %s\n",Address::instance().print_nodeaddr(node->address()));
	tcl.evalf ("%s target [%s entry]", flows.at(i)->local()->name(), node->name());
	send_update_msg (flows.at(i)->remote(),node);
	redirectMac_ = mac_infos[best_index]->getMac()->addr();
	flows.at(i)->update_interface(node);
      }
    }
  }
  else {
    debug ("\tNo better interface found :-(\n");
    
    //in this particular scenario, when the 802.11 interface is down
    //we turn on 802.16 interface
    if (e->linkIdentifier.type == LINK_802_11) {
      for (int i = 0 ; i < nb_mac_infos ; i++) {
	if (mac_infos[i]->getType() == LINK_802_16 
	    && mac_infos[i]->getStatus() != LINK_STATUS_UP) {
	  mih_configure_req_s * req = (mih_configure_req_s *) malloc (sizeof (mih_configure_req_s));
	  req->information = 1 << CONFIG_OPERATION_MODE;
	  req->linkIdentifier.type = mac_infos[i]->getType();
	  req->linkIdentifier.macMobileTerminal = mac_infos[i]->getMacAddr();
	  req->operationMode = NORMAL_MODE;
	  mih_->mih_configure_link (this, mih_->getID(), req);
	}
      } 
    }
  }
  //scan 802.16 802.11
  if (e->linkIdentifier.type == LINK_802_16 || e->linkIdentifier.type == LINK_802_11) {
    struct mih_scan_request_s *req = (struct mih_scan_request_s *) malloc (sizeof (struct mih_scan_request_s));
    struct mih_scan_req_entry_s *entry = (struct mih_scan_req_entry_s *) malloc (sizeof (struct mih_scan_req_entry_s)); 
    req->nbElement = 1;
    req->requestSet = entry;
    req->requestSet[0].linkIdentifier = e->linkIdentifier;    
    int macAddr = e->linkIdentifier.macMobileTerminal;
    map<int,int>::iterator iter = pending_scan.find(macAddr);
    if(iter ==  pending_scan.end()){
      pending_scan[macAddr] = 1;
      mih_->mih_scan (this, mih_->getID(), req);
    }
  }
  free (mac_infos);
  free (e);
}

/*
 * Process the link going down event
 * @param e The event information
 */
void Handover2::process_link_going_down (link_going_down_t *e)
{
  //If we want to anticipate, look for any other link we could connect to
  //we could also start redirecting the flows to another interface.

  debug ("At %f in %s Handover2 received Link going down \n\t probability = %d%%\n", \
	 NOW, Address::instance().print_nodeaddr(addr()),\
	 e->confidenceLevel);

  if (e->confidenceLevel > confidence_th_ ) {
    MIHAgent *mih;
    link_down_t *e2 = (link_down_t *) malloc (sizeof (link_down_t));
    e2->linkIdentifier =  e->linkIdentifier;
    debug ("\tWe fake a link down\n");
    process_link_down (e2);
    mih = get_mih();
  }
  else
    debug ("\tDo nothing\n");
}

/*
 * Process an expired prefix
 * @param data The information about the prefix that expired
 */
void Handover2::process_exp_prefix (exp_prefix* data)
{
  Mac *mac;
  Tcl& tcl = Tcl::instance();

  debug ("At %f in %s Handover2 received expired prefix %s\n", \
	 NOW, Address::instance().print_nodeaddr(addr()),\
	 Address::instance().print_nodeaddr(data->prefix));

  if ((data->interface->address() & 0xFFFFF800) == (unsigned)data->prefix) {
    //Now we got the address of new PoA
    //get the MAC for which we received a new prefix.
    tcl.evalf ("%s set mac_(0)",data->interface->name());
    mac = (Mac*) TclObject::lookup(tcl.result());

  
    link_down_t *e = (link_down_t *) malloc (sizeof (link_down_t));
    e->linkIdentifier.type = mac->getType();
    e->linkIdentifier.macMobileTerminal = mac->addr();

    debug ("\tWe fake a link down\n");
    process_link_down (e);
  }

  free (data);
}


/*
 * Process a new prefix entry
 * @param data The new prefix information 
 */
void Handover2::process_new_prefix (new_prefix* data)
{
  MIHInterfaceInfo *config;
  MIHInterfaceInfo **mac_infos;
  int nb_mac_infos;
  vector <FlowEntry*> flows;
  vector <Agent*> flows_to_redirect;
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
  debug ("At %f in %s Handover2 received new prefix %s\n", NOW, str1, str2);
  delete []str1;
  delete []str2;

  compute_new_address (data->prefix, data->interface);

  //send capability request on new interface
  mih_->send_cap_disc_req (this, mac);

  config = mih_->get_interface (mac->addr());
  //Get the node of the new interface 
  Node *new_node = data->interface;

  //redirect the flows to this interface
  flows = get_flows ();
  mac_infos = mih_->get_interfaces ();
  nb_mac_infos = mih_->get_nb_interfaces ();
  //for each interface
  for (int mac_index = 0 ; mac_index < nb_mac_infos ; mac_index++) {
    //compare to the new one
    if (mac_infos[mac_index]->getType() == LINK_UMTS && (config->getType() == LINK_802_16 || config->getType() == LINK_802_11 ||config->getType() == LINK_ETHERNET)
	|| mac_infos[mac_index]->getType() == LINK_802_16 && (config->getType() == LINK_802_11 ||config->getType() == LINK_ETHERNET)
	|| (mac_infos[mac_index]->getType() == LINK_802_11 && config->getType() == LINK_ETHERNET)
	|| mac_infos[mac_index]->getMac() == mac) {
      debug ("\tThe new up interface is better...checking for flows to redirect\n");
      //get the flows that are using this interface
      Node *node;
      tcl.evalf ("%s get-node", mac_infos[mac_index]->getMac()->name());
      node = (Node*) TclObject::lookup(tcl.result());
      for (int i=0 ; i < (int)flows.size() ; i++) {
	debug ("Studying flow %d using interface %s\n", i, Address::instance().print_nodeaddr(flows.at(i)->interface()->address()));
	//find the interface the flow is using
	if ( flows.at(i)->interface() == node){
	  //we redirect this flow to the new interface
	  debug ("\tMust redirect flow from interface %s to %s\n",	\
		 Address::instance().print_nodeaddr(flows.at(i)->interface()->address()), \
		 Address::instance().print_nodeaddr(new_node->address()));
	  //flows.at(i)->tmp_iface = new_node;
	  tcl.evalf ("%s target [%s entry]", flows.at(i)->local()->name(), new_node->name());
	  flows_to_redirect.push_back (flows.at(i)->remote());
	  flows.at(i)->update_interface(data->interface);
	}
      }
    }
  }
 
  if ((int) flows_to_redirect.size()>0) {
    send_update_msg (flows_to_redirect,data->interface);
    redirectMac_ = mac->addr();
  }


  free (mac_infos);
  free (data);
  
}

/**
 * Process MIH_Capability_Discover.confirm
 * @param mihh The packet containing the response
 */
void Handover2::process_cap_disc_conf (hdr_mih *mihh)
{
  struct mih_cap_disc_res *rsp ;
  rsp = &(mihh->payload.mih_cap_disc_res_);

  assert (rsp!=NULL);
  

  char *str1 = Address::instance().print_nodeaddr(this->addr());
  debug ("At %f in %s Handover2 received MIH_Capability_discover.confirm\n", NOW, str1);
  delete []str1; 
 debug ("\tRemote MIHF: Id = %d, \n", mihh->hdr.mihf_id);
  //let's try to register
  mih_->send_reg_request (this, mihh->hdr.mihf_id);
}

/**
 * Process MIH_Register.confirm
 * @param rsp the result of the registration
 */
void Handover2::process_register_conf (struct mih_reg_rsp *rsp)
{
  assert (rsp!=NULL);

  char *str1 = Address::instance().print_nodeaddr(this->addr());
  debug ("At %f in %s Handover2 received MIH_Register.confirm\n", NOW, str1);
  delete []str1;
  debug ("\tStatus is : %d\n", rsp->reg_result);
}

/**
 * Process MIH_Deregister.confirm
 * @param rsp the result of the deregistration
 */
void Handover2::process_deregister_conf (struct mih_dereg_rsp *rsp)
{
  assert (rsp!=NULL);

  char *str1 = Address::instance().print_nodeaddr(this->addr());
  debug ("At %f in %s Handover2 received MIH_Deregister.confirm\n", NOW, str1);
  delete []str1;
  debug ("\tStatus is : %d\n", rsp->result_code);
}

/**
 * Process MIH_Scan.confirm
 * @param rsp The result to scan request
 */
void Handover2::process_scan_conf (struct mih_scan_rsp *rsp)
{
  bool found = false;
  BSSDescription *n;
  for(int i=0; i <  rsp->nbElement; i++){     
    pending_scan.erase(mih_->eth_to_int(rsp->resultSet[i].macMobileTerminal));
    switch(rsp->resultSet[i].linkType){
    case LINK_802_11 :
      Mac802_11_scan_response_s *scan_response;
      scan_response = (Mac802_11_scan_response_s *)(rsp->resultSet[i].scanResponse);
      for ( n=scan_response->bssDesc_head.lh_first; n ; n=n->next_entry()) {
	debug ("\t Scan AP %d on channel %d\n", n->bss_id(), n->channel());
	if((u_int32_t)n->bss_id() != (unsigned)mih_->eth_to_int(rsp->resultSet[i].macPoA)) {
	  break;
	}
      }
      if(n != NULL){
	debug ("\tI launch the connection on the link to AP %d on channel %d\n", n->bss_id(), n->channel());
	connectingMac_ = mih_->eth_to_int(rsp->resultSet[i].macMobileTerminal);
	mih_configure_req_s * req = (mih_configure_req_s *) malloc (sizeof (mih_configure_req_s));
	req->information = 1 << CONFIG_LINK_ID;
	req->linkIdentifier.type = (link_type_t)rsp->resultSet[i].linkType;
	req->linkIdentifier.macMobileTerminal = mih_->eth_to_int(rsp->resultSet[i].macMobileTerminal);
	req->linkIdentifier.macPoA = n->bss_id();
	//mih_->mih_configure_link (mih_->eth_to_int(rsp->resultSet[i].macMobileTerminal), req);
	mih_->mih_configure_link (this, mih_->getID(), req);
        found = true;
      }
      break;
    case LINK_802_16:
      int *listOfNewPoa = (int *)rsp->resultSet[i].scanResponse;
      int poa = mih_->eth_to_int(rsp->resultSet[i].macPoA);
      int nbElementPoa = rsp->resultSet[i].responseSize/sizeof(int);
      for(int j = 0; j < nbElementPoa; j++){
	if(poa != listOfNewPoa[j]) {
	  debug ("\tI launch the connection on the link to BS %d\n", listOfNewPoa[j]);
	  connectingMac_ = mih_->eth_to_int(rsp->resultSet[i].macMobileTerminal);
	  mih_configure_req_s * req = (mih_configure_req_s *) malloc (sizeof (mih_configure_req_s));
	  req->information = 1 << CONFIG_LINK_ID;
	  req->linkIdentifier.type = (link_type_t)rsp->resultSet[i].linkType;
	  req->linkIdentifier.macPoA =  listOfNewPoa[j];
	  req->linkIdentifier.macMobileTerminal = mih_->eth_to_int(rsp->resultSet[i].macMobileTerminal);
	  //mih_->mih_configure_link (this, mih_->eth_to_int(rsp->resultSet[i].macMobileTerminal), req);
	  mih_->mih_configure_link (this, mih_->getID(), req);
	  found = true;
	  break;
	}
      }
      break;
    }
  }
  //if we could not find any AP after scan, look for another interface
  if (!found) {
    Tcl& tcl = Tcl::instance();
    MIHInterfaceInfo **mac_infos;
    int nb_mac_infos;

    tcl.evalf ("%s get-node",  mih_->get_mac (mih_->eth_to_int (rsp->resultSet[0].macMobileTerminal))->name());
    Node *node = (Node*) TclObject::lookup(tcl.result());
    vector <FlowEntry*> flows = get_flows ();

    //for now, just pick any interface UP
    mac_infos = mih_-> get_interfaces ();
    nb_mac_infos = mih_->get_nb_interfaces ();
    //if interface is 802.11, scan periodically
    if(rsp->resultSet[0].linkType == LINK_802_11){
      //VG timer and scan
      //start new scan every 3 seconds
      struct link_identifier_t linkIdentifier;
      linkIdentifier.macMobileTerminal = mih_->eth_to_int(rsp->resultSet[0].macMobileTerminal);
      linkIdentifier.type = LINK_802_11;
      if(scanTimer_ == NULL && rsp->resultSet[0].linkType == LINK_802_11){
	scanTimer_ = new ScanTimerH2 (this, linkIdentifier);
	scanTimer_->sched (3.0);
      }else{
	scanTimer_-> update(this, linkIdentifier); 
	scanTimer_->resched (3.0);
      }
    }
    for (int mac_index = 0 ; mac_index < nb_mac_infos ; mac_index++) {
      if (mac_infos[mac_index]->getStatus() == LINK_STATUS_UP) {
	//let's us it
	for (int i=0 ; i < (int)flows.size() ; i++) {
	  debug ("Studying flow %d using interface %s\n", i, Address::instance().print_nodeaddr(flows.at(i)->interface()->address()));
	  //find the interface the flow is using
	  if ( flows.at(i)->interface() == node){
	    //we redirect this flow to the new interface
	    Node *new_node;
	    tcl.evalf ("%s get-node", mac_infos[mac_index]->getMac()->name());
	    new_node = (Node*) TclObject::lookup(tcl.result());
	    debug ("\tMust redirect this flow to use interface %s\n",Address::instance().print_nodeaddr(new_node->address()));
	    tcl.evalf ("%s target [%s entry]", flows.at(i)->local()->name(), new_node->name());
	    send_update_msg (flows.at(i)->remote(),new_node);
	    redirectMac_ = mac_infos[mac_index]->getMac()->addr();
	    flows.at(i)->update_interface(new_node);
	    break;
	  }
	}	
      }
    }
  }
}

void 
Handover2::process_scanTimer (struct link_identifier_t linkIdentifier)
{
  //scan 802.11
  if (linkIdentifier.type == LINK_802_11) {
    struct mih_scan_request_s *req = (struct mih_scan_request_s *) malloc (sizeof (struct mih_scan_request_s)); 
    struct mih_scan_req_entry_s *entry = (struct mih_scan_req_entry_s *) malloc (sizeof (struct mih_scan_req_entry_s)); 
    req->nbElement = 1;
    req->requestSet = entry;
    req->requestSet[0].linkIdentifier = linkIdentifier;
    struct Mac802_11_scan_request_s *command = (struct Mac802_11_scan_request_s*) malloc (sizeof (struct Mac802_11_scan_request_s));
    command->scanType = SCAN_TYPE_ACTIVE;
    command->action = SCAN_ALL;
    command->nb_channels = 11;
    command->pref_ch = (int*) malloc (11*sizeof (int));
    for (int i=0 ; i <11 ; i++) {
      command->pref_ch[i] = i+1;
    }
    command->minChannelTime = 0.020;
    command->maxChannelTime = 0.060;
    command->probeDelay = 0.002;
    req->requestSet[0].commandSize = sizeof (struct Mac802_11_scan_request_s);
    req->requestSet[0].command = command;
    int macAddr = linkIdentifier.macMobileTerminal;
    map<int,int>::iterator iter = pending_scan.find(macAddr);
    if(iter ==  pending_scan.end()){
      pending_scan[macAddr] = 1;
      mih_->mih_scan (this, mih_->getID(), req);
    }
  }
}

/* 
 * Process received packet
 * @param p The received packet
 * @param h Packet handler
 */
void Handover2::recv(Packet* p, Handler *h)
{
  hdr_cmn *hdrc = HDR_CMN(p);

  switch (hdrc->ptype()) {
  case PT_RRED:
    if (HDR_RTRED(p)->ack()) {
      recv_redirect_ack (p);
      Packet::free (p);
      return;
    } else {
      recv_redirect(p);
    }
    break;
  default:
    Handover::recv(p, h);
  }
}

/* 
 * Process received packet
 * @param p The received packet
 */
void Handover2::recv_redirect_ack (Packet* p)
{
  //received packet
  hdr_ip *iph   = HDR_IP(p);

  assert (HDR_CMN(p)->ptype() == PT_RRED);

  char *str1 = Address::instance().print_nodeaddr(this->addr());
  debug ("At %f Handover2 in %s received ack for redirect packet from %s\n", \
	 NOW, str1, Address::instance().print_nodeaddr(iph->saddr()));
  delete []str1;

  //if this ack indicates that handover is finished on 
  //wlan interface, shutdown the wman interface
  if (shutdown_on_ack_) {
    //check if this ack was sent to wlan interface. If it is, then 
    //thurn off wman interface
    Tcl& tcl = Tcl::instance();
    bool shutdown = false;
    link_identifier_t id16;
    link_identifier_t id11;
    mih_get_status_s *status = mih_->mih_get_status (this, mih_->getID(), 0x4); //request linkID    
    debug ("MIH_Get_Status results for %d interface(s):\n", status->nbInterface);
    for (int i=0; i < status->nbInterface ; i++) {
      debug ("\tLink ID: linkType=%d, macMobileTerminal=%d, macPoA=%d\n", status->status[i].linkId.type, status->status[i].linkId.macMobileTerminal, status->status[i].linkId.macPoA);
      if (status->status[i].linkId.type == LINK_802_16) {
	id16 = status->status[i].linkId;
      } else if (status->status[i].linkId.type == LINK_802_11) {
	id11 = status->status[i].linkId;	
      }
    }
    if (redirectMac_ == id11.macMobileTerminal) {
      debug ("\tShutdown 802.16 interface\n");
      //execute mih_configure
	mih_configure_req_s * req = (mih_configure_req_s *) malloc (sizeof (mih_configure_req_s));
	req->information = 1 << CONFIG_OPERATION_MODE;
	req->linkIdentifier = id16;
	req->operationMode = POWER_DOWN;
	mih_->mih_configure_link (this, mih_->getID(), req);
    }
    free (status);
  }
  redirectMac_ = -1;
}
