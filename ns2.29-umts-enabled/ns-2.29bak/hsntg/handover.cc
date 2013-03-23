/*
 * Superclass for all Handover modules.
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
 * rouil - 2005/05/20 - 1.0     - Source created<BR>
 * <BR>
 * <BR>
 * </PRE><P>
 */

#include "handover.h"
#include "if-mngmt.h"

static class HandoverClass : public TclClass {
public:
  HandoverClass() : TclClass("Agent/MIHUser/IFMNGMT/MIPV6/Handover") {}
  TclObject* create(int, const char*const*) {
    return (new Handover());
  }
} class_handover;


/*
 * Creates a handover module
 */
Handover::Handover() : MIPV6Agent ()
{
  bind ("detectionTimeout_", &detectionTimeout_);

  regTimer_ = new RegistrationTimer (this);
  regTimer_->sched (REGISTRATION_BACKOFF);

  detectionTimer_ = new DetectionTimer (this);
}

/*
 * Register with MIH to receive events from local MACs
 */
void Handover::register_mih ()
{
  //will be defined by subclass
}

/*
 * Handle the registration of remote events
 * The layer 3 management notifies the handover that a new
 * remote interface can be accessed. The handover decides what
 * events to receive from this interface
 */
void Handover::register_remote_mih (Mac *mac)
{
  //will be defined by subclass
}

/*
 * Process a no link detected event
 * @param e The event information
 */
void Handover::process_no_link_detected (void *e)
{
  //to be defined by subclass
  free (e);
}

/*
 * Process a scan response
 * @param e The event information
 */
void Handover::process_scan_response (mih_scan_response_s *e)
{
  //to be defined by subclass
  free (e);
}

/*
 * Process a poll response
 * @param data The poll response
 */
void Handover::process_get_status_response (mih_get_status_s* data)
{
  //to be defined by subclass
  free (data);
}

/*
 * Request information about the given Mac
 * @param mac The Mac we need the information
 * @return the Mac configuration
 */
mih_get_status_s * Handover::get_mac_info (int mac)
{
  //request status of all parameters
  u_int32_t param = 0xFF; //all 8 parameters currently defined
  
  return mih_->mih_get_status (this, mac, param);
}

/*
 * Copmute the new address using the new prefix received
 * @param prefix The new prefix received
 * @param interface The interface to update
 */
int Handover::compute_new_address (int prefix, Node *interface)
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
