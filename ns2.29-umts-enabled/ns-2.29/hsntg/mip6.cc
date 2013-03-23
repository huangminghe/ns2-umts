/*
 * Implementation of interface management protocol
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
 * rouil - 2005/05/01 - 1.0     - Source created<BR>
 * <BR>
 * <BR>
 * </PRE><P>
 */

#include "mip6.h"
#include "nd.h"
#include "handover.h"
//NC
#include "mac-802_11.h"

#define MYNUM	Address::instance().print_nodeaddr(addr())

/*
 * Packet definitions
 */
int hdr_rtred::offset_;
static class RTREDHeaderClass : public PacketHeaderClass {
public:
	RTREDHeaderClass() : PacketHeaderClass("PacketHeader/RTRED",
					     sizeof(hdr_rtred)) {
	    bind_offset(&hdr_rtred::offset_);
        }
} class_rtredhdr;

int hdr_freq::offset_;
static class FREQHeaderClass : public PacketHeaderClass {
public:
	FREQHeaderClass() : PacketHeaderClass("PacketHeader/FREQ",
					     sizeof(hdr_freq)) {
	    bind_offset(&hdr_freq::offset_);
        }
} class_freqhdr;

int hdr_fres::offset_;
static class FRESHeaderClass : public PacketHeaderClass {
public:
	FRESHeaderClass() : PacketHeaderClass("PacketHeader/FRES",
					     sizeof(hdr_fres)) {
	    bind_offset(&hdr_fres::offset_);
        }
} class_freshdr;

int hdr_hand::offset_;
static class HANDHeaderClass : public PacketHeaderClass {
public:
	HANDHeaderClass() : PacketHeaderClass("PacketHeader/HAND",
					     sizeof(hdr_hand)) {
	    bind_offset(&hdr_hand::offset_);
        }
} class_handhdr;

/*
 * Static constructor for TCL
 */
static class MIPV6AgentClass : public TclClass {
public:
	MIPV6AgentClass() : TclClass("Agent/MIHUser/IFMNGMT/MIPV6") {}
	TclObject* create(int, const char*const*) {
		return (new MIPV6Agent());
		
	}
} class_ifmngmtagent;

/*
 * Creates a neighbor discovery agent
 */
MIPV6Agent::MIPV6Agent() : IFMNGMTAgent()
{
  flowRequestTimer_ = new FlowRequestTimer (this);
  seqNumberFlowRequest_ = 0;
  bind("flowRequestTimeout_", &flowRequestTimeout_);
}

/* 
 * Interface with TCL interpreter
 * @param argc The number of elements in argv
 * @param argv The list of arguments
 * @return TCL_OK if everything went well else TCL_ERROR
 */
int MIPV6Agent::command(int argc, const char*const* argv)
{
  return (IFMNGMTAgent::command(argc, argv));
}

/*
 * Process the given event
 * @param type The event type
 * @param data The data associated with the event
 */
void MIPV6Agent::process_nd_event (int type, void* data)
{
  switch (type){
  case ND_NEW_PREFIX: 
    process_new_prefix ((new_prefix *)data);
    break;
  case ND_PREFIX_EXPIRED: 
    process_exp_prefix ((exp_prefix *)data);
    break;
  }
}

/* 
 * Process received packet
 * @param p The received packet
 * @param h Packet handler
 */
void MIPV6Agent::recv(Packet* p, Handler *h)
{
  hdr_cmn *hdrc = HDR_CMN(p);

  switch (hdrc->ptype()) {
  case PT_RRED:
    recv_redirect(p);
    // handover_->recv (p,h); 
    break;
  case PT_FREQ:
    recv_flow_request(p);
    break;
  case PT_FRES:
    recv_flow_response (p);
    break;
  case PT_HAND:
    //handover_->recv (p, h);
    break;
  default:
    debug ("At %f MIPv6 Agent in %s received Unknown packet type\n", \
	   NOW, MYNUM);
  }
  Packet::free (p);
}

/*
 * Send an update message to the interface management.
 * @param dagent The agent that needs to redirect its messages
 * @param iface The new interface to use
 */
void MIPV6Agent::send_update_msg (Agent *dagent, Node *iface)
{
  vector <Agent *> agents;
  agents.push_back (dagent);
  send_update_msg (agents, iface);
}

/*
 * Send an update message to the interface management.
 * @param dagent The agent that needs to redirect its messages
 * @param iface The new interface to use
 */
void MIPV6Agent::send_update_msg (vector <Agent *> dagent, Node *iface)
{
  Packet *p = allocpkt();
  hdr_ip *iph = HDR_IP(p);
  hdr_rtred *rh = HDR_RTRED(p);
  hdr_cmn *hdrc = HDR_CMN(p);
  
  iph->saddr() = iface->address(); //we overwrite to use the interface address
  iph->daddr() = dagent.at(0)->addr();
  iph->dport() = port();
  hdrc->ptype() = PT_RRED;
  hdrc->size() = PT_RRED_SIZE;
    
  rh->ack() = 0;
  rh->destination() = iface->address();
  for (int i = 0 ; i < (int)dagent.size(); i++)
    rh->agent().push_back(dagent.at(i));
  
  //we need to send using the interface
  {
    Tcl& tcl = Tcl::instance();
    tcl.evalf ("%s target [%s entry]", this->name(), iface->name());
  }
 
  debug ("At %f MIPv6 Agent in %s send redirect message using interface %s\n", \
	 NOW, MYNUM, Address::instance().print_nodeaddr(iface->address()));
  
  send(p, 0);

}

/* 
 * Process received packet
 * @param p The received packet
 */
void MIPV6Agent::recv_redirect(Packet* p)
{
  //received packet
  hdr_ip *iph   = HDR_IP(p);
  hdr_rtred *rh = HDR_RTRED(p);

  if (rh->ack()) {
    recv_redirect_ack (p);
    return;
  }

  //reply packet
  Packet *p_ack = allocpkt();
  hdr_ip *iph_ack = HDR_IP(p_ack);
  hdr_rtred *rh_ack = HDR_RTRED(p_ack);
  hdr_cmn *hdrc_ack = HDR_CMN(p_ack);

  Tcl& tcl = Tcl::instance();

  assert (HDR_CMN(p)->ptype() == PT_RRED);

  debug ("At %f MIPv6 Agent in %s received redirect packet from %s\n", \
	 NOW, MYNUM, Address::instance().print_nodeaddr(iph->saddr()));

  for (int i = 0 ; i < (int) rh->agent().size() ; i++) {
    //modify the destination address of the agent.
    tcl.evalf ("%s set dst_addr_ %d", rh->agent().at(i)->name(), rh->destination());
  }

  //send ack message to sender
  iph_ack->daddr() = iph->saddr();
  iph_ack->dport() = iph->sport();
  hdrc_ack->ptype() = PT_RRED;
  hdrc_ack->size() = PT_RRED_SIZE;

  rh_ack->ack() = 1;
  rh_ack->destination() = rh->destination();
  rh_ack->agent() = rh->agent();

  send(p_ack, 0);
}

/* 
 * Process received packet
 * @param p The received packet
 */
void MIPV6Agent::recv_redirect_ack (Packet* p)
{
  //received packet
  hdr_ip *iph   = HDR_IP(p);

  assert (HDR_CMN(p)->ptype() == PT_RRED);

  debug ("At %f MIPv6 Agent in %s received ack for redirect packet from %s\n", \
	 NOW, MYNUM, Address::instance().print_nodeaddr(iph->saddr()));

  //do additional processing ??
}


/*
 * Send an RS message for the given interface
 * @param mac The interface to use
 */
void MIPV6Agent::send_rs (Mac *mac)
{
  assert (mac);

  NDAgent *nd_agent = get_nd_by_mac (mac);
  if (nd_agent) {
    debug ("At %f MIPv6 Agent in %s requests ND to send RS\n", NOW, MYNUM);
    nd_agent->send_rs ();
  }
  else {
    debug ("WARNING: At %f MIPv6 Agent in %s does not have ND to send RS\n", \
	   NOW, MYNUM);
  }
    
}

void MIPV6Agent::send_flow_request (vector<FlowEntry*> flows, Node *iface, int destination)
{
  Packet *p = allocpkt();
  hdr_ip *iph = HDR_IP(p);
  hdr_freq *rh = HDR_FREQ(p);
  hdr_cmn *hdrc = HDR_CMN(p);
    
  iph->daddr() = destination;
  iph->saddr() = iface->address();
  iph->dport() = port();
  hdrc->ptype() = PT_FREQ;
  hdrc->size() = PT_FREQ_SIZE;
  
  debug ("At %f MIPv6 Agent in %s send flow request to %s\n", \
	 NOW, MYNUM, Address::instance().print_nodeaddr(destination));
  
  rh->flow_info = flows;
  //seqNumberFlowRequest_ ++;
  rh->seqNumber_ = seqNumberFlowRequest_; 
  //we need to send using the interface
  {
    Tcl& tcl = Tcl::instance();
    tcl.evalf ("%s target [%s entry]", this->name(), iface->name());
  }
  flowRequestTimer_->attach_event(flows, iface, destination);
  flowRequestTimer_->resched(flowRequestTimeout_);
  send(p, 0);
}

/* 
 * Process received packet
 * @param p The received packet
 */
void MIPV6Agent::recv_flow_request(Packet* p)
{
  //received packet
  hdr_ip *iph = HDR_IP(p);
  hdr_freq *rh = HDR_FREQ(p);
  //repsonse packet
  Packet *p_res = allocpkt();
  hdr_ip *iph_res = HDR_IP(p_res);
  hdr_fres *rh_res = HDR_FRES(p_res);
  hdr_cmn *hdrc_res = HDR_CMN(p_res);
  

  float requested_bw=0;
  float available_bw = 5000;

  assert (HDR_CMN(p)->ptype() == PT_FREQ);

  debug ("At %f MIPv6 Agent in %s received flow request packet\n", NOW, MYNUM);

  //We check if we can accept the client
  
  //compute the total requested bandwidth
  
  for (int i= 0 ; i < (int) (rh->flow_info.size()) ; i++){
    requested_bw += rh->flow_info.at(i)->minBw();
  }
  debug ("\tRequested bandwidth = %f. Total aleady used=%f, available=%f \n", requested_bw, get_used_bw(), available_bw);
  
  //NC: get the bssid of the BS
  Tcl& tcl = Tcl::instance();
  tcl.evalf ("%s set mac_(0)", rh->flow_info.at(0)->tmp_iface->name());
  Mac *mac = (Mac*) TclObject::lookup(tcl.result());
  //printf("bss_id %d \n",((Mac802_11*)(mac))->bss_id());

  if ((((Mac802_11*) (mac))->bss_id()) != 8){
    //end NC
    //if ((available_bw-get_used_bw()) >= requested_bw){
    //we accept the new flows
    for (int i= 0 ; i < (int) (rh->flow_info.size()) ; i++){
      list_node *new_info = (list_node*) malloc (sizeof (list_node));
      rh->flow_info.at(i)->redirect_ = true;
      //store information
      new_info->minBw= rh->flow_info.at(i)->minBw();
      new_info->node = rh->flow_info.at(i)->tmp_iface;
      list_nodes.push_back (new_info);
      //update file
      nbAccepted_++; 
      //fprintf(Mac802_11::finfocom,"Connection accepted \t A %i \t R %i \t Bw %f\n",nbAccepted,nbRefused, get_used_bw());
      //fflush(Mac802_11::finfocom);
    }  
  }
  else {
    for (int i= 0 ; i < (int) (rh->flow_info.size()) ; i++){
      nbRefused_++;
      //NC
      rh->flow_info.at(i)->redirect_ = false;
      //end NC
      //fprintf(Mac802_11::finfocom,"Connection refused \t A %i \t R %i \t Bw %f\n",
      //      nbAccepted_, nbRefused_, get_used_bw()); 
      //fflush(Mac802_11::finfocom);
    }
  }
  
  //send result
  rh_res->flow_info = rh->flow_info; //attach same data
  iph_res->daddr() = iph->saddr();
  iph_res->dport() = iph->sport();
  hdrc_res->ptype() = PT_FRES;
  hdrc_res->size() = PT_FRES_SIZE;
  
  rh_res->seqNumber_ = rh->seqNumber_;
  debug ("\tSending reply to %s\n", Address::instance().print_nodeaddr((int)(iph_res->daddr())));
  send (p_res,0);

  /*802.21*/
  //tell the handover there is a new remote MAC
  //we want to register for link going down
  //get the mac of the node we need to connect
  /*
  if (handover_) {
    Tcl& tcl = Tcl::instance();
    tcl.evalf ("%s set mac_(0)", rh->flow_info.at(0)->tmp_iface->name());
    Mac *mac = (Mac*) TclObject::lookup(tcl.result());
    handover_->register_remote_mih (mac);
  }
  */
}

/* 
 * Process received packet
 * @param p The received packet
 */
void MIPV6Agent::recv_flow_response(Packet* p)
{
  hdr_fres *rh = HDR_FRES(p);
  Tcl& tcl = Tcl::instance();
  //NC
  int flow_accepted = 0;
  //end NC
  
  assert (HDR_CMN(p)->ptype() == PT_FRES);

  debug ("At %f MIPv6 Agent in %s received flow response packet\n", NOW, MYNUM);
  // cancel flow resquet timer
  if (flowRequestTimer_->status()!=TIMER_IDLE) {	
    flowRequestTimer_->cancel();}
  
  if( seqNumberFlowRequest_ == rh->seqNumber_)
    {
      //we look at the answer and redirect for the accepted flows
      for (int i= 0 ; i < (int) (rh->flow_info.size()) ; i++){
	if (rh->flow_info.at(i)->redirect_){
	  debug ("\tflow %d accepted...we redirect\n",i);
	  tcl.evalf ("%s target [%s entry]", rh->flow_info.at(i)->local()->name(), rh->flow_info.at(i)->tmp_iface->name());
	  vector<Agent*> agents;
	  agents.push_back (rh->flow_info.at(i)->remote());
	  send_update_msg (agents,rh->flow_info.at(i)->tmp_iface);
	  rh->flow_info.at(i)->update_interface(rh->flow_info.at(i)->tmp_iface);
	  //NC: If one flow is accepted the interface has to stay up
	  flow_accepted = 1;
	  //end NC
	}
	else {
	  //debug ("\tflow %d refused.\n",i);
	  //tcl.evalf ("%s set X_ 10", rh->flow_info.at(i)->tmp_iface->name());
	  //tcl.evalf ("%s set Y_ 10", rh->flow_info.at(i)->tmp_iface->name());
	  //tcl.evalf ("[%s set ragent_] set perup_ 1000",rh->flow_info.at(i)->tmp_iface->name() );
	}
      }
      //NC: if all the flows were rejected, disconnect the interface
      /*
      if( flow_accepted == 0){
	Tcl& tcl = Tcl::instance();
	tcl.evalf ("%s set mac_(0)", rh->flow_info.at(0)->tmp_iface->name());
	Mac *mac = (Mac*) TclObject::lookup(tcl.result());
	mih_->link_disconnect(mac);
	//end NC
      }
      */
      seqNumberFlowRequest_ ++;
    }
}

/* 
 * Compute the bandwidth used by registered MNs
 * @return The bandwidth used by registered MNs
 */
float MIPV6Agent::get_used_bw ()
{
  float used_bw=0;

  for (int i = 0 ; i < (int)list_nodes.size() ; i++){
    used_bw += list_nodes.at(i)->minBw;
  }

  return used_bw;
}

/*
void MIPV6Agent::process_client_going_down (int macSrc)
{
  Tcl& tcl = Tcl::instance();

  if (print_info_)
    cout << "At " << NOW << " MIPv6 agent in node " << MYNUM 
	 << " received client going down\n";

  //we check if we find the structure 
  for (int i=0; i < (int)list_nodes.size() ; i++) {
    tcl.evalf ("%s get-mac-by-addr %d", list_nodes.at(i)->node->name(),macSrc);
    if (tcl.result()!="") {
      list_node *tmp = list_nodes.at (i);
      printf ("Found node %s to remove\n", Address::instance().print_nodeaddr(list_nodes.at(i)->node->address()));
      list_nodes.erase (list_nodes.begin()+i);
      free (tmp);
      break; 
    }
  }
  //Nist - infocom
  //fprintf(Mac802_11::finfocom, "%f - %i - Deconnexion\n", NOW, macSrc);

}
*/

/*
 * Process a new prefix entry
 * @param data The new prefix information 
 */
void MIPV6Agent::process_new_prefix (new_prefix* data)
{
  //to be defined by subclass
  free (data);
}

/*
 * Process an expired prefix
 * @param data The information about the prefix that expired
 */
void MIPV6Agent::process_exp_prefix (exp_prefix* data)
{
  //to be defined by subclass
  free (data);
}

