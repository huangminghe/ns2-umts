/*
 * Implementation of Handover module for MIPv6 (INFOCOM '05)
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

#include "handover-infocom.h"
#include "simulator.h"
#include "mip6.h"

static class InfocomHandoverClass : public TclClass {
public:
  InfocomHandoverClass() : TclClass("Handover/Infocom") {}
  TclObject* create(int, const char*const*) {
    return (new InfocomHandover());
  }
} class_simplehandover;


/*
 * Creates a handover module
 */
InfocomHandover::InfocomHandover() : Handover(), connectingMac_(0)
{
  prefix_timer_ = new PrefixHandleTimer (this);
}

void InfocomHandover::register_mih ()
{
  MIHAgent *mih;
  vector <Mac *> macs;

  assert (ifMngmt_);
  debug ("At %f in %s InfocomHandover registers the MACs\n", NOW, \
	 Address::instance().print_nodeaddr(ifMngmt_->addr()));
  
  mih = ifMngmt_->get_mih();
  macs = mih->get_local_mac ();

  for (int i=0; i < (int) macs.size() ; i++) {
    //register event
    mih->event_register (ifMngmt_, macs.at (i), MIH_LINK_DETECTED);
    mih->event_register (ifMngmt_, macs.at (i), MIH_LINK_UP);
    mih->event_register (ifMngmt_, macs.at (i), MIH_LINK_GOING_DOWN);
    mih->event_register (ifMngmt_, macs.at (i), MIH_LINK_DOWN);
  }
}

//INFOCOM
void InfocomHandover::process_link_parameter_change (link_parameter_change_t *e)
{
  MIHAgent *mih;
  link_parameter_config_t *config;
  
  debug("At %f in %s InfocomHandover processes Link Parameter Change\n", \
	NOW, Address::instance().print_nodeaddr(ifMngmt_->addr()));

  assert (ifMngmt_);

  mih = ifMngmt_->get_mih();

  config = Handover::get_mac_info (e->eventSource);

  //Add test to see which parameter has changed, for now it can only be assoc status
  //  if()
  //  config->assoc = e->new_value.assoc;
  //printf("New Status for the link : %d", config->assoc);

  //Richard: free something?? e?
}

/*
 * Process the link detected event
 * @param e The event information
 */
void InfocomHandover::process_link_detected (link_detected_t *e)
{
  MIHAgent *mih;
  link_parameter_config_t *config;

  assert (ifMngmt_);

  mih = ifMngmt_->get_mih();

  mih_interface_info **info = mih->get_interfaces ();
  int nb_info = mih->get_nb_interfaces ();

  config = Handover::get_mac_info (e->eventSource);
  debug ("At %f in %s InfocomHandover link detected \n\ttype %d, bandwidth = %f, MacAddr=%d, PoA=%d\n", \
	 NOW, Address::instance().print_nodeaddr(ifMngmt_->addr()), \
	 config->type, config->bandwidth, e->eventSource->addr(),e->macNewPoA);

  //CASE 1: we don't have any connection up, we want to use this one
  for(int i=0;i < nb_info; i++) {
    if (info[i]->status_==LINK_STATUS_UP || connectingMac_ == info[i]->mac_) {
      //case 1: we already have a connection up. Is this one better?
      link_parameter_config_t *config2 =  Handover::get_mac_info (info[i]->mac_);
      //compare to the new one
      if (config2->type == LINK_ETHERNET 
	  || (config2->type == LINK_802_11 && config->type != LINK_ETHERNET)) {
	//we have a better interface already. don't connected
	  debug ("\twe have a better interface already. don't connect\n");
	free (config2);
	goto done;
      }
      free (config2);
    }  
  }
  if (nb_info>0 )
    debug ("\tThe new interface is better...connect\n");
  else
    debug ("\tThis is the first interface...connect\n");

  //case 2: there is no other connection up or better, we want to use this one
  //INFOCOM - no better, but check if the better was already refused (added the condition)
  if (config->type == LINK_802_11 /*&& config->assoc == true*/) {
    debug ("\tI launch the connection on the link\n");
    connectingMac_ = e->eventSource; 
    mih->link_connect (e->eventSource, e->macNewPoA);
  }

 done:
  free (e);
  free (info);
  free (config);
}

/*
 * Process the link up event
 * @param e The event information
 */
void InfocomHandover::process_link_up (link_up_t *e)
{
  //We can use the interface, let's switch the flows to the new interface
  //Here we suppose that we only connect to the best interface
  MIHAgent *mih;
  link_parameter_config_t *config;
  vector <FlowEntry*> flows;
  vector <FlowEntry*> flows_to_redirect;

  assert (ifMngmt_);


  mih = ifMngmt_->get_mih();

  //first clear the connectingMac_ if necessary
  if (e->eventSource == connectingMac_) {
    connectingMac_ = NULL; //we are not waiting for it anymore
  }

  config = Handover::get_mac_info (e->eventSource);
  debug ("At %f in %s InfocomHandover received link up \n\ttype %d, bandwidth = %f, MacAddr=%d\n", \
	 NOW, Address::instance().print_nodeaddr(ifMngmt_->addr()),\
	 config->type, config->bandwidth, e->eventSource->addr());

  free (e);
  //free (mac_infos);
  free (config);
}

/*
 * Process the link down event
 * @param e The event information
 */
void InfocomHandover::process_link_down (link_down_t *e)
{
  //We look if there is any flow that uses this interface.
  //if yes, then we need to switch to another interface
  MIHAgent *mih;
  link_parameter_config_t *config;
  link_parameter_config_t *config2;
  vector <FlowEntry*> flows;
  mih_interface_info **mac_infos;
  int nb_mac_infos;
  int best_index, best_type, found=0;

  assert (ifMngmt_);

  debug ("At %f in %s InfocomHandover received link down \n\t type %d, bandwidth = %f, MacAddr=%d\n", \
	 NOW, Address::instance().print_nodeaddr(ifMngmt_->addr()),\
	 config->type, config->bandwidth, e->eventSource->addr());

  mih = ifMngmt_->get_mih();


  if (connectingMac_) {
    //another link went down but we know we are connecting to another one
    //it may also be the one from the handover
    debug ("\tWe are connecting to another interface, we don't look for another one\n");
    return;
  }

  config = Handover::get_mac_info (e->eventSource);



  //get the flows
  flows = ifMngmt_->get_flows ();
  //get list of interfaces
  mac_infos = ifMngmt_->get_mih ()-> get_interfaces ();
  nb_mac_infos = ifMngmt_->get_mih ()->get_nb_interfaces ();

  //printf ("\t++We have %d interfaces and %d flows\n", nb_mac_infos, flows.size());

  //get the first one 
  best_index = 0 ; 
  if (nb_mac_infos > 0) {
    while (best_index < nb_mac_infos && (mac_infos[best_index]->mac_ == e->eventSource
	 || mac_infos[best_index]->status_ != LINK_STATUS_UP))
      best_index++ ;
 
    if (best_index < nb_mac_infos){
      found = 1;
      config2 = Handover::get_mac_info (mac_infos[best_index]->mac_);
      best_type = config2->type;
      free (config2);
    }
  }
  for (int mac_index = best_index+1 ; mac_index < nb_mac_infos ; mac_index++) {
    if (mac_infos[mac_index]->mac_ != e->eventSource
	&& mac_infos[mac_index]->status_ == LINK_STATUS_UP) {
      //maybe we can use this one..
      config2 = Handover::get_mac_info (mac_infos[mac_index]->mac_);
      if ( (best_type == LINK_802_11 && config2->type == LINK_ETHERNET)
	   || (best_type == LINK_UMTS && (config2->type == LINK_802_11 
					  || config2->type == LINK_ETHERNET))){
	best_type = config2->type;
	best_index = mac_index;
      }
      free (config2);
    }
  }
  //check if we found another better one
  if (found) {
    Tcl& tcl = Tcl::instance();
    Node *node;
    tcl.evalf ("%s get-node", e->eventSource->name());
    node = (Node*) TclObject::lookup(tcl.result());
    //we transfert the flows 
    for (int i=0 ; i < (int)flows.size() ; i++) {
      //printf ("Studying flow %d using interface %s\n", i, Address::instance().print_nodeaddr(flows.at(i)->interface()->address()));
      //find the interface the flow is using
      if ( flows.at(i)->interface() == node){
	//we redirect this flow to the new interface
	Node *new_node;
	tcl.evalf ("%s get-node", mac_infos[best_index]->mac_->name());
	new_node = (Node*) TclObject::lookup(tcl.result());
	debug ("\tMust redirect this flow to use interface %s\n",Address::instance().print_nodeaddr(new_node->address()));
	tcl.evalf ("%s target [%s entry]", flows.at(i)->local()->name(), new_node->name());
	((MIPV6Agent*) ifMngmt_)->send_update_msg (flows.at(i)->remote(),new_node);
	flows.at(i)->update_interface(new_node);
      }
    }
  }
  else {
    printf ("\tNo better interface found :-(\n");
  }

  free (config);
  free (mac_infos);
  free (e);
}

/*
 * Process the link going down event
 * @param e The event information
 */
void InfocomHandover::process_link_going_down (link_going_down_t *e)
{
  //If we want to anticipate, look for any other link we could connect to
  //we could also start redirecting the flows to another interface.

  debug ("At %f in %s InfocomHandover received Link going down \n\t probability = %d%%\n", \
	 NOW, Address::instance().print_nodeaddr(ifMngmt_->addr()),\
	 e->confidenceLevel);

  free (e);
}

void InfocomHandover::process_new_prefix (new_prefix* data)
{
  debug ("At %f in %s InfocomHandover received new prefix %s\n", \
	 NOW, Address::instance().print_nodeaddr(ifMngmt_->addr()),\
	 Address::instance().print_nodeaddr(data->prefix));

  //prefix_timer_ = new PrefixHandleTimer (this, data);
  //prefix_timer_->new_data (data);
  //prefix_timer_->sched (5);
  compute_new_address (data->prefix, data->interface);
  //cout << "creating timer\n" ;
  process_new_prefix2 (data);
}



/*
 * Process a new prefix entry
 * @param data The new prefix information 
 */
void InfocomHandover::process_new_prefix2 (new_prefix* data)
{
  MIHAgent *mih;
  link_parameter_config_t *config;
  link_parameter_config_t *config2;
  vector <FlowEntry*> flows;
  vector <FlowEntry*> flows_to_redirect;
  mih_interface_info **mac_infos;
  int nb_mac_infos;
  Mac *mac;
  Tcl& tcl = Tcl::instance();

  //Now we got the address of new PoA
  //get the MAC for which we received a new prefix.
  tcl.evalf ("%s set mac_(0)",data->interface->name());
  mac = (Mac*) TclObject::lookup(tcl.result());

  //check if that is a better interface and redirect flows.
  //We can use the interface, let's switch the flows to the new interface
  //Here we suppose that we only connect to the best interface
  
  assert (ifMngmt_);

  mih = ifMngmt_->get_mih();

  config = Handover::get_mac_info (mac);
  
  debug ("At %f in %s InfocomHandover processes new prefix %s\n", \
	 NOW, Address::instance().print_nodeaddr(ifMngmt_->addr()),\
	 Address::instance().print_nodeaddr(data->prefix));

  //compute_new_address (data->prefix, data->interface);

  //Get the node of the new interface 
  Node *new_node = data->interface;
  //tcl.evalf ("%s get-node", e->eventSource->name());
  //new_node = (Node*) TclObject::lookup(tcl.result());

  //we check if there are flows that are using an interface with less priority
  flows = ifMngmt_->get_flows ();
  mac_infos = ifMngmt_->get_mih ()-> get_interfaces ();
  nb_mac_infos = ifMngmt_->get_mih ()->get_nb_interfaces ();
  //for each interface
  for (int mac_index = 0 ; mac_index < nb_mac_infos ; mac_index++) {
    if (mac_infos[mac_index]->mac_ != mac) {
      //this is not the new interface
      //get mac information
      config2 =  Handover::get_mac_info (mac_infos[mac_index]->mac_);
      //compare to the new one
      if (config2->type == LINK_UMTS && (config->type == LINK_802_11 ||config->type == LINK_ETHERNET)
	  || (config2->type == LINK_802_11 && config->type == LINK_ETHERNET)) {
	debug ("\tThe new up interface is better...checking for flows to redirect\n");
	//this is not the best interface type
	//get the flows that are using this interface
	Node *node;
	tcl.evalf ("%s get-node", mac_infos[mac_index]->mac_->name());
	node = (Node*) TclObject::lookup(tcl.result());
	//if (!node)
	  //  printf ("Node not found\n");
	//printf ("Found not so good interface on node %s\n", Address::instance().print_nodeaddr(node->address()));
	for (int i=0 ; i < (int)flows.size() ; i++) {
	  //printf ("Studying flow %d using interface %s\n", i, Address::instance().print_nodeaddr(flows.at(i)->interface()->address()));
	  //find the interface the flow is using
	  if ( flows.at(i)->interface() == node){
	    //we redirect this flow to the new interface
	    debug ("\tMust redirect flow from interface %s to %s\n",\
		   Address::instance().print_nodeaddr(flows.at(i)->interface()->address()), \
		   Address::instance().print_nodeaddr(new_node->address()));
	    flows.at(i)->tmp_iface = new_node;
	    flows_to_redirect.push_back (flows.at(i));
	  }
	}
      }
      free (config2);
    }
  }
 
  if ((int) flows_to_redirect.size()>0) {
    Node *bstation;
    //tcl.evalf ("%s get-node-by-mac %d", Simulator::instance().name(), e->macNewPoA);
    tcl.evalf ("%s  get-node-by-addr %s", Simulator::instance().name(), Address::instance().print_nodeaddr(data->prefix));

    bstation = (Node*) TclObject::lookup(tcl.result());
    ((MIPV6Agent*) ifMngmt_)->send_flow_request (flows_to_redirect, new_node, bstation->address());
  }


  free (mac_infos);
  free (config);
  free (data);
}

int InfocomHandover::compute_new_address (int prefix, Node *interface)
{
  int new_address;
  int old_address = interface->address();
  Tcl& tcl = Tcl::instance();

  new_address = (old_address & 0x7FF)|(prefix & 0xFFFFF800);

  char *os = Address::instance().print_nodeaddr(old_address);
  char *ns = Address::instance().print_nodeaddr(new_address);
  char *ps = Address::instance().print_nodeaddr(prefix);
  debug ("\told address: %s, prefix=%s, new address will be %s\n", os, ps, ns);

  //update the new address in the node
  tcl.evalf ("%s addr %s", interface->name(), ns);
  tcl.evalf ("[%s set ragent_] addr %s", interface->name(), ns);
  tcl.evalf ("%s base-station [AddrParams addr2id %s]",interface->name(),ps);  
  //if I update the address, then I also need to update the local route...
  
  delete []os;
  delete []ns;
  delete []ps;

  return new_address;

}
