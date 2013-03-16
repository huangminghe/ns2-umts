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

#include "mih-user.h"

#define MYNUM	Address::instance().print_nodeaddr(addr())

/**
 * Static constructor for TCL
 */
static class MIHUserAgentClass : public TclClass {
public:
	MIHUserAgentClass() : TclClass("Agent/MIHUser") {}
	TclObject* create(int, const char*const*) {
		return (new MIHUserAgent());
	}
} class_mihuseragent;

/**
 * Creates a neighbor discovery agent
 */
MIHUserAgent::MIHUserAgent() : Agent(PT_UDP)
{

}

/** 
 * Interface with TCL interpreter
 * @param argc The number of elements in argv
 * @param argv The list of arguments
 * @return TCL_OK if everything went well else TCL_ERROR
 */
int MIHUserAgent::command(int argc, const char*const* argv)
{
  if (argc == 3) {
    if (strcmp(argv[1], "mih") == 0) {
      mih_ = (MIHAgent *) TclObject::lookup(argv[2]);
      if (mih_)
	return TCL_OK;
      return TCL_ERROR;
    }
  }
  return (Agent::command(argc, argv));
}


/**
 * Return the MIH agent it is connected to
 * @return The MIH agent it is connected to
 */
MIHAgent* MIHUserAgent::get_mih ()
{
  return mih_;
}

/** 
 * Process received packet
 * @param p The received packet
 * @param h Packet handler
 */
void MIHUserAgent::recv(Packet* p, Handler *h)
{
  debug ("At %f in %s MIH User received packet\n", NOW, MYNUM);

  Packet::free (p);
}

/**
 * Dispatch the given event received from the MIH module
 * @param type The event type
 * @param data The data associated with the event
 */
void MIHUserAgent::process_mih_event (mih_event_t type, void* data)
{
  debug ("At %f in %s Interface Manager received MIH event\n",NOW, MYNUM);

  switch (type) {
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
  default:
    debug ("At %f MIHUser Agent in %s received unknown trigger of type %d\n", \
	   NOW, MYNUM, type);
  }
}

/**
 * Process MIH_LINK_UP
 * @param e The Link UP event
 */
void MIHUserAgent::process_link_up (struct link_up_t *e)
{
  //To be implemented by subclass
  debug ("MIHUser does not process MIH_LINK_UP\n");
  free (e);
}

/**
 * Process MIH_LINK_DOWN
 * @param e The Link DOWN event
 */
void MIHUserAgent::process_link_down (struct link_down_t *e)
{
  //To be implemented by subclass
  debug ("MIHUser does not process MIH_LINK_DOWN\n");
  free (e);
}

/**
 * Process MIH_LINK_GOING_DOWN
 * @param e The Link Going Down event
 */
void MIHUserAgent::process_link_going_down (struct link_going_down_t *e)
{
  //To be implemented by subclass
  debug ("MIHUser does not process MIH_LINK_GOING_DOWN\n");
  free (e);
}

/**
 * Process MIH_LINK_DETECTED
 * @param e The Link Detected event
 */
void MIHUserAgent::process_link_detected (struct link_detected_t *e)
{
  //To be implemented by subclass
  debug ("MIHUser does not process  MIH_LINK_DETECTED\n");
  free (e);
}

/**
 * Process MIH_LINK_PARAMETERS_REPORT
 * @param e The Link Parameters report event
 */
void MIHUserAgent::process_link_parameters_report (struct link_parameters_report_t *e)
{
  //To be implemented by subclass
  debug ("MIHUser does not process MIH_LINK_PARAMETERS_REPORT\n");
  free (e);
}

/**
 * Process MIH_LINK_ROLLBACK
 * @param e The Link Rollback event
 */
void MIHUserAgent::process_link_rollback (struct link_rollback_t *e)
{
  //To be implemented by subclass
  debug ("MIHUser does not process MIH_LINK_ROLLBACK\n");
  free (e);
}

/**
 * Process MIH_LINK_HANDOVER_IMMINENT
 * @param e The Link Handover imminent event
 */
void MIHUserAgent::process_link_handover_imminent (struct link_handover_imminent_t *e)
{
  //To be implemented by subclass
  debug ("MIHUser does not process MIH_LINK_HANDOVER_IMMINENT\n");
  free (e);
}

/**
 * Process MIH_LINK_HANDOVER_COMPLETE
 * @param e The Link Handover complete event
 */
void MIHUserAgent::process_link_handover_complete (struct link_handover_complete_t *e)
{
  //To be implemented by subclass
  debug ("MIHUser does not process MIH_LINK_HANDOVER_COMPLETE\n");
  free (e);
}

/**
 * Process MIH_Capability_Discover.confirm
 * @param mihh The MIH packet containing the response
 */
void MIHUserAgent::process_cap_disc_conf (hdr_mih *mihh)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Capability_Discover.confirm\n");
}

/**
 * Process MIH_Register.indication
 * @param mihh MIH packet
 */
void MIHUserAgent::process_register_ind (hdr_mih *mihh)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Register.indication\n");

}

/**
 * Process MIH_Register.confirm
 * @param rsp the result of the registration
 */
void MIHUserAgent::process_register_conf (struct mih_reg_rsp *rsp)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Register.confirm\n");

}

/**
 * Process MIH_DeRegister.indication
 * @param mihh MIH packet
 */
void MIHUserAgent::process_deregister_ind (hdr_mih *mihh)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Deregister.indication\n");

}

/**
 * Process MIH_DeRegister.confirm
 * @param rsp the result of the registration
 */
void MIHUserAgent::process_deregister_conf (struct mih_dereg_rsp *rsp)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Deregister.confirm\n");

}


/**
 * Process MIH_Event_subscribe.confirm
 * @param rsp The result of subscription request
 */
void MIHUserAgent::process_event_sub_conf (struct mih_event_sub_rsp *rsp)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Event_subscribe.confirm\n");
}

/**
 * Process MIH_Event_unsubscribe.confirm
 * @param rsp The result of unsubscription request
 */
void MIHUserAgent::process_event_unsub_conf (struct mih_event_unsub_rsp *rsp)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Event_unsubscribe.confirm\n");
}

/**
 * Process MIH_Get_Status.confirm
 * @param mihh MIH packet
 */
void MIHUserAgent::process_get_status_conf (hdr_mih *mihh)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Get_Status.confirm\n");
}

/**
 * Process MIH_Switch.confirm
 * @param rsp The result to switch request
 */
void MIHUserAgent::process_switch_conf (struct mih_switch_rsp *rsp)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Switch.confirm\n");
}

/**
 * Process MIH_Configure.confirm
 * @param rsp The result to configure request
 */
void MIHUserAgent::process_configure_conf (struct mih_configure_rsp *rsp)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Configure.confirm\n");
}

/**
 * Process MIH_Scan.confirm
 * @param rsp The result to scan request
 */
void MIHUserAgent::process_scan_conf (struct mih_scan_rsp *rsp)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement Process MIH_Scan.confirm\n");
}

/**
 * Process MIH_Handover_Initiate.indication
 * @param ind Indication of handover initiate from remote node
 */
void MIHUserAgent::process_handover_init_ind (struct mih_handover_initiate_req *ind)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Handover_Initiate.indication\n");
}

/**
 * Process MIH_Handover_Initiate.confirm
 * @param rsp The result of handover initiate request
 */
void MIHUserAgent::process_handover_init_conf (struct mih_handover_initiate_res *rsp)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Handover_Initiate.confirm\n");
}

/**
 * Process MIH_Handover_Prepare.indication
 * @param ind Indication of handover prepare from remote node
 */
void MIHUserAgent::process_handover_prepare_ind (struct mih_handover_prepare_req *ind)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Handover_Prepare.indication\n");
}

/**
 * Process MIH_Handover_Prepare.confirm
 * @param rsp The result of handover prepare request
 */
void MIHUserAgent::process_handover_prepare_conf (struct mih_handover_prepare_res *rsp)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Handover_Prepare.confirm\n");
}

/**
 * Process MIH_Handover_Commit.indication
 * @param ind Indication of handover commit from remote node
 */
void MIHUserAgent::process_handover_commit_ind (struct mih_handover_commit_req *ind)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Handover_Commit.indication\n");
}

/**
 * Process MIH_Handover_Commit.confirm
 * @param rsp The result of handover commit request
 */
void MIHUserAgent::process_handover_commit_conf (struct mih_handover_commit_res *rsp)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Handover_Commit.confirm\n");
}

/**
 * Process MIH_Handover_Complete.indication
 * @param ind Indication of handover complete from remote node
 */
void MIHUserAgent::process_handover_complete_ind (struct mih_handover_complete_req *ind)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Handover_Complete.indication\n");
}

/**
 * Process MIH_Handover_Complete.confirm
 * @param rsp The result of handover complete request
 */
void MIHUserAgent::process_handover_complete_conf (struct mih_handover_complete_res *rsp)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Handover_Complete.confirm\n");
}

/**
 * Process MIH_Network_Address_Information.indication
 * @param ind Indication of network address information request
 */
void MIHUserAgent::process_network_info_ind (struct mih_network_address_info_req *ind)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Network_Address_Information.indication\n");
}

/**
 * Process MIH_Network_Address_Information.confirm
 * @param rsp The result of network address information request
 */
void MIHUserAgent::process_network_info_conf (struct mih_network_address_info_res *rsp)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement IH_Network_Address_Information.confirm\n");
}

/**
 * Process MIH_Get_Information.indication
 * @param ind Indication of information request 
 */
void MIHUserAgent::process_get_info_ind (struct mih_get_information_req *ind)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Get_Information.indication\n");
}

/**
 * Process MIH_Get_Information.confirm
 * @param rsp The result of information request
 */
void MIHUserAgent::process_get_info_conf (struct mih_get_information_rsp *rsp)
{
  //To be implemented by subclass
  debug ("MIHUser does not implement MIH_Get_Information.confirm\n");
}
