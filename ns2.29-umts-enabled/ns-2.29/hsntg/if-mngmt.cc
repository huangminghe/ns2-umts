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

#include "if-mngmt.h"
#include "nd.h"
#include "handover.h"

#define MYNUM	Address::instance().print_nodeaddr(addr())

/*
 * Static constructor for TCL
 */
static class IFMNGMTAgentClass : public TclClass {
public:
	IFMNGMTAgentClass() : TclClass("Agent/MIHUser/IFMNGMT") {}
	TclObject* create(int, const char*const*) {
		return (new IFMNGMTAgent());
	}
} class_ifmngmtagent;

/*
 * Creates a neighbor discovery agent
 */
IFMNGMTAgent::IFMNGMTAgent() : MIHUserAgent()
{
    LIST_INIT(&fList_head_);
    LIST_INIT(&nd_mac_);

    //bind tcl attributes
    bind("default_port_", &default_port_);
}

/* 
 * Interface with TCL interpreter
 * @param argc The number of elements in argv
 * @param argv The list of arguments
 * @return TCL_OK if everything went well else TCL_ERROR
 */
int IFMNGMTAgent::command(int argc, const char*const* argv)
{
  if (argc == 3) {
    if (strcmp(argv[1], "connect-mih") == 0) {
      mih_ = (MIHAgent *) TclObject::lookup(argv[2]);
      if (mih_)
	return TCL_OK;
      return TCL_ERROR;
    }
  }
  if (argc == 4) {
    if (strcmp(argv[1], "nd_mac") == 0) {
      NDAgent *nd = (NDAgent *) TclObject::lookup(argv[2]);
      Mac *mac = (Mac *) TclObject::lookup(argv[3]);
      if (nd == NULL || mac == NULL)
	return TCL_ERROR;
      Nd_Mac *h = new Nd_Mac (nd, mac);
      h->insert_entry(&nd_mac_);
      return TCL_OK;
    }
  }
  
  if (argc == 6) {
    // add-flow $local_agent $remote_agent $interface $priority
    if (strcmp(argv[1], "add-flow") == 0) {
       Agent *local = (Agent *) TclObject::lookup(argv[2]);
       Agent *remote = (Agent *) TclObject::lookup(argv[3]);
       Node * iface = (Node *) TclObject::lookup(argv[4]);
       int priority = atoi(argv[5]);
       add_flow (local, remote, iface, priority, 0);
      return TCL_OK;
    }
  }

  if (argc == 7) {
    // add-flow $local_agent $remote_agent $interface $priority $minBw
    if (strcmp(argv[1], "add-flow") == 0) {
      Agent *local = (Agent *) TclObject::lookup(argv[2]);
      Agent *remote = (Agent *) TclObject::lookup(argv[3]);
      Node * iface = (Node *) TclObject::lookup(argv[4]);
      int priority = atoi(argv[5]);
      float minBw = atof (argv[6]);
      add_flow (local, remote, iface, priority, minBw);
      return TCL_OK;
    }
  }
  return (MIHUserAgent::command(argc, argv));
}

/*
 * Get the flow connecting the given agents
 * @param local The agent running locally
 * @param remote The remote agent
 * @return The flow information connecting the agent of null
 */
FlowEntry* IFMNGMTAgent:: get_flow (Agent *local, Agent *remote)
{
  FlowEntry *n = head(fList_head_);
  
  for ( ; n ; n=n->next_entry()) {
    if(n->local()==local && n->remote()==remote)
      return n;
  }
  return NULL;
}

/*
 * Get the flow connecting the given agents
 * @param local The agent running locally
 * @param remote The remote agent
 * @return The flow information connecting the agent of null
 */
vector <FlowEntry*> IFMNGMTAgent:: get_flows ()
{
  FlowEntry *n = head(fList_head_);
  vector <FlowEntry *> res;

  for ( ; n ; n=n->next_entry()) {
    res.push_back (n);
  }

  return res;
}


/*
 * Connect both agent and create a flow information
 * @param local The local agent
 * @param remote The remote agent
 * @param iface The interface to use
 * @param priority The flow priority
 */
void IFMNGMTAgent:: add_flow (Agent *local, Agent *remote, Node *iface, int priority, float minBw)
{
  Tcl& tcl = Tcl::instance();
 
  if (!get_flow (local, remote))
    {
      tcl.evalf ("[Simulator instance] simplex-connect %s %s", local->name(), remote->name());
      tcl.evalf ("%s set dst_addr_ [AddrParams addr2id [%s node-addr]]", remote->name(), iface->name());
      tcl.evalf ("%s set dst_port_ [%s set agent_port_]",remote->name(), local->name());
  
      //add entry
      FlowEntry *new_entry = new FlowEntry(local, remote, priority, minBw, iface);
      new_entry->insert_entry(&fList_head_);
      debug ("At %f in %s Interface Manager : Flow added\n", NOW, MYNUM);
    }
  else
    { 
      debug ("At %f in %s Interface Manager : Warning..a flow already exists between %s and %s\n",\
	     NOW, MYNUM, local->name(), remote->name());
    }
}

/*
 * Remove the flow information connecting the 2 agents
 * @param local The local agent
 * @param remote The remote agent
 * @TODO if add_flow connects the agents, delete_flow should disconnects them??
 */
void IFMNGMTAgent:: delete_flow (Agent *local, Agent *remote)
{
  FlowEntry *n = get_flow (local, remote);
  if (n)
    n->remove_entry ();
}

/* 
 * Process received packet
 * @param p The received packet
 * @param h Packet handler
 */
void IFMNGMTAgent::recv(Packet* p, Handler *h)
{
  debug ("At %f in %s Interface Manager received packet\n", NOW, MYNUM);

  Packet::free (p);
}

/*
 * Process an event received from the ND module.
 * This method must be overwritten by subclasses
 * @param type The event type
 * @param data The data associated with the event
 */
void IFMNGMTAgent::process_nd_event (int type, void* data)
{
  debug ("At %f in %s Interface Manager received ND event\n",NOW, MYNUM);

  free (data);
}

/*
 * Find the ND module in the node where the mac is
 * @param mac The MAC module
 * @return The ND module in the node if it exists or NULL
 */
NDAgent * IFMNGMTAgent::get_nd_by_mac (Mac *mac)
{
  Nd_Mac *h = nd_mac_.lh_first;
  for ( ; h ; h=h->next_entry()) {
    if (h->mac() == mac) {
      return h->nd();
    }
  }
  return NULL;
}

/* Return a Packet 
 * @return A newly allocated packet
 */
Packet* IFMNGMTAgent::getPacket()
{
  Packet *p = allocpkt();
  hdr_ip *iph = HDR_IP(p);

  iph->dport() = port();

  return p;
}
