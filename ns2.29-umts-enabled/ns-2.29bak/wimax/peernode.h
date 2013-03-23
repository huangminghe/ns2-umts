/* This software was developed at the National Institute of Standards and
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
 */

#ifndef PEERNODE_H
#define PEERNODE_H

#include "connection.h"
#include "mac-stats.h"

class PeerNode;
LIST_HEAD (peerNode, PeerNode);
/**
 * Class PeerNode
 * Supports list
 */ 
class PeerNode {

public:

  /**
   * Constructor
   * @param index The Mac address of the peer node
   */
  PeerNode (int index);
  
  /**
   * Return the address of the peer node
   * @return The address of the peer node
   */
  int getPeerNode () { return peerIndex_; }

  /**
   * Set the connection for delay-intolerant management messages
   * @param connection The connection used as basic 
   */
  void  setBasic (Connection * connection);
  
  /**
   * Return the connection used for delay-intolerant messages
   */
  Connection*  getBasic () { return basic_; }
  
  /**
   * Set the connection for delay-tolerant management messages
   * @param connection 
   */
  void  setPrimary (Connection * connection);
  
  /**
   * Return the connection used for delay-tolerant messages
   */
  inline Connection*  getPrimary () { return primary_; }
  
  /**
   * Set the channel used for standard-based messages
   * @param connection 
   */
  void  setSecondary (Connection * connection);
  
  /**
   * Return the connection used for standard-based messages
   */
  Connection*  getSecondary () { return secondary_; }

  /**
   * Set the channel used for data messages
   * @param connection 
   */
  void  setInData (Connection * connection);
  
  /**
   * Set the channel used for data messages
   * @param connection 
   */
  void  setOutData (Connection * connection);
  
  /**
   * Return the connection used for data messages
   */
  Connection*  getOutData () { return outdata_; }

  /**
   * Return the connection used for data messages
   */
  Connection*  getInData () { return indata_; }

  /**
   * Set the time the last packet was received
   * @param time The time the last packet was received
   */
  void setRxTime (double time);

  /**
   * Get the time the last packet was received
   * @return The time the last packet was received
   */
  double getRxTime ();  

  /**
   * Return the stat watch
   * @return The stat watch
   */
  StatWatch * getStatWatch();

  /**
   * Return true if the peer is going down
   * @return true if the peer is going down
   */
  inline bool isGoingDown () { return going_down_; }

  /**
   * Set the status of going down
   * @param status The link going down status
   */
  inline void setGoingDown (bool status) { going_down_ = status; }

  // Chain element to the list
  inline void insert_entry(struct peerNode *head) {
    LIST_INSERT_HEAD(head, this, link);
  }
  
  // Return next element in the chained list
  PeerNode* next_entry(void) const { return link.le_next; }

  // Remove the entry from the list
  inline void remove_entry() { 
    LIST_REMOVE(this, link); 
  }  
protected:

  /*
   * Pointer to next in the list
   */
  LIST_ENTRY(PeerNode) link;
  //LIST_ENTRY(PeerNode); //for magic draw

private:
  /**
   * Mac address of peer node
   */
  int peerIndex_;

  /**
   * Used to send delay intolerant management messages
   */
   Connection* basic_;
  /**
   * Used to send delay tolerant mac messages
   */
   Connection* primary_;
  /**
   * Used to transport standard-based protocol (DHCP...)
   */
   Connection* secondary_;
  /**
   * Incomfing data connection to this client
   */
   Connection* indata_;  
   
  /**
   * Outgoing data connection to this client
   */
   Connection* outdata_;  
   
   /**
    * Time last packet was received for this peer
    */
   double rxtime_;

   /**
    * Received signal strength stats
    */
   StatWatch rxp_watch_;

   /**
    * Inidicate the link going down status of the peer node
    */
   bool going_down_;

};
#endif //PEERNODE_H

