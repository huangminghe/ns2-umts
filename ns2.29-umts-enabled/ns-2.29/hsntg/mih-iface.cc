/* Implementation of Media Independant Handover Function
 *
 * This software was developed at the National Institute of Standards and
 * Technology by employees of the Federal Government in the course of
 * their official duties. Pursuant to title 17 Section 105 of the United
 * States Code this software is not subject to copyright protection and
 * islink_ in the public domain.
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

#include "mih-iface.h"

/**
 * Create information for the given MAC
 * Note: The MAC is local
 * @param mac The pointer to local MAC
 */
MIHInterfaceInfo::MIHInterfaceInfo (Mac *mac)
{
  mac_ = mac;
  type_ = mac->getType();
  macAddr_ = mac->addr();
  macPoA_ = -1; //default init
  status_ = LINK_STATUS_DOWN;
  //get Link capabilities. We assume it does not change over time
  capability_ = mac->link_capability_discover ();
}

/**
 * Create information for the given remote MAC
 * @param macAddr address of remote MAC
 * @param status status information about remote MAC
 */
MIHInterfaceInfo::MIHInterfaceInfo (link_identifier_t id)
{
  mac_ = NULL;
  type_= id.type;
  macAddr_ = id.macMobileTerminal;
  macPoA_ = id.macPoA;
  status_ = LINK_STATUS_DOWN;
}

/**
 * Return if the MAC is local or not
 * @return true if the MAC is local
 */
bool MIHInterfaceInfo::isLocal ()
{
  return (mac_ != NULL);
}

/**
 * Update the PoA information
 * @param macPoA MAC address of PoA
 */
void MIHInterfaceInfo::setMacPoA (int macPoA)
{
  macPoA_ = macPoA;
}

/**
 * Set the status of the interface
 * @param status The new status
 */
void MIHInterfaceInfo::setStatus (interface_status_t status)
{
  status_ = status;
}

/**
 * Return the MAC status
 * @return the MAC status
 */
interface_status_t  MIHInterfaceInfo::getStatus ()
{
  return status_;
}

/**
 * Return the MAC type
 * @return the MAC type
 */
link_type_t MIHInterfaceInfo::getType ()
{
  return type_;
}


/**
 * Register upper layer for the given events
 * @param upper The upper layer that wants event notifications
 * @param events List of events to subscribe
 * @return subscription status or NULL if the MAC is remote
 */
void MIHInterfaceInfo::event_subscribe (MIHUserAgent *upper, link_event_list events)
{
  for (int i = 0 ; i < LINK_LAST_EVENT; i++) {
    if ( (events >> i) & 0x1) {
      //double check if remote is not already in the list
      bool duplicate = false;
      for (int j = 0 ; j < (int)local_sub_[i].size() ; j++) {
	if (local_sub_[i].at(j)==upper) {
	  duplicate = true;
	  break; //duplicate
	}
      }
      if (!duplicate)
	local_sub_[i].push_back (upper);
    }
  }
}

/**
 * Unregister upper layer for the given events
 * @param upper The upper layer that wants event notifications
 * @param events List of events to subscribe
 * @return subscription status or NULL if the MAC is remote
 */
void MIHInterfaceInfo::event_unsubscribe (MIHUserAgent *upper, link_event_list events)
{
  for (int i = 0 ; i < LINK_LAST_EVENT; i++) {
    if ( (events >> i) & 0x1) {
      //check list of subscriber
      for (int j=0; j < (int)local_sub_[i].size(); j++) {
	if (local_sub_[i].at(j)==upper) {
	  local_sub_[i].erase (local_sub_[i].begin()+j);
	  break;
	}
      }
    }
  }
}

/**
 * Register upper layer for the given events
 * @param remote The remote session that wants event notifications
 * @param events List of events to subscribe
 * @return subscription status or NULL if the MAC is remote
 */
void MIHInterfaceInfo::remote_event_subscribe (session_info *remote, link_event_list events)
{
  for (int i = 0 ; i < LINK_LAST_EVENT; i++) {
    if ( (events >> i) & 0x1) {
      //double check if remote is not already in the list
      bool duplicate = false;
      for (int j = 0 ; j < (int)remote_sub_[i].size() ; j++) {
	if (remote_sub_[i].at(j)==remote) {
	  duplicate = true;
	  break; //duplicate
	}
      }
      if (!duplicate)
	remote_sub_[i].push_back (remote);
    }
  }
}

/**
 * Unregister upper layer for the given events
 * @param remote The remote session that wants event notifications
 * @param events List of events to subscribe
 * @return subscription status or NULL if the MAC is remote
 */
void MIHInterfaceInfo::remote_event_unsubscribe (session_info *remote, link_event_list events)
{
  for (int i = 0 ; i < LINK_LAST_EVENT; i++) {
    if ( (events >> i) & 0x1) {
      //check list of subscriber
      for (int j=0; j < (int)remote_sub_[i].size(); j++) {
	if (remote_sub_[i].at(j)==remote) {
	  remote_sub_[i].erase (remote_sub_[i].begin()+j);
	  break;
	}
      }
    }
  }
}

/**
 * Return the vector of registered upper layer for the given event
 * @return The vector of registered upper layer for the given event
 */
vector <MIHUserAgent *> * MIHInterfaceInfo::getRegisteredLocalUser (int event)
{
  return &local_sub_[event];
}

/**
 * Return the vector of registered upper layer for the given event
 * @return The vector of registered upper layer for the given event
 */
vector <session_info *> *MIHInterfaceInfo::getRegisteredRemoteUser (int event)
{
  return &remote_sub_[event];
}


/**
 * Set the session to communicate with the remote MIH
 * @param session The session to use to communicate with remote MIH
 */
void MIHInterfaceInfo::setMIHSession (session_info *session)
{
  assert (session);
  session_ = session;
}

/**
 * Return the session to communicate with the remote MIH
 * @return The session to use to communicate with remote MIH
 */
session_info* MIHInterfaceInfo::getSession ()
{
  return session_;
}

/**
 * Return the MAC of local interface
 * @return The MAC of local interface or NULL
 */
Mac* MIHInterfaceInfo::getMac ()
{
  return mac_;
}

/**
 * Return the MAC address of mobile interface
 * @return The MAC address of mobile interface
 */
int MIHInterfaceInfo::getMacPoA ()
{
  return macPoA_;
}

/**
 * Return the MAC address of mobile interface
 * @return The MAC address of mobile interface
 */
int MIHInterfaceInfo::getMacAddr ()
{
  return macAddr_;
}

/**
 * Return the link capability
 * @return link capability
 */
capability_list_s * MIHInterfaceInfo::getLinkCapability ()
{
  struct capability_list_s * cap = (capability_list_s *) malloc (sizeof (capability_list_s));
  *cap = *capability_;
  return cap;
}
