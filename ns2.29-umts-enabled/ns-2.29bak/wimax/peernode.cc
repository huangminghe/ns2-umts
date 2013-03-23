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

#include "peernode.h"

/**
 * Constructor
 * @param index The Mac address of the peer node
 */
PeerNode::PeerNode (int index): basic_(0), primary_(0), secondary_(0), indata_(0),
				outdata_(0), rxtime_ (0.0), rxp_watch_()
{
  peerIndex_ = index;
  going_down_ = false;
}
    
/**
 * Set the basic connection
 * @param connection The basic connection
 */
void PeerNode::setBasic (Connection* connection ) 
{
  assert (connection != NULL);

  basic_ = connection;
  connection->set_category (CONN_BASIC);
  connection->setPeerNode (this);
}

/**
 * Set the primary connection
 * @param connection The primary connection
 */
void PeerNode::setPrimary (Connection* connection ) 
{
  assert (connection != NULL);

  primary_ = connection;
  connection->set_category (CONN_PRIMARY);
  connection->setPeerNode (this);
}

/**
 * Set the secondary connection
 * @param connection The secondary connection
 */
void PeerNode::setSecondary (Connection* connection ) 
{
  assert (connection != NULL);
  
  secondary_ = connection;
  connection->set_category (CONN_SECONDARY);
  connection->setPeerNode (this);
}

/**
 * Set the incoming data connection
 * @param connection The connection
 */
void PeerNode::setInData (Connection* connection ) 
{
  assert (connection != NULL);
  
  indata_ = connection;
  connection->set_category (CONN_DATA);
  connection->setPeerNode (this);
}

/**
 * Set the outgoing data connection
 * @param connection The connection
 */
void PeerNode::setOutData (Connection* connection ) 
{
  assert (connection != NULL);
  
  outdata_ = connection;
  connection->set_category (CONN_DATA);
  connection->setPeerNode (this);
}

/**
 * Set the time the last packet was received
 * @param time The time the last packet was received
 */
void PeerNode::setRxTime (double time)
{
  assert (time >=0.0);
  rxtime_ = time;
}

/**
 * Get the time the last packet was received
 * @return The time the last packet was received
   */
double PeerNode::getRxTime ()
{
  return rxtime_;
}

/**
 * Return the stat watch
 * @return The stat watch
 */
StatWatch * PeerNode::getStatWatch()
{
  return &rxp_watch_;
}
