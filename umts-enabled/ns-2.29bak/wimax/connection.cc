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

#include "connection.h"
#include "connectionmanager.h"
#include "mac802_16.h"

static int basicIndex = BASIC_CID_START;
static int primaryIndex = PRIMARY_CID_START;
static int transportIndex = TRANSPORT_SEC_CID_START;
static int multicastIndex = MULTICAST_CID_START;

/**
 * Constructor used by BS to automatically assign CIDs
 * @param type The connection type. 
 */
Connection::Connection (ConnectionType_t type) : peer_(0), 
						 frag_status_(FRAG_NOFRAG), 
						 frag_nb_(0), 
						 frag_byte_proc_(0),
						 frag_enable_(true)
{
   switch (type) {
   case CONN_INIT_RANGING:
     cid_ = INITIAL_RANGING_CID;
     break;
   case CONN_AAS_INIT_RANGING:
     cid_ = AAS_INIT_RANGIN_CID;
     break;
   case CONN_PADDING:
     cid_ = PADDING_CID;
     break;
   case CONN_BROADCAST:
     cid_ = BROADCAST_CID;
     break;
   case CONN_MULTICAST_POLLING:
     cid_ = multicastIndex++;
     assert (multicastIndex <= MULTICAST_CID_STOP);
     break;
   case CONN_BASIC:
     cid_ = basicIndex++;
     assert (basicIndex <= BASIC_CID_STOP);
     break;
   case CONN_PRIMARY:
     cid_ = primaryIndex++;
     assert (primaryIndex <= PRIMARY_CID_STOP);
       break;
   case CONN_SECONDARY:
   case CONN_DATA:
     cid_ = transportIndex++;
     assert (transportIndex <= TRANSPORT_SEC_CID_STOP);
     break;
   default:
     fprintf (stderr, "Unsupported connection type\n");
     exit (1);
   }
   type_ = type;
   queue_ = new PacketQueue();

}

/**
 * Constructor used by SSs when the CID is already known
 * @param type The connection type
 * @param cid The connection cid
 */
Connection::Connection (ConnectionType_t type, int cid) : peer_(0), 
							  frag_status_(FRAG_NOFRAG), 
							  frag_nb_(0), 
							  frag_byte_proc_(0),
							  frag_enable_(true)
{
  cid_ = cid;
  type_ = type;
  queue_ = new PacketQueue();
}

/**
 * Destructor
 */
Connection::~Connection ()
{
  flush_queue ();
}

/**
 * Set the connection manager
 * @param manager The Connection manager 
 */
void Connection::setManager (ConnectionManager *manager)
{
  manager_ = manager;
}

/**
 * Enqueue the given packet
 * @param p The packet to enqueue
 */
void Connection::enqueue (Packet * p) 
{
  //Mark the timestamp for queueing delay
  HDR_CMN(p)->timestamp() = NOW;
  queue_->enque (p);
}

/**
 * Dequeue a packet from the queue
 * @param p The packet to enqueue
 */
Packet * Connection::dequeue () 
{
  return queue_->deque ();
}

/**
 * Flush the queue and return the number of packets freed
 * @return The number of packets flushed
 */
int Connection::flush_queue()
{
  int i=0;
  Packet *p;
  while ( (p=queue_->deque()) ) {
    manager_->getMac()->drop(p, "CON");
    i++;
  }
  return i;
}

/**
 * Return queue size in bytes

 */
int Connection::queueByteLength () 
{
  return queue_->byteLength ();
}

/**
 * Return queue size in bytes

 */
int Connection::queueLength () 
{
  return queue_->length ();
}

/** 
 * Update the fragmentation information
 * @param status The new fragmentation status
 * @param index The new fragmentation index
 * @param bytes The number of bytes 
 */
void Connection::updateFragmentation (fragment_status status, int index, int bytes)
{
  frag_status_ = status;
  frag_nb_ = index;
  frag_byte_proc_ = bytes;
}



