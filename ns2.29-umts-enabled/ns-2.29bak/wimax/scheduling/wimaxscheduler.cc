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

#include "wimaxscheduler.h"

/** Subframe timers **/

void DlTimer::expire (Event *e)
{
  s_->start_dlsubframe();
}

void UlTimer::expire (Event *e)
{
  s_->start_ulsubframe();
}



/*
 * Create a scheduler
 * @param mac The Mac where it is located
 */
WimaxScheduler::WimaxScheduler ()
{
  map_ = NULL;
  dl_timer_ = new DlTimer (this);
  ul_timer_ = new UlTimer (this);
  nbr_db_ = new NeighborDB ();
}

/*
 * Set the mac
 * @param mac The Mac where it is located
 */
void WimaxScheduler::setMac (Mac802_16 *mac)
{
  assert (mac!=NULL);

  mac_ = mac;

  init ();
}

/**
 * Initialize the scheduler.
 */
void WimaxScheduler::init()
{
  //create map structure
  map_ = new FrameMap (mac_);
}

/**
 * Called when a timer expires
 * @param The timer ID
 */
void WimaxScheduler::expire (timer_id id)
{
  
}

/**
 * Return the type of STA this scheduler is good for.
 * Must be overwritten by subclass.
 */
station_type_t WimaxScheduler::getNodeType ()
{
  return STA_UNKNOWN;
}

/**
 * Start a new DL dlframe
 */
void WimaxScheduler::start_dlsubframe ()
{
  //must be overwritten by subclasses.
}

/**
 * Start a new UL subframe
 */
void WimaxScheduler::start_ulsubframe ()
{
  //must be overwritten by subclasses.
}

/**
 * Process a packet received by the Mac. 
 * Only scheduling related packets should be sent here (BW request, UL_MAP...)
 */
void WimaxScheduler::process (Packet * p) {
  
}

/**
 * Transfert the packets from the given connection to the given burst
 * @param con The connection
 * @param b The burst
 * @param duration The current occupation of the burst
 * @return the new burst occupation
 */
int WimaxScheduler::transfer_packets (Connection *c, Burst *b, int duration)
{
  Packet *p;
  hdr_cmn* ch;
  hdr_mac802_16 *wimaxHdr;
  double txtime;
  int txtime_s;
  bool pkt_transfered = false;
  OFDMPhy *phy = mac_->getPhy();
  
  p = c->get_queue()->head(); 

  int max_data;
  if (getNodeType ()==STA_BS)
    max_data = phy->getMaxPktSize (b->getDuration()-duration, map_->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
  else 
    max_data = phy->getMaxPktSize (b->getDuration()-duration, map_->getUlSubframe()->getProfile (b->getIUC())->getEncoding());

  debug2 ("max data=%d (burst duration=%d, used duration=%d\n", max_data, b->getDuration(), duration);
  if (max_data < HDR_MAC802_16_SIZE ||
      (c->getFragmentationStatus()!=FRAG_NOFRAG && max_data < HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE))
    return duration; //not even space for header

  while (p) {
    ch = HDR_CMN(p);
    wimaxHdr = HDR_MAC802_16(p);
    
    if (getNodeType ()==STA_BS)
      max_data = phy->getMaxPktSize (b->getDuration()-duration, map_->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
    else 
      max_data = phy->getMaxPktSize (b->getDuration()-duration, map_->getUlSubframe()->getProfile (b->getIUC())->getEncoding());
    
    if (max_data < HDR_MAC802_16_SIZE ||
	(c->getFragmentationStatus()!=FRAG_NOFRAG && max_data < HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE))
      return duration; //not even space for header
    
    if (c->getFragmentationStatus()!=FRAG_NOFRAG) {
      if (max_data >= ch->size()-c->getFragmentBytes()+HDR_MAC802_16_FRAGSUB_SIZE) {
	//add fragmentation header
	wimaxHdr->frag_subheader = true;
	//no need to fragment again
	wimaxHdr->fc = FRAG_LAST;
	wimaxHdr->fsn = c->getFragmentNumber ();
	ch->size() = ch->size()-c->getFragmentBytes()+HDR_MAC802_16_FRAGSUB_SIZE; //new packet size
	//remove packet from queue
	c->dequeue();
	//update fragmentation
	debug2 ("End of fragmentation %d (max_data=%d, bytes to send=%d\n", wimaxHdr->fsn&0x7, max_data, ch->size());
	c->updateFragmentation (FRAG_NOFRAG, 0, 0);
      } else {
	//need to fragment the packet again
	p = p->copy(); //copy packet to send
	ch = HDR_CMN(p);
	wimaxHdr = HDR_MAC802_16(p);
	//add fragmentation header
	wimaxHdr->frag_subheader = true;
	wimaxHdr->fc = FRAG_CONT;
	wimaxHdr->fsn = c->getFragmentNumber ();
	ch->size() = max_data; //new packet size
	//update fragmentation
	c->updateFragmentation (FRAG_CONT, (c->getFragmentNumber ()+1)%8, c->getFragmentBytes()+max_data-(HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE));
	debug2 ("Continue fragmentation %d\n", wimaxHdr->fsn&0x7);
      }
    } else {
      if (max_data < ch->size() && c->isFragEnable()) {
	//need to fragment the packet for the first time
	p = p->copy(); //copy packet to send
	ch = HDR_CMN(p);
	wimaxHdr = HDR_MAC802_16(p);
	//add fragmentation header
	wimaxHdr->frag_subheader = true;
	wimaxHdr->fc = FRAG_FIRST;
	wimaxHdr->fsn = c->getFragmentNumber ();
	ch->size() = max_data; //new packet size
	//update fragmentation
	c->updateFragmentation (FRAG_FIRST, 1, c->getFragmentBytes()+max_data-(HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE));
	debug2 ("First fragmentation\n");
      } else if (max_data < ch->size() && !c->isFragEnable()) {
	//the connection does not support fragmentation
	//can't move packets anymore
	return duration;
      } else {
	//no fragmentation necessary
	c->dequeue();
      }
    }

    if (getNodeType ()==STA_BS)
      txtime = phy->getTrxTime (ch->size(), map_->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
    else
      txtime = phy->getTrxTime (ch->size(), map_->getUlSubframe()->getProfile (b->getIUC())->getEncoding());
    ch->txtime() = txtime;
    txtime_s = (int)round (txtime/phy->getSymbolTime ()); //in units of symbol 
    //printf ("symbtime=%f\n", phy->getSymbolTime ());
    //printf ("Check packet to send: size=%d txtime=%f(%d) duration=%d(%f)\n", ch->size(),txtime, txtime_s, b->getDuration(), b->getDuration()*phy->getSymbolTime ());
    assert ( (duration+txtime_s) <= b->getDuration() );
    //printf ("transfert to burst\n");
    //p = c->dequeue();   //dequeue connection queue      
    b->enqueue(p);      //enqueue into burst
    duration += txtime_s; //increment time
    if (!pkt_transfered && getNodeType ()!=STA_BS){ //if we transfert at least one packet, remove bw request
      pkt_transfered = true;
      map_->getUlSubframe()->getBw_req()->removeRequest (c->get_cid());
    }
    p = c->get_queue()->head(); //get new head
  }
  return duration;
}

