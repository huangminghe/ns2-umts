/*
 * Include file for Mac information used in MIH database 
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
 * rouil - 2006/11/06 - 1.0     - Source created<BR>
 * <BR>
 * <BR>
 * </PRE><P>
 */

#ifndef mih_iface_h
#define mih_iface_h

#include "mih.h"

/**
 * This class stores information about the MAC interfaces
 */
class MIHInterfaceInfo {
 public: 

  /**
   * Create information for the given MAC
   * Note: The MAC is local
   * @param mac The pointer to local MAC
   */
  MIHInterfaceInfo (Mac *mac);

  /**
   * Create information for the given remote link
   * @param id link identifier of remote link
   * @param status status information about remote link
   */
  MIHInterfaceInfo (link_identifier_t id);
  
  /**
   * Return if the MAC is local or not
   * @return true if the MAC is local
   */
  bool isLocal ();

  /**
   * Update the PoA information
   * @param macPoA MAC address of PoA
   */
  void setMacPoA (int macPoA);

  /**
   * Set the status of the interface
   * @param status The new status
   */
  void setStatus (interface_status_t status);

  /**
   * Return the MAC status
   * @return the MAC status
   */
  interface_status_t getStatus ();

  /**
   * Return the MAC type
   * @return the MAC type
   */
  link_type_t getType ();

  /**
   * Register upper layer for the given events
   * @param upper The upper layer that wants event notifications
   * @param events List of events to subscribe
   * @return subscription status or NULL if the MAC is remote
   */
  void event_subscribe (MIHUserAgent *upper, link_event_list events);

  /**
   * Unregister upper layer for the given events
   * @param upper The upper layer that wants event notifications
   * @param events List of events to subscribe
   * @return subscription status or NULL if the MAC is remote
   */
  void event_unsubscribe (MIHUserAgent *upper, link_event_list events);

  /**
   * Register upper layer for the given events
   * @param remote The remote session that wants event notifications
   * @param events List of events to subscribe
   * @return subscription status or NULL if the MAC is remote
   */
  void remote_event_subscribe (session_info *remote, link_event_list events);

  /**
   * Unregister upper layer for the given events
   * @param remote The remote session that wants event notifications
   * @param events List of events to subscribe
   * @return subscription status or NULL if the MAC is remote
   */
  void remote_event_unsubscribe (session_info *remote, link_event_list events);

  /**
   * Set the session to communicate with the remote MIH
   * @param session The session to use to communicate with remote MIH
   */
  void setMIHSession (session_info *session);

  /**
   * Return the session to communicate with the remote MIH
   * @return The session to use to communicate with remote MIH
   */
  session_info* getSession ();

  /**
   * Return the MAC of local interface
   * @return The MAC of local interface or NULL
   */
  Mac* getMac ();

  /**
   * Return the MAC PoA of local interface
   * @return The MAC PoA of local interface or NULL
   */
  int getMacPoA ();

  /**
   * Return the MAC address of mobile interface
   * @return The MAC address of mobile interface
   */
  int getMacAddr ();

  /**
   * Return the link capability
   * @return link capability
   */
  capability_list_s * getLinkCapability ();
  

  /**
   * Return the vector of registered upper layer for the given event
   * @return The vector of registered upper layer for the given event
   */
  vector <MIHUserAgent *> * getRegisteredLocalUser (int event);

  /**
   * Return the vector of registered upper layer for the given event
   * @return The vector of registered upper layer for the given event
   */
  vector <session_info *> * getRegisteredRemoteUser (int event);  

 private:
  Mac *mac_; //pointer only if local to node
  link_type_t type_;
  int macAddr_;
  int macPoA_;

  capability_list_s* capability_; //event/command supported by MAC layer

  interface_status_t status_; //The status of the interface
  int priority_;               //Priority of the interface

  int prefix_;                 //layer 3 prefix
  
  //interface management modules that will process events.
  vector <MIHUserAgent *> local_sub_[LINK_LAST_EVENT];
  //sessions for remote subscriber (only if MAC is local)
  vector <session_info *> remote_sub_[LINK_LAST_EVENT]; 
  //session ID by which the MAC can be reached (for remote MAC)
  session_info *session_;

};





#endif
