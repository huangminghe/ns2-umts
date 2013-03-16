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

#include "connectionmanager.h"
#include "mac802_16.h"

/**
 * Create the manager for the given mac
 * @param mac The Mac where the manager is located
 */
ConnectionManager::ConnectionManager (Mac802_16 * mac) 
{
  assert (mac!=NULL);
  mac_ = mac;

  //init list
  LIST_INIT (&up_con_list_);
  LIST_INIT (&down_con_list_);
}


/**
 * Add a connection to the list
 * @param con The connection to add
 * @param incoming true if it is an uplink connection
 */
void ConnectionManager::add_connection (Connection* con, bool uplink) {
  assert (con!=NULL);
  assert (!get_connection (con->get_cid(), uplink)); //check duplicate
  mac_->debug ("At %f in %d adding %s connection %d\n", \
	       NOW, mac_->addr(), uplink?"uplink":"downlink", con->get_cid());
  if (uplink)
    con->insert_entry (&up_con_list_);
  else
    con->insert_entry (&down_con_list_);

  con->setManager(this);
}

/**
 * Remove a connection
 * @param The connection to remove
 */
void ConnectionManager::remove_connection (Connection* con) {
  assert (con !=NULL);
  mac_->debug ("At %f in %d removing connection %d\n", \
	       NOW, mac_->addr(), con->get_cid());
  con->remove_entry ();
}

/**
 * Remove connection by CID, both directions.
 * @param cid The connection id
 */
void ConnectionManager::remove_connection (int cid)
{
  Connection *con = get_connection (cid, true);
  if (con)
    remove_connection (con);
  con = get_connection (cid, false);
  if (con)
    remove_connection (con);
}
  

/**
 * Return the connection that has the given CID
 * @param cid The connection ID
 * @param uplink The direction
 * @return the connection or NULL
 */
Connection* ConnectionManager::get_connection (int cid, bool uplink) {
  //search throught the list
  for (Connection *n=uplink?up_con_list_.lh_first:down_con_list_.lh_first; 
       n; n=n->next_entry()) {
    if (n->get_cid ()==cid)
      return n;
  }
  return NULL;
}

/**
 * Flush the queues. This can be called after switching BS.
 */
void ConnectionManager::flush_queues () {
  mac_->debug ("At %f in %d Flushing queues\n", NOW, mac_->addr());
  for (Connection *n=down_con_list_.lh_first; n; n=n->next_entry()) {
    int i = n->flush_queue();
    mac_->debug ("\tFreed %d packet in queue for connection %d\n", i, n->get_cid());
  }
}

