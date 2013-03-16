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

#include "ssscheduler.h"
#include "burst.h"

/**
 * Tcl hook for creating SS scheduler 
 */
static class SSschedulerClass : public TclClass {
public:
  SSschedulerClass() : TclClass("WimaxScheduler/SS") {}
  TclObject* create(int, const char*const*) {
    return (new SSscheduler());
    
  }
} class_ssscheduler;

/*
 * Create a scheduler
 */
SSscheduler::SSscheduler (): t1timer_(0),t2timer_(0),t6timer_(0), t12timer_(0),
			     t21timer_(0), lostDLMAPtimer_(0), lostULMAPtimer_(0),
			     t44timer_(0), scan_info_(0), scanFlag (0)
{
  debug2 ("SSscheduler created\n");
}

/**
 * Initializes the scheduler
 */
void SSscheduler::init ()
{
  WimaxScheduler::init();
  
  //At initialization, the SS is looking for synchronization
  mac_->setMacState (MAC802_16_WAIT_DL_SYNCH);
  mac_->getPhy()->setMode (OFDM_RECV);
  //start timer for expiration
  t21timer_ = new WimaxT21Timer (mac_);
  t21timer_->start (mac_->macmib_.t21_timeout);

  //creates other timers
  t1timer_ = new WimaxT1Timer (mac_);
  t12timer_ = new WimaxT12Timer (mac_);
  t2timer_ = new WimaxT2Timer (mac_);
  lostDLMAPtimer_ = new WimaxLostDLMAPTimer (mac_);
  lostULMAPtimer_ = new WimaxLostULMAPTimer (mac_);

  nb_scan_req_ = 0;

  scan_info_ = (struct scanning_structure *) malloc (sizeof (struct scanning_structure));
  memset (scan_info_, 0, sizeof (struct scanning_structure));
  scan_info_->nbr = NULL;
  scan_info_->substate = NORMAL;
}

/**
 * Interface with the TCL script
 * @param argc The number of parameter
 * @param argv The list of parameters
 */
int SSscheduler::command(int argc, const char*const* argv)
{
  if (argc == 2) {
    if (strcmp(argv[1], "send-scan") == 0) {
      send_scan_request();
      return TCL_OK;
    }
  }
  
  return TCL_ERROR;
}

/**
 * Start a new frame
 */
void SSscheduler::start_dlsubframe ()
{
  //mac_->debug ("At %f in Mac %d SS scheduler dlsubframe expires %d\n", NOW, mac_->addr(), scan_info_->substate);

  mac_->frame_number_++;

  switch (scan_info_->substate) {
  case SCAN_PENDING: 
    if (scan_info_->count == 0) {
      resume_scanning();
      return;
    } 
    scan_info_->count--;
    break;
  case HANDOVER_PENDING:
    if (scan_info_->handoff_timeout == 0) {
      assert (scan_info_->nbr);
#ifdef USE_802_21
      //mac_->debug ("At %f in Mac %d link handoff proceeding\n", NOW, mac_->addr());
      //mac_->send_link_handoff_proceeding (mac_->addr(), mac_->getPeerNode_head()->getPeerNode(), scan_info_->nbr->getID());
#endif 
      scan_info_->substate = HANDOVER;
      //restore previous state 
      //mac_->restore_state (scan_info_->nbr->getState()->state_info);
      mac_->setChannel (scan_info_->nbr->getState()->state_info->channel);
      lost_synch ();
      //add target as peer
      mac_->addPeerNode (new PeerNode(scan_info_->nbr->getID()));
      return;
    }
    scan_info_->handoff_timeout--;
    break;
  default:
    break;
  }
    
  //change state of PHY
  //mac_->getPhy()->setMode (OFDM_RECV);
  
  //this is the begining of new frame
  map_->setStarttime (NOW);

  //start handler of dlsubframe
  map_->getDlSubframe()->getTimer()->sched (0);

  //reschedule for next frame
  dl_timer_->resched (mac_->getFrameDuration());
}

/**
 * Start a new frame
 */
void SSscheduler::start_ulsubframe ()
{
  //mac_->debug ("At %f in Mac %d SS scheduler ulsubframe expires\n", NOW, mac_->addr());
  
  //change state of PHY: even though it should have been done before
  //there are some cases where it does not (during scanning)
  mac_->getPhy()->setMode (OFDM_SEND);

  //1-Transfert the packets from the queues in Connections to burst queues
  Burst *b;
  OFDMPhy *phy = mac_->getPhy();
  //printf ("SS has %d ul bursts\n", map_->getUlSubframe()->getNbPdu ());

  PeerNode *peer = mac_->getPeerNode_head(); //this is the BS
  assert (peer!=NULL);

  for (int index = 0 ; index < map_->getUlSubframe()->getNbPdu (); index++) {
    b = map_->getUlSubframe()->getPhyPdu (index)->getBurst (0);

    if (b->getIUC()==UIUC_END_OF_MAP) {
      //consistency check..
      assert (index == map_->getUlSubframe()->getNbPdu ()-1);
      break;
    }    
    
    if (b->getIUC()==UIUC_INITIAL_RANGING || b->getIUC()==UIUC_REQ_REGION_FULL)
      continue;
    int duration = 0; 
    //get the packets from the connection with the same CID
    //printf ("\tBurst CID=%d\n", b->getCid());
    Connection *c=mac_->getCManager ()->get_connection (b->getCid(), true);
    //assert (c);
    if (!c)
      continue; //I do not have this CID. Must be for another node
    //transfert the packets until it reaches burst duration or no more packets
    assert (c->getType()==CONN_PRIMARY);
    if (peer->getBasic()!= NULL) 
      duration = transfer_packets (peer->getBasic(), b, duration);
    if (peer->getPrimary()!= NULL)
      duration = transfer_packets (peer->getPrimary(), b, duration);
    if (peer->getSecondary()!= NULL)
      duration = transfer_packets (peer->getSecondary(), b, duration);
    if (peer->getOutData()!=NULL)
      duration = transfer_packets (peer->getOutData(), b, duration);
  }

  //2-compute size of data left to create bandwidth requests
  if (peer->getBasic()!= NULL) 
    create_request (peer->getBasic());
  if (peer->getPrimary()!= NULL)
    create_request (peer->getPrimary());
  if (peer->getSecondary()!= NULL)
    create_request (peer->getSecondary());
  if (peer->getOutData()!=NULL)
    create_request (peer->getOutData());

  //start handler for ulsubframe
  b = map_->getUlSubframe()->getPhyPdu (0)->getBurst (0);
  map_->getUlSubframe()->getTimer()->sched (b->getStarttime()*phy->getSymbolTime());

  //reschedule for next frame
  ul_timer_->resched (mac_->getFrameDuration());      
}

/**
 * Create a request for the given connection
 * @param con The connection to check
 */
void SSscheduler::create_request (Connection *con)
{
  if (con->queueLength()==0)
    return; //queue is empty
  else if (map_->getUlSubframe()->getBw_req()->getRequest (con->get_cid())!=NULL) {
    debug2 ("At %f in Mac %d already pending requests for cid=%d\n", NOW, mac_->addr(), con->get_cid());
    return; //there is already a pending request
  }

  Packet *p= mac_->getPacket();
  hdr_cmn* ch = HDR_CMN(p);
  bw_req_header_t *header = (bw_req_header_t *)&(HDR_MAC802_16(p)->header);
  header->ht=1;
  header->ec=1;
  header->type = 0; //incremental..to check meaning
  header->br = con->queueByteLength();
  header->cid = con->get_cid();

  double txtime = mac_->getPhy()->getTrxTime (ch->size(), map_->getUlSubframe()->getProfile (UIUC_REQ_REGION_FULL)->getEncoding());
  ch->txtime() = txtime;
  map_->getUlSubframe()->getBw_req()->addRequest (p, con->get_cid(), con->queueByteLength());
  debug2 ("SSscheduler enqueued request for cid=%d len=%d\n", con->get_cid(), con->queueByteLength());
  //start timeout for request
}

/**
 * Process a packet received by the Mac. Only scheduling related packets should be sent here (BW request, UL_MAP...)
 * @param p The packet to process
 */
void SSscheduler::process (Packet * p)
{
  assert (mac_ && HDR_CMN(p)->ptype()==PT_MAC);
  debug2 ("SSScheduler received packet to process\n");
  
  hdr_mac802_16 *wimaxHdr = HDR_MAC802_16(p);
  gen_mac_header_t header = wimaxHdr->header;

  //we cast to this frame because all management frame start with
  //a type 
  mac802_16_dl_map_frame *frame = (mac802_16_dl_map_frame*) p->accessdata();

  switch (frame->type) {
  case MAC_DL_MAP: 
    map_->setStarttime (NOW-HDR_CMN(p)->txtime());
    //printf ("At %f frame start at %f\n", NOW, map_->getStarttime());
    process_dl_map (frame);
    break;
  case MAC_DCD: 
    process_dcd ((mac802_16_dcd_frame*)frame);
    break;
  case MAC_UL_MAP: 
    process_ul_map ((mac802_16_ul_map_frame*)frame);
    break;
  case MAC_UCD: 
    process_ucd ((mac802_16_ucd_frame*)frame);
    break;
  case MAC_RNG_RSP:
    process_ranging_rsp ((mac802_16_rng_rsp_frame*) frame);
    break;
  case MAC_REG_RSP:
    process_reg_rsp ((mac802_16_reg_rsp_frame*) frame);
    break;    
  case MAC_MOB_SCN_RSP:
    process_scan_rsp ((mac802_16_mob_scn_rsp_frame *) frame);
    break;
  case MAC_MOB_BSHO_RSP:
    process_bsho_rsp ((mac802_16_mob_bsho_rsp_frame *) frame);
    break;
  case MAC_MOB_NBR_ADV:
    process_nbr_adv ((mac802_16_mob_nbr_adv_frame *) frame);
    break;
  default:
    mac_->debug ("unknown packet in SS %d\n", mac_->addr());
    //exit (0);
  }
  //Packet::free (p);
}

/**
 * Return the type of STA this scheduler is good for
 * @return STA_SS
 */
station_type_t SSscheduler::getNodeType ()
{
  return STA_MN;
}

/**
 * Called when lost synchronization
 */
void SSscheduler::lost_synch ()
{
#ifdef USE_802_21
  int poa = -1;
  bool disconnect = false;
#endif

  //reset timers
  if (t1timer_->busy()!=0)
    t1timer_->stop();
  if (t12timer_->busy()!=0)
    t12timer_->stop();
  if (t21timer_->busy()!=0)
    t21timer_->stop();
  if (lostDLMAPtimer_->busy()!=0)
    lostDLMAPtimer_->stop(); 
  if (lostULMAPtimer_->busy()!=0)
    lostULMAPtimer_->stop(); 
  if (t2timer_->busy()!=0)
    t2timer_->stop(); 
  if (t44timer_ && t44timer_->busy()!=0)
    t44timer_->stop();

  //we need to go to receiving mode
  //printf ("Set phy to recv %x\n", mac_->getPhy());
  mac_->getPhy()->setMode (OFDM_RECV);
  if (mac_->getMacState()==MAC802_16_CONNECTED) {
    //remove possible pending requests
    map_->getUlSubframe()->getBw_req()->removeRequests(); 

#ifdef USE_802_21
    poa = mac_->getPeerNode_head()->getPeerNode ();
    disconnect = true;
#endif
  }

  //remove information about peer node
  if (mac_->getPeerNode_head())
    mac_->removePeerNode (mac_->getPeerNode_head());

  //start waiting for DL synch
  mac_->setMacState (MAC802_16_WAIT_DL_SYNCH);
  t21timer_->start (mac_->macmib_.t21_timeout);
  if (dl_timer_->status()==TIMER_PENDING)
    dl_timer_->cancel();
  map_->getDlSubframe()->getTimer()->reset();
  if (ul_timer_->status()==TIMER_PENDING)
    ul_timer_->cancel();
  map_->getUlSubframe()->getTimer()->reset();

#ifdef USE_802_21
  if (disconnect) {
    mac_->debug ("At %f in Mac %d, send link down\n", NOW, mac_->addr());
    mac_->send_link_down (mac_->addr(), poa, LD_RC_FAIL_NORESOURCE);
  }
#endif

  if (scan_info_->substate == HANDOVER_PENDING || scan_info_->substate == SCAN_PENDING) {
    //we have lost synch before scanning/handover is complete
    for (int i=0 ; i < scan_info_->nb_rdv_timers ; i++) {
      //debug ("canceling rdv timer\n");
      if (scan_info_->rdv_timers[i]->busy()) {
	scan_info_->rdv_timers[i]->stop();
      }
      delete (scan_info_->rdv_timers[i]);
    }
    scan_info_->nb_rdv_timers = 0;
  }

  if (scan_info_->substate == HANDOVER_PENDING) {
    mac_->debug ("Lost synch during pending handover (%d)\n", scan_info_->handoff_timeout);
    //since we lost connection, let's execute handover immediately 
    scan_info_->substate = HANDOVER;
    mac_->setChannel (scan_info_->nbr->getState()->state_info->channel);
    //add target as peer
    mac_->addPeerNode (new PeerNode(scan_info_->nbr->getID()));
    return;
  } 
  if (scan_info_->substate == SCAN_PENDING) {
    mac_->debug ("Lost synch during pending scan (%d)\n", scan_info_->count);
    //we must cancel the scanning
    scan_info_->substate = NORMAL;
  }
}
 
/**
 * Called when a timer expires
 * @param The timer ID
 */
void SSscheduler::expire (timer_id id)
{
  switch (id) {
  case WimaxT21TimerID:
    mac_->debug ("At %f in Mac %d, synchronization failed\n", NOW, mac_->addr());
    //go to next channel
    mac_->nextChannel();
    t21timer_->start (mac_->macmib_.t21_timeout);
    break;
  case WimaxLostDLMAPTimerID:
    mac_->debug ("At %f in Mac %d, lost synchronization (DL_MAP)\n", NOW, mac_->addr());
    lost_synch ();
    break;
  case WimaxT1TimerID:
    mac_->debug ("At %f in Mac %d, lost synchronization (DCD)\n", NOW, mac_->addr());
    lost_synch ();
    break;
  case WimaxLostULMAPTimerID:
    mac_->debug ("At %f in Mac %d, lost synchronization (UL_MAP)\n", NOW, mac_->addr());
    lost_synch ();
    break;
  case WimaxT12TimerID:
    mac_->debug ("At %f in Mac %d, lost uplink param (UCD)\n", NOW, mac_->addr());
    lost_synch ();
    break;
  case WimaxT2TimerID:
    mac_->debug ("At %f in Mac %d, lost synchronization (RNG)\n", NOW, mac_->addr());
    map_->getUlSubframe()->getRanging()->removeRequest ();
    lost_synch ();
    break;
  case WimaxT3TimerID:
    mac_->debug ("At %f in Mac %d, no response from BS\n", NOW, mac_->addr());
    //we reach the maximum number of retries
    //mark DL channel usuable (i.e we go to next)
    map_->getUlSubframe()->getRanging()->removeRequest ();
    mac_->nextChannel();
    lost_synch ();
    break;
  case WimaxT6TimerID:
    mac_->debug ("At %f in Mac %d, registration timeout (nbretry=%d)\n", NOW, mac_->addr(),
		 nb_reg_retry_);
    if (nb_reg_retry_ == mac_->macmib_.reg_req_retry) {
      mac_->debug ("\tmax retry excedeed\n");
      lost_synch ();
    } else {
      send_registration();
    }
    break;
  case WimaxT44TimerID:
    mac_->debug ("At %f in Mac %d, did not receive MOB_SCN-RSP (nb_retry=%d/%d)\n", NOW, mac_->addr(), nb_scan_req_, mac_->macmib_.scan_req_retry);
    if (nb_scan_req_ <= mac_->macmib_.scan_req_retry) {
      send_scan_request ();
    } else { //reset for next time
      nb_scan_req_ = 0;
    }
    break;
  case WimaxScanIntervalTimerID:
    pause_scanning ();
    break;    
  case WimaxRdvTimerID:
    //we need to meet at another station. We cancel the current scanning
    //lost_synch ();
    mac_->debug ("At %f in Mac %d Rdv timer expired\n", NOW, mac_->addr());
    break;
  default:
    mac_->debug ("Trigger unkown\n");
  }
}

/**** Packet processing methods ****/

/**
 * Process a DL_MAP message
 * @param frame The dl_map information
 */
void SSscheduler::process_dl_map (mac802_16_dl_map_frame *frame)
{
  assert (frame);
  
  //create an entry for the BS
  if (mac_->getPeerNode (frame->bsid)==NULL)
    mac_->addPeerNode (new PeerNode (frame->bsid));

  map_->parseDLMAPframe (frame);

  if (mac_->getMacState()==MAC802_16_WAIT_DL_SYNCH) {
    mac_->debug ("At %f in %d, received DL_MAP for synch from %d (substate=%d)\n", 
		 NOW, mac_->addr(), frame->bsid,scan_info_->substate);
    assert (t21timer_->busy()!=0);
    //synchronization is done
    t21timer_->stop();
    //start lost_dl_map
    lostDLMAPtimer_->start (mac_->macmib_.lost_dlmap_interval);
    //start T1: DCD
    t1timer_->start (mac_->macmib_.t1_timeout);
    //start T12: UCD
    t12timer_->start (mac_->macmib_.t12_timeout);

#ifdef USE_802_21
    if (scan_info_->substate != SCANNING) {
      mac_->debug ("At %f in Mac %d, send link detected\n", NOW, mac_->addr());
      mac_->send_link_detected (mac_->addr(), frame->bsid, 1);
    }
#endif
    
    mac_->setMacState(MAC802_16_WAIT_DL_SYNCH_DCD);

    //if I am doing handoff and we have dcd/ucd information 
    //from scanning, use it
    if (scan_info_->substate == HANDOVER || scan_info_->substate == SCANNING) {
      if (scan_info_->substate == SCANNING) {
	if (scan_info_->nbr == NULL || scan_info_->nbr->getID()!=frame->bsid) {
	  //check if an entry already exist in the database
	  scan_info_->nbr = nbr_db_->getNeighbor (frame->bsid);
	  if (scan_info_->nbr == NULL) {
	    //create entry
	    debug2 ("Creating nbr info for node %d\n", frame->bsid);
	    scan_info_->nbr = new WimaxNeighborEntry (frame->bsid);
	    nbr_db_->addNeighbor (scan_info_->nbr);
	  } else {
	    debug2 ("loaded nbr info\n");
	    if (scan_info_->nbr->isDetected ()) {
 	      //we already synchronized with this AP...skip channel
 	      mac_->nextChannel();
 	      lost_synch ();
 	      return;
 	    } 
	  }
	}
      }//if HANDOVER, scan_info_->nbr is already set

      bool error = false;
      //we check if we can read the DL_MAP
      mac802_16_dcd_frame *dcd = scan_info_->nbr->getDCD();
      if (dcd!=NULL) {
	debug2 ("Check if we can decode stored dcd\n");
	//check if we can decode dl_map with previously acquired dcd      
	bool found;
	for (int i = 0 ; !error && i < map_->getDlSubframe()->getPdu()->getNbBurst() ; i++) {
	  int diuc = map_->getDlSubframe()->getPdu()->getBurst(i)->getIUC();
	  if (diuc == DIUC_END_OF_MAP)
	    continue;
	  found = false;
	  for (u_int32_t j = 0 ; !found && j < dcd->nb_prof; j++) {
	    found = dcd->profiles[j].diuc==diuc;	    
	  }
	  error = !found;
	}
	if (!error)
	  process_dcd (dcd);
      } else {
	debug2 ("No DCD information found\n");
      }
    }
  } else {
    //maintain synchronization
    assert (lostDLMAPtimer_->busy());
    lostDLMAPtimer_->stop();
    //printf ("update dlmap timer\n");
    lostDLMAPtimer_->start (mac_->macmib_.lost_dlmap_interval);

    if (mac_->getMacState()!= MAC802_16_WAIT_DL_SYNCH_DCD
	&& mac_->getMacState()!=MAC802_16_UL_PARAM) {

      //since the map may have changed, we need to adjust the timer 
      //for the DLSubframe
      double stime = map_->getStarttime();
      stime += map_->getDlSubframe()->getPdu()->getBurst(1)->getStarttime()*mac_->getPhy()->getSymbolTime();
      //printf ("received dl..needs to update expiration to %f, %f,%f\n", stime, NOW,map_->getStarttime());
      map_->getDlSubframe()->getTimer()->resched (stime-NOW);
      dl_timer_->resched (map_->getStarttime()+mac_->getFrameDuration()-NOW);
    }
  }
}

/**
 * Process a DCD message
 * @param frame The dcd information
 */
void SSscheduler::process_dcd (mac802_16_dcd_frame *frame)
{
  if (mac_->getMacState()==MAC802_16_WAIT_DL_SYNCH) {
    //we are waiting for DL_MAP, ignore this message
    return;
  }

  map_->parseDCDframe (frame);
  if (mac_->getMacState()==MAC802_16_WAIT_DL_SYNCH_DCD) {
    mac_->debug ("At %f in %d, received DCD for synch\n", NOW, mac_->addr());
    //now I have all information such as frame duration
    //adjust timing in case the frame we received the DL_MAP
    //and the DCD is different
    while (NOW - map_->getStarttime () > mac_->getFrameDuration()) {
      map_->setStarttime (map_->getStarttime()+mac_->getFrameDuration());
    }
    
    //store information to be used during potential handoff
    if (scan_info_->substate == SCANNING) {
      mac802_16_dcd_frame *tmp = (mac802_16_dcd_frame *) malloc (sizeof (mac802_16_dcd_frame));
      memcpy (tmp, frame, sizeof (mac802_16_dcd_frame));
      mac802_16_dcd_frame *old = scan_info_->nbr->getDCD(); 
      if (frame == old)
	frame = tmp;
      if (old)
	free (old); //free previous entry
      scan_info_->nbr->setDCD(tmp);    //set new one
    }

    mac_->setMacState(MAC802_16_UL_PARAM);
    //we can schedule next frame
    //printf ("SS schedule next frame at %f\n", map_->getStarttime()+mac_->getFrameDuration());
    //dl_timer_->sched (map_->getStarttime()+mac_->getFrameDuration()-NOW);
  }

  if (t1timer_->busy()!=0) {
    //we were waiting for this packet
    t1timer_->stop();
    t1timer_->start (mac_->macmib_.t1_timeout);
  }
}

/**
 * Process a UCD message
 * @param frame The ucd information
 */
void SSscheduler::process_ucd (mac802_16_ucd_frame *frame)
{
  if (mac_->getMacState()==MAC802_16_WAIT_DL_SYNCH
      ||mac_->getMacState()==MAC802_16_WAIT_DL_SYNCH_DCD) {
    //discard the packet
    return;
  }
  assert (t12timer_->busy()!=0); //we are waiting for this packet

  if (mac_->getMacState()==MAC802_16_UL_PARAM) {
    //check if uplink channel usable
    mac_->debug ("At %f in %d, received UL(UCD) parameters\n", NOW, mac_->addr());
    //start T2: ranging
    t2timer_->start (mac_->macmib_.t2_timeout);
    //start Lost UL-MAP
    lostULMAPtimer_->start (mac_->macmib_.lost_ulmap_interval);

    //store information to be used during potential handoff
    if (scan_info_->substate == SCANNING) {
      mac802_16_ucd_frame *tmp = (mac802_16_ucd_frame *) malloc (sizeof (mac802_16_ucd_frame));
      memcpy (tmp, frame, sizeof (mac802_16_ucd_frame));
      mac802_16_ucd_frame *old = scan_info_->nbr->getUCD(); 
      if (frame == old)
	frame = tmp;
      if (old) 
	free (old); //free previous entry
      scan_info_->nbr->setUCD(tmp);    //set new one            
      
    }

    //change state
    mac_->setMacState (MAC802_16_RANGING);
  }

  //reset T12
  t12timer_->stop();
  t12timer_->start (mac_->macmib_.t12_timeout);

  map_->parseUCDframe (frame);
}

/**
 * Process a UL_MAP message
 * @param frame The ul_map information
 */
void SSscheduler::process_ul_map (mac802_16_ul_map_frame *frame)
{
  if (mac_->getMacState()==MAC802_16_WAIT_DL_SYNCH 
      || mac_->getMacState()==MAC802_16_WAIT_DL_SYNCH_DCD) {
    //discard the packet
    return;
  }

  if (mac_->getMacState()==MAC802_16_UL_PARAM) {
    if (scan_info_->substate == HANDOVER || scan_info_->substate==SCANNING) {
      FrameMap *tmpMap = new FrameMap (mac_);
      tmpMap->parseULMAPframe (frame); 
      //printf ("Checking if we can read UL_MAP\n");
      bool error = false;
      //we check if we can read the UL_MAP
      mac802_16_ucd_frame *ucd = scan_info_->nbr->getUCD();
      if (ucd!=NULL) {
	//check if we can decode ul_map with previously acquired ucd      
	bool found;
	for (int i = 0 ; !error && i < tmpMap->getUlSubframe()->getNbPdu() ; i++) {
	  UlBurst *b = (UlBurst*)tmpMap->getUlSubframe()->getPhyPdu(i)->getBurst(0);
	  int uiuc = b->getIUC();
	  if (uiuc == UIUC_END_OF_MAP)
	    continue;
	  if (uiuc == UIUC_EXT_UIUC && b->getExtendedUIUC ()== UIUC_FAST_RANGING)
 	    uiuc = b->getFastRangingUIUC();	  
	  found = false;
	  for (u_int32_t j = 0 ; !found && j < ucd->nb_prof; j++) {
	    //printf ("\t prof=%d, search=%d\n", ucd->profiles[j].uiuc, uiuc);
	    found = ucd->profiles[j].uiuc==uiuc;	    
	  }
	  error = !found;
	}
	if (!error)
	  process_ucd (ucd);
      }
      delete (tmpMap);
      if (error) {
	//we cannot read message
	return;
      }
    } else
      return;
  }

  if (scan_info_->substate == SCANNING) {
    //TBD: add checking scanning type for the given station
    u_char scanning_type = 0;
    for (int i = 0 ; i < scan_info_->rsp->n_recommended_bs_full ; i++) {
      if (scan_info_->rsp->rec_bs_full[i].recommended_bs_id == scan_info_->nbr->getID()) {
	scanning_type = scan_info_->rsp->rec_bs_full[i].scanning_type;
	break;
      }
    }
    if (scanning_type == 0) {
      //store information about possible base station and keep scanning
      scan_info_->nbr->getState()->state_info= mac_->backup_state();
      mac_->debug ("At %f in Mac %d bs %d detected during scanning\n", NOW, mac_->addr(), scan_info_->nbr->getID());
      scan_info_->nbr->setDetected (true);
      mac_->nextChannel();
      lost_synch ();
      return;
    }
  }

  map_->parseULMAPframe (frame);  
  if (mac_->getMacState()==MAC802_16_RANGING) {
      //execute ranging
     assert (t2timer_->busy()!=0); //we are waiting for this packet
     init_ranging ();
  }

  //schedule when to take care of outgoing packets
  double start = map_->getStarttime();
  start += map_->getUlSubframe()->getStarttime()*mac_->getPhy()->getPS(); //offset for ul subframe
  start -= NOW; //works with relative time not absolute
  debug2 ("Uplink starts in %f (framestate=%f) %f %f\n", 
	  start, 
	  map_->getStarttime(),
	  mac_->getFrameDuration()/mac_->getPhy()->getPS(), 
	  mac_->getFrameDuration()/mac_->getPhy()->getSymbolTime());
  
  ul_timer_->resched (start);

  //reset Lost UL-Map
  lostULMAPtimer_->stop();
  lostULMAPtimer_->start (mac_->macmib_.lost_ulmap_interval);
}

/**
 * Process a ranging response message 
 * @param frame The ranging response frame
 */
void SSscheduler::process_ranging_rsp (mac802_16_rng_rsp_frame *frame)
{
  //check the destination
  if (frame->ss_mac_address != mac_->addr())
    return;
  
  Connection *basic, *primary;
  PeerNode *peer;

  //TBD: add processing for periodic ranging

  //check status 
  switch (frame->rng_status) {
  case RNG_SUCCESS:
    mac_->debug ("Ranging response: status = Success.Basic=%d, Primary=%d\n",
	    frame->basic_cid, frame->primary_cid);

    peer = mac_->getPeerNode_head();
    assert (peer);
    map_->getUlSubframe()->getRanging()->removeRequest ();

    if (scan_info_->substate == SCANNING) {
      //store information about possible base station and keep scanning
      scan_info_->nbr->getState()->state_info= mac_->backup_state();
      scan_info_->nbr->setDetected (true);
      //keep the information for later
      mac802_16_rng_rsp_frame *tmp = (mac802_16_rng_rsp_frame *) malloc (sizeof (mac802_16_rng_rsp_frame));
      memcpy (tmp, frame, sizeof (mac802_16_rng_rsp_frame));
      scan_info_->nbr->setRangingRsp (tmp);
      mac_->nextChannel();
      lost_synch ();
      return;
    }

    //ranging worked, now we must register
    basic = peer->getBasic();
    primary = peer->getPrimary();
    if (basic!=NULL && basic->get_cid ()==frame->basic_cid) {
      //duplicate response
      assert (primary->get_cid () == frame->primary_cid);
    } else {
      if (basic !=NULL) {
	//we have been allocated new cids..clear old ones
	mac_->getCManager ()->remove_connection (basic);
	mac_->getCManager ()->remove_connection (primary);
	if (peer->getSecondary()!=NULL)
	  mac_->getCManager ()->remove_connection (peer->getSecondary());
	if (peer->getOutData()!=NULL)
	  mac_->getCManager ()->remove_connection (peer->getOutData());
	if (peer->getInData()!=NULL)
	  mac_->getCManager ()->remove_connection (peer->getInData());
      } 

      basic = new Connection (CONN_BASIC, frame->basic_cid);
      Connection *upbasic = new Connection (CONN_BASIC, frame->basic_cid);
      primary = new Connection (CONN_PRIMARY, frame->primary_cid);
      Connection *upprimary = new Connection (CONN_PRIMARY, frame->primary_cid);

      //a SS should only have one peer, the BS
      peer->setBasic (upbasic); //set outgoing
      peer->setPrimary (upprimary); //set outgoing
      basic->setPeerNode (peer);
      primary->setPeerNode (peer);
      mac_->getCManager()->add_connection (upbasic, true);
      mac_->getCManager()->add_connection (basic, false);
      mac_->getCManager()->add_connection (upprimary, true);
      mac_->getCManager()->add_connection (primary, false);
    }

    //registration must be sent using Primary Management CID
    mac_->setMacState (MAC802_16_REGISTER);
    //stop timeout timer
    t2timer_->stop ();
    nb_reg_retry_ = 0; //first time sending
    send_registration();

    break;
  case RNG_ABORT:
  case RNG_CONTINUE:
  case RNG_RERANGE:
    break;
  default:
    fprintf (stderr, "Unknown status reply\n");
    exit (-1);
  }
}

/**
 * Schedule a ranging
 */
void SSscheduler::init_ranging ()
{
  //check if there is a ranging opportunity
  UlSubFrame *ulsubframe = map_->getUlSubframe();
  DlSubFrame *dlsubframe = map_->getDlSubframe();

  /* If I am doing a Handoff, check if I already associated 
     with the target AP*/
  if (scan_info_->substate == HANDOVER && scan_info_->nbr->getRangingRsp()!=NULL) {
    mac_->debug ("At %f in Mac %d MN already executed ranging during scanning\n", NOW, mac_->addr());
    process_ranging_rsp (scan_info_->nbr->getRangingRsp());
    return;
  }

  //check if there is Fast Ranging IE
  for (PhyPdu *p = map_->getUlSubframe ()->getFirstPdu(); p ; p= p ->next_entry()) {
    UlBurst *b = (UlBurst*) p->getBurst(0);
    if (b->getIUC() == UIUC_EXT_UIUC && 
	b->getExtendedUIUC ()== UIUC_FAST_RANGING &&
	b->getFastRangingMacAddr ()==mac_->addr()) {
      debug2 ("Found fast ranging\n");
      //we should put the ranging request in that burst
      Packet *p= mac_->getPacket();
      hdr_cmn* ch = HDR_CMN(p);
      HDR_MAC802_16(p)->header.cid = INITIAL_RANGING_CID;

      p->allocdata (sizeof (struct mac802_16_rng_req_frame));
      mac802_16_rng_req_frame *frame = (mac802_16_rng_req_frame*) p->accessdata();
      frame->type = MAC_RNG_REQ;
      frame->dc_id = dlsubframe->getChannelID();
      frame->ss_mac_address = mac_->addr();
      //other elements??      
      ch->size() += RNG_REQ_SIZE;
      //compute when to send message
      double txtime = mac_->getPhy()->getTrxTime (ch->size(), ulsubframe->getProfile (b->getFastRangingUIUC ())->getEncoding());
      ch->txtime() = txtime;
      //starttime+backoff
      ch->timestamp() = NOW; //add timestamp since it bypasses the queue
      b->enqueue(p);
      mac_->setMacState(MAC802_16_WAIT_RNG_RSP);
      return;
    }
  }


  for (PhyPdu *pdu = ulsubframe->getFirstPdu(); pdu ; pdu = pdu->next_entry()) {
    if (pdu->getBurst(0)->getIUC()==UIUC_INITIAL_RANGING) {
      mac_->debug ("At %f SS Mac %d found ranging opportunity\n", NOW, mac_->addr());
      Packet *p= mac_->getPacket();
      hdr_cmn* ch = HDR_CMN(p);
      HDR_MAC802_16(p)->header.cid = INITIAL_RANGING_CID;

      p->allocdata (sizeof (struct mac802_16_rng_req_frame));
      mac802_16_rng_req_frame *frame = (mac802_16_rng_req_frame*) p->accessdata();
      frame->type = MAC_RNG_REQ;
      frame->dc_id = dlsubframe->getChannelID();
      frame->ss_mac_address = mac_->addr();
      //other elements??      
      ch->size() += RNG_REQ_SIZE;
      //compute when to send message
      double txtime = mac_->getPhy()->getTrxTime (ch->size(), ulsubframe->getProfile (pdu->getBurst(0)->getIUC())->getEncoding());
      ch->txtime() = txtime;
      //starttime+backoff
      map_->getUlSubframe()->getRanging()->addRequest (p);
      mac_->setMacState(MAC802_16_WAIT_RNG_RSP);

      return;
    }
  }
}

/**
 * Prepare to send a registration message
 */
void SSscheduler::send_registration ()
{
  Packet *p;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr;
  mac802_16_reg_req_frame *reg_frame;
  PeerNode *peer;

  //create packet for request
  p = mac_->getPacket ();
  ch = HDR_CMN(p);
  wimaxHdr = HDR_MAC802_16(p);
  p->allocdata (sizeof (struct mac802_16_reg_req_frame));
  reg_frame = (mac802_16_reg_req_frame*) p->accessdata();
  reg_frame->type = MAC_REG_REQ;
  ch->size() += REG_REQ_SIZE;

  peer = mac_->getPeerNode_head();  
  wimaxHdr->header.cid = peer->getPrimary()->get_cid();
  peer->getPrimary()->enqueue (p);

  //start reg timeout
  if (t6timer_==NULL) {
    t6timer_ = new WimaxT6Timer (mac_);
  }
  t6timer_->start (mac_->macmib_.t6_timeout);
  nb_reg_retry_++;
}

/**
 * Process a registration response message 
 * @param frame The registration response frame
 */
void SSscheduler::process_reg_rsp (mac802_16_reg_rsp_frame *frame)
{
  //check the destination
  PeerNode *peer = mac_->getPeerNode_head();

  if (frame->response == 0) {
    //status OK
    mac_->debug ("At %f in Mac %d, registration sucessful (nbretry=%d)\n", NOW, mac_->addr(),
		 nb_reg_retry_);
    Connection *secondary = peer->getSecondary();
    if (!secondary) {
      Connection *secondary = new Connection (CONN_SECONDARY, frame->sec_mngmt_cid);
      Connection *upsecondary = new Connection (CONN_SECONDARY, frame->sec_mngmt_cid);
      mac_->getCManager()->add_connection (upsecondary, true);
      mac_->getCManager()->add_connection (secondary, false);
      peer->setSecondary (upsecondary);
      secondary->setPeerNode (peer);
    }
    //cancel timeout
    t6timer_->stop ();

    //update status
    mac_->setMacState(MAC802_16_CONNECTED);

    //we need to setup a data connection (will be moved to service flow handler)
    mac_->getServiceHandler ()->sendFlowRequest (peer->getPeerNode(), true);
    mac_->getServiceHandler ()->sendFlowRequest (peer->getPeerNode(), false);

#ifdef USE_802_21
    if (scan_info_->substate==HANDOVER) {
      mac_->debug ("At %f in Mac %d link handoff complete\n", NOW, mac_->addr());      
      mac_->send_link_handover_complete (mac_->addr(), scan_info_->serving_bsid, peer->getPeerNode());
      scan_info_->handoff_timeout = -1;
    }
    mac_->debug ("At %f in Mac %d, send link up\n", NOW, mac_->addr());
    mac_->send_link_up (mac_->addr(), peer->getPeerNode(), -1);
#endif
    
  } else {
    //status failure
    mac_->debug ("At %f in Mac %d, registration failed (nbretry=%d)\n", NOW, mac_->addr(),
		 nb_reg_retry_);
    if (nb_reg_retry_ == mac_->macmib_.reg_req_retry) {
#ifdef USE_802_21
      if (scan_info_ && scan_info_->handoff_timeout == -2) {
	mac_->debug ("At %f in Mac %d link handoff failure\n", NOW, mac_->addr());      
	//mac_->send_link_handoff_failure (mac_->addr(), scan_info_->serving_bsid, peer->getPeerNode());
	scan_info_->handoff_timeout = -1;
      }
#endif
      lost_synch ();
    } else {
      send_registration();
    }
  }
}

/**
 * Send a scanning message to the serving BS
 */
void SSscheduler::send_scan_request ()
{
  Packet *p;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr;
  mac802_16_mob_scn_req_frame *req_frame;
  PeerNode *peer;

  //if the mac is not connected, cannot send the request
  if (mac_->getMacState() != MAC802_16_CONNECTED) {
    mac_->debug ("At %f in Mac %d scan request invalid because MAC is disconnected\n", NOW, mac_->addr());
    return;
  }


  mac_->debug ("At %f in Mac %d enqueue scan request\n", NOW, mac_->addr());

  //create packet for request
  p = mac_->getPacket ();
  ch = HDR_CMN(p);
  wimaxHdr = HDR_MAC802_16(p);
  p->allocdata (sizeof (struct mac802_16_mob_scn_req_frame));
  req_frame = (mac802_16_mob_scn_req_frame*) p->accessdata();
  req_frame->type = MAC_MOB_SCN_REQ;

  req_frame->scan_duration = mac_->macmib_.scan_duration;
  req_frame->interleaving_interval = mac_->macmib_.interleaving;
  req_frame->scan_iteration = mac_->macmib_.scan_iteration;
  req_frame->n_recommended_bs_index = 0;
  req_frame->n_recommended_bs_full = 0;

  ch->size() += Mac802_16pkt::getMOB_SCN_REQ_size(req_frame);
  peer = mac_->getPeerNode_head();  
  wimaxHdr->header.cid = peer->getPrimary()->get_cid();
  peer->getPrimary()->enqueue (p);

  //start reg timeout
  if (t44timer_==NULL) {
    t44timer_ = new WimaxT44Timer (mac_);
  }
  t44timer_->start (mac_->macmib_.t44_timeout);
  nb_scan_req_++;
}

/**
 * Process a scanning response message 
 * @param frame The scanning response frame
 */
void SSscheduler::process_scan_rsp (mac802_16_mob_scn_rsp_frame *frame)
{
  //PeerNode *peer = mac_->getPeerNode_head();
  if (!t44timer_->busy()) {
    //we are receiving the response too late..ignore
    mac_->debug ("At %f in Mac %d, scan response arrives too late\n", NOW, mac_->addr());
    return;
  }


  if (frame->scan_duration != 0) {
    //scanning accepted
    mac_->debug ("At %f in Mac %d, scanning accepted (dur=%d it=%d)\n", NOW, mac_->addr(), frame->scan_duration,frame->scan_iteration );
    //allocate data for scanning
    //scan_info_ = (struct scanning_structure *) malloc (sizeof (struct scanning_structure));
    //store copy of frame
    
    scan_info_->rsp = (struct mac802_16_mob_scn_rsp_frame *) malloc (sizeof (struct mac802_16_mob_scn_rsp_frame));
    memcpy (scan_info_->rsp, frame, sizeof (struct mac802_16_mob_scn_rsp_frame));
    scan_info_->iteration = 0;
    scan_info_->count = frame->start_frame;
    scan_info_->substate = SCAN_PENDING;
    scan_info_->handoff_timeout = 0; 
    scan_info_->serving_bsid = mac_->getPeerNode_head()->getPeerNode();
    scan_info_->nb_rdv_timers = 0;

    //mark all neighbors as not detected
    for (int i = 0 ; i < nbr_db_->getNbNeighbor() ; i++) {
      nbr_db_->getNeighbors()[i]->setDetected(false);
    }

    //schedule timer for rdv time (for now just use full)
    //TBD: add rec_bs_index
    mac_->debug ("\tstart scan in %d frames (%f)\n",frame->start_frame,NOW+frame->start_frame*mac_->getFrameDuration());
    for (int i = 0 ; i < scan_info_->rsp->n_recommended_bs_full ; i++) {
      if (scan_info_->rsp->rec_bs_full[i].scanning_type ==SCAN_ASSOC_LVL1 
	  || scan_info_->rsp->rec_bs_full[i].scanning_type==SCAN_ASSOC_LVL2) {
	debug2 ("Creating timer for bs=%d at time %f\n", 
		scan_info_->rsp->rec_bs_full[i].recommended_bs_id, 
		NOW+mac_->getFrameDuration()*scan_info_->rsp->rec_bs_full[i].rdv_time);
	assert (nbr_db_->getNeighbor (scan_info_->rsp->rec_bs_full[i].recommended_bs_id));
	//get the channel
	int ch = mac_->getChannel (nbr_db_->getNeighbor (scan_info_->rsp->rec_bs_full[i].recommended_bs_id)->getDCD ()->frequency*1000);
	assert (ch!=-1);
	WimaxRdvTimer *timer = new WimaxRdvTimer (mac_, ch);
	scan_info_->rdv_timers[scan_info_->nb_rdv_timers++] = timer;
	timer->start(mac_->getFrameDuration()*scan_info_->rsp->rec_bs_full[i].rdv_time);
      }
    }

  } else {
    mac_->debug ("At %f in Mac %d, scanning denied\n", NOW, mac_->addr());
    //what do I do???
  }

  t44timer_->stop();
  nb_scan_req_ = 0;
}

/**
 * Start/Continue scanning
 */
void SSscheduler::resume_scanning ()
{
  if (scan_info_->iteration == 0) 
    mac_->debug ("At %f in Mac %d, starts scanning\n", NOW, mac_->addr());
  else 
    mac_->debug ("At %f in Mac %d, resume scanning\n", NOW, mac_->addr());
  
  scan_info_->substate = SCANNING;

  //backup current state
  scan_info_->normal_state.state_info = mac_->backup_state();
  if (t1timer_->busy())
    t1timer_->pause();
  scan_info_->normal_state.t1timer = t1timer_;
  if (t2timer_->busy())
    t2timer_->pause();
  scan_info_->normal_state.t2timer = t2timer_;
  if (t6timer_->busy())
    t6timer_->pause();
  scan_info_->normal_state.t6timer = t6timer_;
  if (t12timer_->busy())
    t12timer_->pause();
  scan_info_->normal_state.t12timer = t12timer_;
  if (t21timer_->busy())
    t21timer_->pause();
  scan_info_->normal_state.t21timer = t21timer_;
  if (lostDLMAPtimer_->busy())
    lostDLMAPtimer_->pause();
  scan_info_->normal_state.lostDLMAPtimer = lostDLMAPtimer_;
  if (lostULMAPtimer_->busy())
    lostULMAPtimer_->pause();
  scan_info_->normal_state.lostULMAPtimer = lostULMAPtimer_;
  scan_info_->normal_state.map = map_;

  if (scan_info_->iteration == 0) {
    //reset state
    t1timer_ = new WimaxT1Timer (mac_);
    t2timer_ = new WimaxT2Timer (mac_);
    t6timer_ = new WimaxT6Timer (mac_);
    t12timer_ = new WimaxT12Timer (mac_);
    t21timer_ = new WimaxT21Timer (mac_);
    lostDLMAPtimer_ = new WimaxLostDLMAPTimer (mac_);
    lostULMAPtimer_ = new WimaxLostULMAPTimer (mac_);
    
    map_ = new FrameMap (mac_);
    
    mac_->nextChannel();

    scan_info_->scn_timer_ = new WimaxScanIntervalTimer (mac_);

    //start waiting for DL synch
    mac_->setMacState (MAC802_16_WAIT_DL_SYNCH);
    t21timer_->start (mac_->macmib_.t21_timeout);
    if (dl_timer_->status()==TIMER_PENDING)
      dl_timer_->cancel();
    map_->getDlSubframe()->getTimer()->reset();
    if (ul_timer_->status()==TIMER_PENDING)
      ul_timer_->cancel();
    map_->getUlSubframe()->getTimer()->reset();
    

  }else{
    //restore where we left
    //restore previous timers
    mac_->restore_state(scan_info_->scan_state.state_info);
    t1timer_ = scan_info_->scan_state.t1timer;
    if (t1timer_->paused())
      t1timer_->resume();
    t2timer_ = scan_info_->scan_state.t2timer;
    if (t2timer_->paused())
      t2timer_->resume();
    t6timer_ = scan_info_->scan_state.t6timer;
    if (t6timer_->paused())
      t6timer_->resume();
    t12timer_ = scan_info_->scan_state.t12timer;
    if (t12timer_->paused())
      t12timer_->resume();
    t21timer_ = scan_info_->scan_state.t21timer;
    if (t21timer_->paused())
      t21timer_->resume();
    lostDLMAPtimer_ = scan_info_->scan_state.lostDLMAPtimer;
    if (lostDLMAPtimer_->paused())
      lostDLMAPtimer_->resume();
    lostULMAPtimer_ = scan_info_->scan_state.lostULMAPtimer;
    if (lostULMAPtimer_->paused())
      lostULMAPtimer_->resume();
    map_ = scan_info_->scan_state.map;
    
    mac_->getPhy()->setMode (OFDM_RECV);

    if (ul_timer_->status()==TIMER_PENDING)
      ul_timer_->cancel();
  }
  mac_->setNotify_upper (false);
  //printf ("Scan duration=%d, frameduration=%f\n", scan_info_->rsp->scan_duration, mac_->getFrameDuration());
  scan_info_->scn_timer_->start (scan_info_->rsp->scan_duration*mac_->getFrameDuration());
  scan_info_->iteration++;
  
}

/**
 * Pause scanning
 */
void SSscheduler::pause_scanning ()
{
  if (scan_info_->iteration < scan_info_->rsp->scan_iteration)
    mac_->debug ("At %f in Mac %d, pause scanning\n", NOW, mac_->addr());
  else 
    mac_->debug ("At %f in Mac %d, stop scanning\n", NOW, mac_->addr());

  //return to normal mode
  if (scan_info_->iteration < scan_info_->rsp->scan_iteration) {
    //backup current state
    scan_info_->scan_state.state_info = mac_->backup_state();
    if (t1timer_->busy())
      t1timer_->pause();
    scan_info_->scan_state.t1timer = t1timer_;
    if (t2timer_->busy())
      t2timer_->pause();
    scan_info_->scan_state.t2timer = t2timer_;
    if (t6timer_->busy())
      t6timer_->pause();
    scan_info_->scan_state.t6timer = t6timer_;
    if (t12timer_->busy())
      t12timer_->pause();
    scan_info_->scan_state.t12timer = t12timer_;
    if (t21timer_->busy())
      t21timer_->pause();
    scan_info_->scan_state.t21timer = t21timer_;
    if (lostDLMAPtimer_->busy())
      lostDLMAPtimer_->pause();
    scan_info_->scan_state.lostDLMAPtimer = lostDLMAPtimer_;
    if (lostULMAPtimer_->busy())
      lostULMAPtimer_->pause();
    scan_info_->scan_state.lostULMAPtimer = lostULMAPtimer_;
    scan_info_->scan_state.map = map_;

    scan_info_->count = scan_info_->rsp->interleaving_interval;

  } else {
    //else scanning is over, no need to save data
    //reset timers
    if (t1timer_->busy()!=0)
      t1timer_->stop();
    delete (t1timer_);
    if (t12timer_->busy()!=0)
      t12timer_->stop();
    delete (t12timer_);
    if (t21timer_->busy()!=0)
      t21timer_->stop();
    delete (t21timer_);
    if (lostDLMAPtimer_->busy()!=0)
      lostDLMAPtimer_->stop(); 
    delete (lostDLMAPtimer_);
    if (lostULMAPtimer_->busy()!=0)
      lostULMAPtimer_->stop(); 
    delete (lostULMAPtimer_);
    if (t2timer_->busy()!=0)
      t2timer_->stop(); 
    delete (t2timer_);
  }
  //restore previous timers
  mac_->restore_state(scan_info_->normal_state.state_info);
  t1timer_ = scan_info_->normal_state.t1timer;
  if (t1timer_->paused())
    t1timer_->resume();
  t2timer_ = scan_info_->normal_state.t2timer;
  if (t2timer_->paused())
    t2timer_->resume();
  t6timer_ = scan_info_->normal_state.t6timer;
  if (t6timer_->paused())
    t6timer_->resume();
  t12timer_ = scan_info_->normal_state.t12timer;
  if (t12timer_->paused())
    t12timer_->resume();
  t21timer_ = scan_info_->normal_state.t21timer;
  if (t21timer_->paused())
    t21timer_->resume();
  lostDLMAPtimer_ = scan_info_->normal_state.lostDLMAPtimer;
  if (lostDLMAPtimer_->paused())
    lostDLMAPtimer_->resume();
  lostULMAPtimer_ = scan_info_->normal_state.lostULMAPtimer;
  if (lostULMAPtimer_->paused())
    lostULMAPtimer_->resume();
  map_ = scan_info_->normal_state.map;

  mac_->setNotify_upper (true);
  dl_timer_->resched (0);

  if (scan_info_->iteration == scan_info_->rsp->scan_iteration) {
    scan_info_->substate = NORMAL;
    
    /** here we check if there is a better BS **/
#ifdef USE_802_21
    if(mac_->mih_){
      int nbDetected = 0;
      for (int i = 0 ; i < nbr_db_->getNbNeighbor() ; i++) {
	if (nbr_db_->getNeighbors()[i]->isDetected()) {
	  nbDetected++;
	}
      }
      int *listOfPoa = new int[nbDetected];
      int itr = 0;
      for (int i = 0 ; i < nbr_db_->getNbNeighbor() ; i++) {
	WimaxNeighborEntry *entry = nbr_db_->getNeighbors()[i];
	if (entry->isDetected()) {
	  listOfPoa[itr] = entry->getID();
	  itr++;
	}  
      }  
      mac_->send_scan_result (listOfPoa, itr*sizeof(int));	
    }else
#endif
    {
      send_msho_req();
    }

    resetScanFlag();
    scan_info_->count--; //to avoid restarting scanning
    //delete (scan_info_);
    //scan_info_ = NULL;
  } else {
    scan_info_->substate = SCAN_PENDING;
  }

}

/**
 * Send a MSHO-REQ message to the BS
 */
void SSscheduler::send_msho_req ()
{
  Packet *p;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr;
  mac802_16_mob_msho_req_frame *req_frame;
  double rssi;

  PeerNode *peer = mac_->getPeerNode_head();

  int nbPref = 0;
  for (int i = 0 ; i < nbr_db_->getNbNeighbor() ; i++) {
    WimaxNeighborEntry *entry = nbr_db_->getNeighbors()[i];
    if (entry->isDetected()) {
      mac_->debug ("At %f in Mac %d Found new AP %d..need to send HO message\n",NOW, mac_->addr(), entry->getID());
      nbPref++;
    }  
  }

  if (nbPref==0)
    return; //no other BS found

  //create packet for request
  p = mac_->getPacket ();
  ch = HDR_CMN(p);
  wimaxHdr = HDR_MAC802_16(p);
  p->allocdata (sizeof (struct mac802_16_mob_msho_req_frame)+nbPref*sizeof (mac802_16_mob_msho_req_bs_index));
  req_frame = (mac802_16_mob_msho_req_frame*) p->accessdata();
  memset (req_frame, 0, sizeof (mac802_16_mob_msho_req_bs_index));
  req_frame->type = MAC_MOB_MSHO_REQ;
  
  req_frame->report_metric = 0x2; //include RSSI
  req_frame->n_new_bs_index = 0;
  req_frame->n_new_bs_full = nbPref;
  req_frame->n_current_bs = 1;
  rssi = mac_->getPeerNode_head()->getStatWatch()->average();
  debug2 ("RSSI=%e, %d, bs=%d\n", rssi, (u_char)((rssi+103.75)/0.25), mac_->getPeerNode_head()->getPeerNode());
  req_frame->bs_current[0].temp_bsid = mac_->getPeerNode_head()->getPeerNode();
  req_frame->bs_current[0].bs_rssi_mean = (u_char)((rssi+103.75)/0.25);
  for (int i = 0, j=0; i < nbr_db_->getNbNeighbor() ; i++) {
    WimaxNeighborEntry *entry = nbr_db_->getNeighbors()[i];
    //TBD: there is an error measuring RSSI for current BS during scanning
    //anyway, we don't put it in the least, so it's ok for now
    if (entry->isDetected() && entry->getID()!= mac_->getPeerNode_head()->getPeerNode()) {
      req_frame->bs_full[j].neighbor_bs_index = entry->getID();
      rssi = entry->getState()->state_info->peer_list->lh_first->getStatWatch()->average();
      debug2 ("RSSI=%e, %d, bs=%d\n", rssi, (u_char)((rssi+103.75)/0.25), entry->getID());
      req_frame->bs_full[j].bs_rssi_mean = (u_char)((rssi+103.75)/0.25);
      //the rest of req_frame->bs_full is unused for now..
      req_frame->bs_full[j].arrival_time_diff_ind = 0;
      j++;
    }
  }
  
  ch->size() += Mac802_16pkt::getMOB_MSHO_REQ_size(req_frame);
  wimaxHdr->header.cid = peer->getPrimary()->get_cid();
  peer->getPrimary()->enqueue (p);
}

/**
 * Process a BSHO-RSP message 
 * @param frame The handover response frame
 */
void SSscheduler::process_bsho_rsp (mac802_16_mob_bsho_rsp_frame *frame)
{
  mac_->debug ("At %f in Mac %d, received handover response\n", NOW, mac_->addr());
 
  //go and switch to the channel recommended by the BS
  int targetBS = frame->n_rec[0].neighbor_bsid;
  PeerNode *peer = mac_->getPeerNode_head();      

  if (peer->getPeerNode ()==targetBS) {
    mac_->debug ("\tDecision to stay in current BS\n");
    return;
  }
  scan_info_->nbr = nbr_db_->getNeighbor (targetBS);

  Packet *p;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr;
  mac802_16_mob_ho_ind_frame *ind_frame;
  
  
  p = mac_->getPacket ();
  ch = HDR_CMN(p);
  wimaxHdr = HDR_MAC802_16(p);
  p->allocdata (sizeof (struct mac802_16_mob_ho_ind_frame));
  ind_frame = (mac802_16_mob_ho_ind_frame*) p->accessdata();
  ind_frame->type = MAC_MOB_HO_IND;
  
  ind_frame->mode = 0; //HO
  ind_frame->ho_ind_type = 0; //Serving BS release
  ind_frame->rng_param_valid_ind = 0;
  ind_frame->target_bsid = targetBS;
  
  ch->size() += Mac802_16pkt::getMOB_HO_IND_size(ind_frame);
  wimaxHdr->header.cid = peer->getPrimary()->get_cid();
  peer->getPrimary()->enqueue (p);
  
#ifdef USE_802_21
  mac_->send_link_handover_imminent (mac_->addr(), peer->getPeerNode(), targetBS);
  mac_->debug ("At %f in Mac %d link handover imminent\n", NOW, mac_->addr());
#endif 
  
  mac_->debug ("\tHandover to BS %d\n", targetBS);
  scan_info_->handoff_timeout = 20;
  scan_info_->substate = HANDOVER_PENDING;
  
  //mac_->setChannel (scan_info_->bs_infos[i].channel);
  //lost_synch ();  
}

/*
 * Connect to the PoA
 */
void SSscheduler::link_connect(int poa)
{
  mac_->debug ("At %f in Mac %d, received link connect to BS %d\n", NOW, mac_->addr(), poa);
 
  //go and switch to the channel recommended by the BS
  int targetBS = poa;
  PeerNode *peer = mac_->getPeerNode_head();      

  if (peer->getPeerNode ()==targetBS) {
    mac_->debug ("\tDecision to stay in current BS\n");
    return;
  }
  scan_info_->nbr = nbr_db_->getNeighbor (targetBS);

  Packet *p;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr;
  mac802_16_mob_ho_ind_frame *ind_frame;
  
  
  p = mac_->getPacket ();
  ch = HDR_CMN(p);
  wimaxHdr = HDR_MAC802_16(p);
  p->allocdata (sizeof (struct mac802_16_mob_ho_ind_frame));
  ind_frame = (mac802_16_mob_ho_ind_frame*) p->accessdata();
  ind_frame->type = MAC_MOB_HO_IND;
  
  ind_frame->mode = 0; //HO
  ind_frame->ho_ind_type = 0; //Serving BS release
  ind_frame->rng_param_valid_ind = 0;
  ind_frame->target_bsid = targetBS;
  
  ch->size() += Mac802_16pkt::getMOB_HO_IND_size(ind_frame);
  wimaxHdr->header.cid = peer->getPrimary()->get_cid();
  peer->getPrimary()->enqueue (p);
  
#ifdef USE_802_21
  mac_->send_link_handover_imminent (mac_->addr(), peer->getPeerNode(), targetBS);
  mac_->debug ("At %f in Mac %d link handover imminent\n", NOW, mac_->addr());
  
#endif 
  
  mac_->debug ("\tHandover to BS %d\n", targetBS);
  scan_info_->handoff_timeout = 20;
  scan_info_->substate = HANDOVER_PENDING;
  //mac_->setChannel (scan_info_->bs_infos[i].channel);
  //lost_synch ();    
}

/**
 * Process a NBR_ADV message 
 * @param frame The handover response frame
 */
void SSscheduler::process_nbr_adv (mac802_16_mob_nbr_adv_frame *frame)
{
  mac_->debug ("At %f in Mac %d, received neighbor advertisement\n", NOW, mac_->addr());

  mac802_16_mob_nbr_adv_frame *copy;

  copy  = (mac802_16_mob_nbr_adv_frame *) malloc (sizeof (mac802_16_mob_nbr_adv_frame));
  memcpy (copy, frame, sizeof (mac802_16_mob_nbr_adv_frame));
  //all we need is to store the information. We will process that only
  //when we will look for another station
  for (int i = 0 ; i < frame->n_neighbors ; i++) {
    int nbrid = frame->nbr_info[i].nbr_bsid;
    mac802_16_nbr_adv_info *info = (mac802_16_nbr_adv_info *) malloc (sizeof(mac802_16_nbr_adv_info));
    WimaxNeighborEntry *entry = nbr_db_->getNeighbor (nbrid);
    if (entry==NULL){
      entry = new WimaxNeighborEntry (nbrid);
      nbr_db_->addNeighbor (entry);
    }
    memcpy(info, &(frame->nbr_info[i]), sizeof(mac802_16_nbr_adv_info));
    if (entry->getNbrAdvMessage ())
      free (entry->getNbrAdvMessage());
    entry->setNbrAdvMessage(info);
    if (info->dcd_included) {
      //set DCD 
      mac802_16_dcd_frame *tmp = (mac802_16_dcd_frame *)malloc (sizeof(mac802_16_dcd_frame));
      memcpy(tmp, &(info->dcd_settings), sizeof(mac802_16_dcd_frame));
      entry->setDCD(tmp);
    }
    else 
      entry->setDCD(NULL);
    if (info->ucd_included) {
      //set DCD 
      mac802_16_ucd_frame *tmp = (mac802_16_ucd_frame *)malloc (sizeof(mac802_16_ucd_frame));
      memcpy(tmp, &(info->ucd_settings), sizeof(mac802_16_ucd_frame));
      entry->setUCD(tmp);
#ifdef DEBUG_WIMAX
      debug2 ("Dump information nbr in Mac %d for nbr %d %lx\n", mac_->addr(), nbrid, (long)tmp);
      int nb_prof = tmp->nb_prof;
      mac802_16_ucd_profile *profiles = tmp->profiles;
      for (int i = 0 ; i < nb_prof ; i++) {
	debug2 ("\t Reading ul profile %i: f=%d, rate=%d, iuc=%d\n", i, 0, profiles[i].fec, profiles[i].uiuc);
      }
#endif
    }
    else
      entry->setUCD(NULL);
  }  

}


void 
SSscheduler::setScanFlag()
{
  scanFlag = true;
}

void 
SSscheduler::resetScanFlag()
{
  scanFlag = false;
}

bool
SSscheduler::isScanRunning()
{
  return scanFlag;
}
