/*
 * Implementation of neighbor discovery protocol
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

#include "nd.h"
#include "if-mngmt.h"

#define MYNUM	Address::instance().print_nodeaddr(addr())

/*
 * Packet definitions
 */
int hdr_rtads::offset_;
static class RTADSHeaderClass : public PacketHeaderClass {
public:
	RTADSHeaderClass() : PacketHeaderClass("PacketHeader/RTADS",
					     sizeof(hdr_rtads)) {
	    bind_offset(&hdr_rtads::offset_);
        }
} class_rtadshdr;

int hdr_rtsol::offset_;
static class RTSOLHeaderClass : public PacketHeaderClass {
public:
	RTSOLHeaderClass() : PacketHeaderClass("PacketHeader/RTSOL",
					     sizeof(hdr_rtsol)) {
	    bind_offset(&hdr_rtsol::offset_);
        }
} class_rtsolhdr;


/*
 * Neighbor list utilities
 */

/* search if an entry already exists for the given address 
 * @param prefix The prefix we are looking for
 * @param n Pointer to the head of the list.
 * @return The entry containing the prefix or NULL
 */
Entry* NDAgent::lookup_entry(int prefix, Entry *n)
{
   for ( ; n ; n=n->next_entry()) {
       if(n->prefix_ == prefix)
             return n;
   }
   return NULL;
}

/*
 * Add a entry in the neighbor list
 * @param prefix The prefix to add
 * @param lifetime The prefix's lifetime
 * @return The newly created entry
 */
Entry* NDAgent::add_prefix(int prefix, double lifetime)
{
   // Check if an entry does not already exist
   Entry *node = lookup_entry(prefix, head(nlist_head_));
   
   assert (node == NULL);

   node = new Entry(prefix, NOW, lifetime, this);
   node->insert_entry(&nlist_head_);
   return node;
}

/** End of Neighbor list utilities **/

/*
 * Static constructor for TCL
 */
static class NDAgentClass : public TclClass {
public:
	NDAgentClass() : TclClass("Agent/ND") {}
	TclObject* create(int, const char*const*) {
		return (new NDAgent());
	}
} class_ndagent;


/*
 * Creates a neighbor discovery agent
 * Initializes the agent and bind variable to be accessible in TCL
 */
NDAgent::NDAgent() : Agent(PT_UDP), timer_ (this), rsTimer_ (this) , iMngmnt_(NULL)
{
  lastRA_time_ = 0; 
  nextRA_time_ = -1;
  nb_rtr_sol_ = 0; 

  LIST_INIT(&nlist_head_);
  LIST_INIT(&ra_daddrs_);
  LIST_INIT(&rs_daddrs_);
  LIST_INIT(&rs_list_head_);
	    
  //bind tcl attributes
  bind("router_", &router_);
  bind("useAdvInterval_", &useAdvInterval_);
  bind("router_lifetime_", &rtrlftm_);
  bind("prefix_lifetime_", &prefix_lifetime_);
  bind("minRtrAdvInterval_", &minRtrAdvInterval_);
  bind("maxRtrAdvInterval_", &maxRtrAdvInterval_);
  bind("minDelayBetweenRA_", &minDelayBetweenRA_);
  bind("maxRADelay_", &maxRADelay_);
  bind("broadcast_", &broadcast_);
  bind("default_port_", &default_port_);
  bind("rs_timeout_", &rs_timeout_);
  bind("max_rtr_solicitation_", &max_rtr_solicitation_);
}

/*
 * Register the given interface module
 * @param ifagent Point to the Interface manager that should receive the 
 * triggers
 */
void NDAgent::register_ifmodule (IFMNGMTAgent *ifagent)
{
  assert (ifagent);

  iMngmnt_ = ifagent;
}

/*
 * Implements actions for Timer timeout
 * @param value Information on the timer that expired. if value>0 then it is the
 *              value of an expired prefix.
 */
void NDAgent::timeout(int value)
{
  if (value == IPv6_TIMER_RA) {
    lastRA_time_ = NOW;
    //get relative time for next RA
    nextRA_time_ = Random::uniform(minRtrAdvInterval_, maxRtrAdvInterval_);
    timer_.resched(nextRA_time_);
    nextRA_time_ += NOW; //we set to absolute time
    if (broadcast_)
      send_ads(IP_BROADCAST);
    else
      { //send for each target in the ra_daddrs
	Target *h = ra_daddrs_.lh_first;
	for ( ; h ; h=h->next_entry()) {
	  send_ads(h->destination());
	}
      }
  }
  else if (value == IPv6_TIMER_RS) {
    if (++nb_rtr_sol_ < max_rtr_solicitation_) {
      debug ("At %f in %s ND module resend RS (nb_tries=%d)\n", NOW, MYNUM, nb_rtr_sol_);
      send_rs ();
    } else {
      debug ("At %f in %s ND module max retry for RS (nb_tries=%d)\n", NOW, MYNUM, nb_rtr_sol_);
      nb_rtr_sol_ = 0;
    }
  }
  else {
    //this is a prefix timeout
    Entry *n = lookup_entry (value,head(nlist_head_)); 
    debug ("At %f in %s ND module has prefix %s expired \n", NOW, MYNUM,Address::instance().print_nodeaddr(value));
    n->remove_entry();
    //notify interface manager
    if (iMngmnt_){
      exp_prefix *data = (exp_prefix*)malloc (sizeof(new_prefix));
      Tcl& tcl= Tcl::instance();
      tcl.evalf ("%s set node_", this->name());
      data->prefix=value;
      data->interface=(Node *) TclObject::lookup(tcl.result());
      iMngmnt_->process_nd_event (ND_PREFIX_EXPIRED, data);
    }
  }
}

/* 
 * Interface with TCL interpreter
 * @param argc The number of elements in argv
 * @param argv The list of arguments
 * @return TCL_OK if everything went well else TCL_ERROR
 */
int NDAgent::command(int argc, const char*const* argv)
{
  if (argc == 2) {
    // starts sending RAs
    if (strcmp(argv[1], "start-ra") == 0) {
      if (router_){
	//get relative time of next RA
	nextRA_time_ = Random::uniform(minRtrAdvInterval_, maxRtrAdvInterval_);
	timer_.resched(nextRA_time_);
	nextRA_time_ += NOW; //here we actually set the absolute time for next RA
	return TCL_OK;
      }
      cout << "Error: Only routers can send RA. Use command set-router" \
	   << " TRUE to activate";
      return TCL_ERROR;
    }
    // stops sending RAs
    else if (strcmp(argv[1], "stop-ra") == 0) {
      if (router_)
	{
	  timer_.cancel();
	  nextRA_time_ = -1; //we invalidate this entry
	}
      return TCL_OK;
    }
    // sends a RS
    else if (strcmp(argv[1], "send-rs") == 0) {
      if (!router_) 
	{
	  send_rs();
	  return TCL_OK;
	}
      cout << "Error: a router cannot send RS..."; 
      return TCL_ERROR;
    }
    // dump the neighbor list
    else if (strcmp(argv[1], "dump-table") == 0) {
      dump_table ();
      return TCL_OK;
    }
  }
  if (argc == 3) {
    // set the Minimum interval between two RAs
    if (strcmp(argv[1], "set-minRtrAdv") == 0) {
      minRtrAdvInterval_ = atof(argv[2]);
      return TCL_OK;
    }
    // set the Maximum interval between two RAs
    if (strcmp(argv[1], "set-maxRtrAdv") == 0) {
      maxRtrAdvInterval_ = atof(argv[2]);
      return TCL_OK;
    }
    // set the router lifetime to include in the RAs
    if (strcmp(argv[1], "router-lifetime") == 0) {
      rtrlftm_ = atof(argv[2]);
      return TCL_OK;
    }
    // set the prefix lifetime to include in the RAs
    if (strcmp(argv[1], "prefix-lifetime") == 0) {
      prefix_lifetime_ = atoi(argv[2]);
      return TCL_OK;
    }
    // set the node as a router or host
    if (strcmp(argv[1], "set-router") == 0) {
      if (strcmp(argv[2], "TRUE") == 0) 
	router_ = 1;
      else 
	router_ = 0;
      return TCL_OK;  
    }
    // set the interface manager 
    if (strcmp(argv[1], "set-ifmanager") == 0) {
      iMngmnt_ = (IFMNGMTAgent *) TclObject::lookup(argv[2]);
      if (iMngmnt_ == 0)
	return TCL_ERROR;
      return TCL_OK;
    }
    // enable/disable sending broadcast
    if (strcmp(argv[1], "enable-broadcast") == 0) {
      if (strcmp(argv[2], "TRUE") == 0) 
	broadcast_ = 1;
      else 
	broadcast_ = 0;
      return TCL_OK;  
    }
    // add a target for RAs
    if (strcmp(argv[1], "add-ra-target") == 0) {
      if ( add_ra_target (Address::instance().str2addr(argv[2])) )
	return TCL_OK;
      return TCL_ERROR;
    }
    // remove a target for RAs
    if (strcmp(argv[1], "remove-ra-target") == 0) {
      if ( remove_ra_target (Address::instance().str2addr(argv[2])) )
	return TCL_OK;
      return TCL_ERROR;
    }
    // add a target for RSs
    if (strcmp(argv[1], "add-rs-target") == 0) {
      if ( add_rs_target (Address::instance().str2addr(argv[2])) )
	return TCL_OK;
      return TCL_ERROR;
    }
    // remove a target for RSs
    if (strcmp(argv[1], "remove-rs-target") == 0) {
      if ( remove_rs_target (Address::instance().str2addr(argv[2])) )
	return TCL_OK;
      return TCL_ERROR;
    }
  }
  return (Agent::command(argc, argv));
}

/* 
 * Router advertisement are always broadcast packets
 * @param daddr The destination of the packet (IP_BROADCAST or unicast)
 */ 
void NDAgent::send_ads( int daddr)
{
  Packet *p = allocpkt();
  hdr_ip *iph = HDR_IP(p);
  hdr_rtads *rh = HDR_RTADS(p);
  hdr_cmn *hdrc = HDR_CMN(p);
  
  //MAC 802.11 allows ip broadcast but not ethernet.TBD
  iph->daddr() = daddr;
  iph->dport() = port();
  hdrc->ptype() = PT_RADS;
  hdrc->size() = PT_RADS_SIZE;
  
  debug ("At %f in %s ND module send RA\n", NOW, MYNUM);

  rh->router_lifetime () = rtrlftm_;
  rh->valid_lifetime () = prefix_lifetime_;
  rh->preferred_lifetime () = prefix_lifetime_;
  rh->prefix() = here_.addr_;
  rh->advertisement_interval() = (uint32_t) (maxRtrAdvInterval_*1000); //convert to ms
   
  send(p, 0);
}

/*
 * Sends a Router Solicitation
 */
void NDAgent::send_rs()
{
  if (!router_) {
    if (rsTimer_.status()==TIMER_PENDING) {
      debug ("At %f in %s ND module RS already waiting for response\n", NOW, MYNUM);
    } else {
      if (broadcast_)
	send_rs(IP_BROADCAST);
      else
	{ //send for each target in the rs_daddrs
	  Target *h = rs_daddrs_.lh_first;
	  for ( ; h ; h=h->next_entry()) {
	    send_rs(h->destination());
	  }
	}
      rsTimer_.resched (rs_timeout_);
    }
  } else {
    debug ("At %f in %s ND module Error: a router cannot send RS\n", NOW, MYNUM);
  }
}

/*
 * Sends a Router Solicitation 
 * @param daddr The destination of the RS message
 */
void NDAgent::send_rs(int daddr)
{
  //in case the node has changed address, reassign it
  Tcl& tcl = Tcl::instance();
  tcl.evalf("%s set node_", name());
  Node *node = (Node*) TclObject::lookup(tcl.result());
  if (addr()!= node->address()) {
    //printf ("Node address has changed. Old=%d new=%d\n", addr(), node->address());
    here_.addr_ = node->address();
  }

  Packet *p = allocpkt();
  hdr_ip *iph = HDR_IP(p);
  hdr_cmn *hdrc = HDR_CMN(p);

  iph->daddr() = daddr;  
  iph->dport() = port();
  hdrc->ptype() = PT_RSOL;
  hdrc->size() = PT_RSOL_SIZE;

  debug ("At %f in %s ND module send RS\n", NOW, MYNUM);
  send(p, 0);
}

/* 
 * Process received packet
 * @param p The packet received
 * @param h The handler that sent the packet
 */
void NDAgent::recv(Packet* p, Handler *h)
{
  hdr_cmn *hdrc = HDR_CMN(p);
   
  if (hdrc->ptype() == PT_RADS)
    {
      if (!router_)
	{
	  // we are in host mode, update information
	  recv_ads(p);
	}
      //else router mode. Silently discard RA packet.
    }
  if (hdrc->ptype() == PT_RSOL)
    {
      if (router_)
	{
	  // we are in router mode, answer by sending a RA 
	  recv_rs (p);
	}
      //else host mode. Silently discard RS packet.
    }
  Packet::free (p);
}

/*
 * Process router advertisements
 * @param p The received RA
 */
void NDAgent::recv_ads(Packet *p)
{
  hdr_rtads *rh = HDR_RTADS(p);
  hdr_ip *iph = HDR_IP(p);
  double timer_lifetime;

  assert (!router_); //only hosts can process RA

  debug ("At %f in %s ND module received RA from node %s\n", NOW, MYNUM, \
	 Address::instance().print_nodeaddr(iph->saddr()));

  debug ("\trouter-lifetime=%f s\n\tprefix valid_lifetime=%d s\n\tprefix=%s\n\tadvertisement interval=%d ms\n",\
	 rh->router_lifetime (), rh->valid_lifetime (), Address::instance().print_nodeaddr(rh->prefix()), rh->advertisement_interval());
 
  //see if we were expecting the answer
  if (rsTimer_.status()!=TIMER_IDLE)
    rsTimer_.cancel();

  //we want to allow a maximum of 2 packets lost and we convert to seconds
  timer_lifetime = ((double)rh->advertisement_interval())*0.003;

  // We search in list of neighbors to check if we know about this prefix
  Entry *node = lookup_entry(rh->prefix(), head(nlist_head_));
  if ( node ) { 
    // We already have this prefix in the list.  
    // Update information
    debug ("\t-> Update lifetime \n");
    if (useAdvInterval_)
      node->update_entry(NOW, timer_lifetime);
    else
      node->update_entry(NOW, rh->router_lifetime());
  } else { 
    // New RA.  This prefix is not in the list yet
    debug ("\t-> New neighbor\n");     
    if (useAdvInterval_)
      add_prefix(rh->prefix(), timer_lifetime);
    else
      add_prefix(rh->prefix(), rh->router_lifetime());

    //notify interface manager
    if (iMngmnt_){
      new_prefix *data = (new_prefix*)malloc (sizeof(new_prefix));
      Tcl& tcl= Tcl::instance();
      tcl.evalf ("%s set node_", this->name());
      data->interface=(Node *) TclObject::lookup(tcl.result());
      data->prefix=rh->prefix();
      iMngmnt_->process_nd_event (ND_NEW_PREFIX, data);
    }
  }
}

/*
 * Process router solicitation
 * @param p The received RS
 */
void NDAgent::recv_rs(Packet *p)
{
  hdr_ip *iph = HDR_IP(p);
  //hdr_rtsol *rh = HDR_RSOL(p);
  double rs_delay;
  RSreplyTimer *th;
  RSReplyEntry *ent;

  assert (router_); //only routers can process RS

  debug ("At %f in %s ND module received RS from node %s\n", NOW, MYNUM, \
	 Address::instance().print_nodeaddr(iph->saddr()));
 
  //check when is the next scheduled RA then send. 
  rs_delay = Random::uniform(0, maxRADelay_);
  if (nextRA_time_ != -1 && nextRA_time_ - NOW < rs_delay) {
    debug ("\tScheduled answer at %f but next RA very soon (%f), we don't send RA.\n",
	   NOW+rs_delay, nextRA_time_);
    return; // we will send a RA pretty soon, so don't bother replying.
  }
  if ( NOW + rs_delay - lastRA_time_ < minDelayBetweenRA_) {
    //check if the answers if after the next RA
    if (nextRA_time_ != -1 && nextRA_time_ > NOW && nextRA_time_ - NOW < rs_delay + minDelayBetweenRA_)
      {
	debug ("\tScheduled answer at %f but next RA very soon (%f), we don't send RA.\n",
	       NOW+rs_delay+ minDelayBetweenRA_, nextRA_time_);
	return; // we will send a RA pretty soon, so don't bother replying.
      }
    //we should delay the answer of rs_delay + (lastRA_time_+ minDelayBetweenRA_ - NOW)
    th = new RSreplyTimer (this, iph->saddr());
    th->sched (rs_delay + lastRA_time_+ minDelayBetweenRA_ - NOW);
    ent = new RSReplyEntry (th);
    ent->insert_entry (&rs_list_head_);
    debug ("\tPrevious RA just occured, we delay our answer.\n");
  }
  else {
    th = new RSreplyTimer (this, iph->saddr());
    th->sched (rs_delay);
    ent = new RSReplyEntry (th);
    ent->insert_entry (&rs_list_head_);
  }
  //else next RA is too soon, we just wait.
}

/*
 * Process an RS reply timeout
 * @param th The timer that expired containing RS information
 */
void NDAgent::process_rsreply (RSreplyTimer *th)
{
  debug ("At %f in %s ND module processes RS reply.\n", NOW, MYNUM);

  //send the reply
  //If we are doing a handover, the new node may be in
  //the old address space, thus we need to send it broadcast.
  //send_ads(th->address());

  if (broadcast_)
    send_ads(IP_BROADCAST);
  else
    { //for unicast (Ethernet, UMTS) we don't have handover yet
      //so we can just reply to the node that sent the RS
      send_ads(th->address());
      /*
      Target *h = ra_daddrs_.lh_first;
      for ( ; h ; h=h->next_entry()) {
	debug("Sending RA to %d\n", h->destination());
	send_ads(h->destination());
      }
      */
    }

  //remove the timer
  RSReplyEntry *ent = rs_list_head_.lh_first;
  for ( ; ent ; ent=ent->next_entry()) {
    if (ent->timer() == th){
      ent->remove_entry();
      break;
    }
  }
}

/*
 * Add an entry in the list of RA targets
 * @param daddr The address of target
 * @return 1 if no error, 0 if any
 */
int NDAgent::add_ra_target (int daddr)
{
  //check if already present
  Target *h = ra_daddrs_.lh_first;
  for ( ; h ; h=h->next_entry()) {
    if (h->destination() == daddr) {
      debug ("At %f in %s ND module add RA target %s already in list.\n", \
	     NOW, MYNUM, Address::instance().print_nodeaddr(daddr));
      return 0; //it is already in the vector
    }
  }

  h = new Target(daddr);
  h->insert_entry(&ra_daddrs_);

  debug ("At %f in %s ND module added RA target %s\n", \
	 NOW, MYNUM, Address::instance().print_nodeaddr(daddr));
  return 1;
}

/*
 * Remove an entry in the list of RA targets
 * @param daddr The address of target
 * @return 1 if no error, 0 if any
 */
int NDAgent::remove_ra_target (int daddr)
{
  //check if already present
  Target *h = ra_daddrs_.lh_first;
  for ( ; h ; h=h->next_entry()) {
    if (h->destination() == daddr) {
      h->remove_entry ();
      debug ("At %f in %s ND module removed RA target %s\n", \
	     NOW, MYNUM, Address::instance().print_nodeaddr(daddr));
      return 1; 
    }
  }
  debug ("At %f in %s ND module RA target %s not present in list. Cannot be removed\n",\
	 NOW, MYNUM, Address::instance().print_nodeaddr(daddr));
  return 0;
}

/*
 * Add an entry in the list of RS targets
 * @param daddr The address of target
 * @return 1 if no error, 0 if any
 */
int NDAgent::add_rs_target (int daddr)
{
  //check if already present
  Target *h = rs_daddrs_.lh_first;
  for ( ; h ; h=h->next_entry()) {
    if (h->destination() == daddr) {
      debug ("At %f in %s ND module add RS target %s already in list.\n", \
	     NOW, MYNUM, Address::instance().print_nodeaddr(daddr));
      return 0; //it is already in the vector
    }
  }

  h = new Target(daddr);
  h->insert_entry(&rs_daddrs_);

  debug ("At %f in %s ND module added RS target %s\n", \
	 NOW, MYNUM, Address::instance().print_nodeaddr(daddr));
  return 1;
}

/*
 * Remove an entry in the list of RS targets
 * @param daddr The address of target
 * @return 1 if no error, 0 if any
 */
int NDAgent::remove_rs_target (int daddr)
{
  //check if already present
  Target *h = rs_daddrs_.lh_first;
  for ( ; h ; h=h->next_entry()) {
    if (h->destination() == daddr) {
      h->remove_entry ();
      debug ("At %f in %s ND module removed RS target %s\n", \
	     NOW, MYNUM, Address::instance().print_nodeaddr(daddr));
      return 1; 
    }
  }
  debug ("At %f in %s ND module RS target %s not present in list. Cannot be removed\n",\
	 NOW, MYNUM, Address::instance().print_nodeaddr(daddr));
  return 0;
}

/*
 * Dump the neighbor table 
 */
void NDAgent::dump_table ()
{
  Entry *n = head(nlist_head_);
  
  cout << "BEGIN Neighbor table in node "<< MYNUM <<" at time " << NOW <<"\n";
  for ( ; n ; n=n->next_entry()) {
    cout << " Prefix: "<< Address::instance().print_nodeaddr(n->prefix())<<" lifetime="<<n->lifetime()<<"\n";
  } 
  cout << "END Neighbor table\n";
}


