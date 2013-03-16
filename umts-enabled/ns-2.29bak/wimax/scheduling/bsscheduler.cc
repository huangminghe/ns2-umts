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

#include "bsscheduler.h"
#include "burst.h"
#include "dlburst.h"
#include "ulburst.h"
#include "random.h"
#include "wimaxctrlagent.h"

/**
 * Bridge to TCL for BSScheduler
 */
static class BSSchedulerClass : public TclClass {
public:
  BSSchedulerClass() : TclClass("WimaxScheduler/BS") {}
  TclObject* create(int, const char*const*) {
    return (new BSScheduler());
    
  }
} class_bsscheduler;

/*
 * Create a scheduler
 */
BSScheduler::BSScheduler () : cl_head_(0), cl_tail_(0), ctrlagent_(0)
{
  debug2 ("BSScheduler created\n");
  LIST_INIT (&t17_head_);
  LIST_INIT (&scan_stations_);
  LIST_INIT (&fast_ranging_head_);
  bw_peer_ = NULL;
  bw_node_index_ = 0;
  default_mod_ = OFDM_BPSK_1_2;
  contention_size_ = MIN_CONTENTION_SIZE;
  sendDCD = false;
  dlccc_ = 0;
  sendUCD = false;
  ulccc_ = 0;
}
 
/*
 * Interface with the TCL script
 * @param argc The number of parameter
 * @param argv The list of parameters
 */
int BSScheduler::command(int argc, const char*const* argv)
{
  if (argc == 3) {
    if (strcmp(argv[1], "set-default-modulation") == 0) {
      if (strcmp(argv[2], "OFDM_BPSK_1_2") == 0)
	default_mod_ = OFDM_BPSK_1_2;
      else if (strcmp(argv[2], "OFDM_QPSK_1_2") == 0)
	default_mod_ = OFDM_QPSK_1_2;
      else if (strcmp(argv[2], "OFDM_QPSK_3_4") == 0)
	default_mod_ = OFDM_QPSK_3_4;
      else if (strcmp(argv[2], "OFDM_16QAM_1_2") == 0)
	default_mod_ = OFDM_16QAM_1_2;
      else if (strcmp(argv[2], "OFDM_16QAM_3_4") == 0)
	default_mod_ = OFDM_16QAM_3_4;
      else if (strcmp(argv[2], "OFDM_64QAM_2_3") == 0)
	default_mod_ = OFDM_64QAM_2_3;
      else if (strcmp(argv[2], "OFDM_64QAM_3_4") == 0)
	default_mod_ = OFDM_64QAM_3_4;
      else
	return TCL_ERROR;
      return TCL_OK;
    }
    else if (strcmp(argv[1], "set-contention-size") == 0) {
      contention_size_ = atoi (argv[2]);
      assert (contention_size_>=0);
      return TCL_OK;
    }
  }
  return TCL_ERROR;
}

/**
 * Initializes the scheduler
 */
void BSScheduler::init ()
{
  WimaxScheduler::init();

  /*print debug information */
  int nbPS = (int) round((mac_->getFrameDuration()/mac_->getPhy()->getPS()));
  debug2 ("duration=%f, PStime=%f, nbPS=%d,Symbol time=%f, %d \n",mac_->getFrameDuration(), mac_->getPhy()->getPS(), nbPS, mac_->getPhy()->getSymbolTime (), mac_->getPhy()->getSymbolPS());

  //At initialization, allocate one DL burst 
  //and contention windows
  //1-create one profile to be used for broadcast 
  //  packets (and DL_MAP...)
  Profile *p = map_->getDlSubframe()->addProfile ((int)round((mac_->getPhy()->getFreq()/1000)), default_mod_);
  p->setIUC (DIUC_PROFILE_1);
  map_->getDlSubframe()->getPdu ()->setPreamble (DL_PREAMBLE); //preamble + fch
  //2-add the burst to carry the broadcast message
  DlBurst *b = (DlBurst*) map_->getDlSubframe()->getPdu ()->addBurst (0);
  b->setCid ( BROADCAST_CID );
  b->setIUC (p->getIUC());
  b->setStarttime (0); //after preamble and fch
  b->setDuration (INIT_DL_DURATION); //enough to send DL_MAP...
  b->setPreamble(true); //this is the first burst after preamble
  //3-Add the End of map element
  b = (DlBurst*) map_->getDlSubframe()->getPdu ()->addBurst (1);
  b->setIUC (DIUC_END_OF_MAP);
  b->setStarttime (INIT_DL_DURATION);

  //create uplink 
  //start of UL subframe is after DL and TTG and unit is PS
  int starttime = (INIT_DL_DURATION+DL_PREAMBLE)*mac_->getPhy()->getSymbolPS()+mac_->phymib_.ttg;
  map_->getUlSubframe()->setStarttime (starttime);
  int slotleft = nbPS - starttime - mac_->phymib_.rtg;
  //duration is in unit of ofdm symbols
  int rangingslot = slotleft/2;
  int rangingduration =(int) round(((mac_->getPhy()->getPS()*rangingslot)/mac_->getPhy()->getSymbolTime()));
  int bwduration = (int) round(((mac_->getPhy()->getPS()*(slotleft - rangingslot))/mac_->getPhy()->getSymbolTime()));

  //we open the uplink to initial ranging and bw requests
  //add profile for the ranging burst
  p = map_->getUlSubframe()->addProfile (0, default_mod_);
  p->setIUC (UIUC_INITIAL_RANGING);
  ContentionSlot *slot = map_->getUlSubframe()->getRanging ();
  slot->setSize (getInitRangingopportunity ());
  slot->setBackoff_start (mac_->macmib_.rng_backoff_start);
  slot->setBackoff_stop (mac_->macmib_.rng_backoff_stop);
  //create burst to represent the contention slot
  Burst* b2 = map_->getUlSubframe()->addPhyPdu (0,0)->addBurst (0);
  b2->setIUC (UIUC_INITIAL_RANGING);
  b2->setDuration (rangingduration);
  b2->setStarttime (0); //we put the contention at the begining

  //now the bw request
  //add profile for the bw burst
  p = map_->getUlSubframe()->addProfile (0, default_mod_);
  p->setIUC (UIUC_REQ_REGION_FULL);  
  slot = map_->getUlSubframe()->getBw_req ();
  slot->setSize (getBWopportunity ());
  slot->setBackoff_start (mac_->macmib_.bw_backoff_start);
  slot->setBackoff_stop (mac_->macmib_.rng_backoff_stop);
  b2 = map_->getUlSubframe()->addPhyPdu (1,0)->addBurst (0);
  b2->setIUC (UIUC_REQ_REGION_FULL);
  b2->setDuration (bwduration);
  b2->setStarttime (rangingduration); //start after the ranging slot

  //end of map
  b2 = map_->getUlSubframe()->addPhyPdu (2,0)->addBurst (0);
  b2->setIUC (UIUC_END_OF_MAP);
  b2->setStarttime (rangingduration+bwduration);

  //schedule the first frame by using a random backoff to avoid
  //synchronization between BSs.
  double stime = mac_->getFrameDuration () + Random::uniform(0, mac_->getFrameDuration ());
  dl_timer_->sched (stime);

  ul_timer_->sched (stime+starttime*mac_->getPhy()->getPS());

  //also start the DCD and UCD timer
  dcdtimer_ = new WimaxDCDTimer (mac_);
  ucdtimer_ = new WimaxUCDTimer (mac_);
  nbradvtimer_ = new WimaxMobNbrAdvTimer (mac_);
  dcdtimer_->start (mac_->macmib_.dcd_interval);
  ucdtimer_->start (mac_->macmib_.ucd_interval);
  nbradvtimer_->start (mac_->macmib_.nbr_adv_interval+stime);
}

/**
 * Compute and return the bandwidth request opportunity size
 * @return The bandwidth request opportunity size
 */
int BSScheduler::getBWopportunity ()
{
  int nbPS = BW_REQ_PREAMBLE * mac_->getPhy()->getSymbolPS();
  //add PS for carrying header
  nbPS += (int) round((mac_->getPhy()->getTrxTime (HDR_MAC802_16_SIZE, map_->getUlSubframe()->getProfile(UIUC_REQ_REGION_FULL)->getEncoding())/mac_->getPhy()->getPS ()));
  //printf ("BWopportunity size=%d\n", nbPS);
  return nbPS;
}

/**
 * Compute and return the initial ranging opportunity size
 * @return The initial ranging opportunity size
 */
int BSScheduler::getInitRangingopportunity ()
{
  int nbPS = INIT_RNG_PREAMBLE * mac_->getPhy()->getSymbolPS();
  //add PS for carrying header
  nbPS += (int) round((mac_->getPhy()->getTrxTime (RNG_REQ_SIZE+HDR_MAC802_16_SIZE, map_->getUlSubframe()->getProfile(UIUC_INITIAL_RANGING)->getEncoding())/mac_->getPhy()->getPS ()));
  //printf ("Init ranging opportunity size=%d\n", nbPS);
  return nbPS;  
}

/**
 * Called when a timer expires
 * @param The timer ID
 */
void BSScheduler::expire (timer_id id)
{
  switch (id) {
  case WimaxDCDTimerID:
    sendDCD = true;
    mac_->debug ("At %f in Mac %d DCDtimer expired\n", NOW, mac_->addr());
    dcdtimer_->start (mac_->macmib_.dcd_interval);
    break;
  case WimaxUCDTimerID:
    sendUCD = true;
    mac_->debug ("At %f in Mac %d UCDtimer expired\n", NOW, mac_->addr());
    ucdtimer_->start (mac_->macmib_.ucd_interval);
    break;
  case WimaxMobNbrAdvTimerID:
    send_nbr_adv();
    nbradvtimer_->start (mac_->macmib_.nbr_adv_interval);
    break;
  default:
    mac_->debug ("Warning: unknown timer expired in BSScheduler\n");
  }
}

/**
 * Set the control agent
 */
void BSScheduler::setCtrlAgent (WimaxCtrlAgent *agent)
{
  assert (agent);
  ctrlagent_ = agent;
}

/**
 * Process a packet received by the Mac. Only scheduling related packets should be sent here (BW request, UL_MAP...)
 * @param p The packet to process
 */
void BSScheduler::process (Packet * p)
{
  //assert (mac_);
  //debug2 ("BSScheduler received packet to process\n");

  assert (mac_ && HDR_CMN(p)->ptype()==PT_MAC);
  debug2 ("BSScheduler received packet to process\n");
  
  hdr_mac802_16 *wimaxHdr = HDR_MAC802_16(p);
  gen_mac_header_t header = wimaxHdr->header;

  //check if this is a bandwidth request
  if (header.ht == 1) {
    process_bw_req (p);
    return;
  }

  //we cast to this frame because all management frame start with
  //a type 
  mac802_16_dl_map_frame *frame = (mac802_16_dl_map_frame*) p->accessdata();
  
  switch (frame->type) {
  case MAC_RNG_REQ: 
    process_ranging_req (p);
    break;
  case MAC_REG_REQ: 
    process_reg_req (p);
    break;
  case MAC_MOB_SCN_REQ:
    if (ctrlagent_) 
      ctrlagent_->process_scan_request (p);
    else
      fprintf (stderr, "Warning: no controler to handle scan request in BS %d\n", mac_->addr());
    break;
  case MAC_MOB_MSHO_REQ:
    process_msho_req (p);
    break;
  case MAC_MOB_HO_IND:
    process_ho_ind (p);
    break;
  default:
    mac_->debug ("unknown packet in BS\n");
  }

  //Packet::free (p);
}


/**
 * Return the type of STA this scheduler is good for
 * @return STA_BS
 */
station_type_t BSScheduler::getNodeType ()
{
  return STA_BS;
}

/**
 * Start a new frame
 */
void BSScheduler::start_ulsubframe ()
{
  //mac_->debug ("At %f in Mac %d BS scheduler ulsubframe expires\n", NOW, mac_->addr());

  //change PHY state
  mac_->getPhy()->setMode (OFDM_RECV);  

  //start handler of ulsubframe
  map_->getUlSubframe()->getTimer()->sched (0);

  //reschedule for next frame
  ul_timer_->resched (mac_->getFrameDuration());
}

/**
 * Start a new frame
 */
void BSScheduler::start_dlsubframe ()
{
  debug2 ("At %f in Mac %d BS scheduler dlsubframe expires (frame=%d)\n", 
	       NOW, mac_->addr(), mac_->frame_number_+1);

  assert (map_);

  Packet *p;
  Burst *b;
  struct hdr_cmn *ch;
  double txtime;
  int txtime_s;
  OFDMPhy *phy = mac_->getPhy();

  //increment frame number
  mac_->frame_number_ ++;
  
  //adjust frame start information
  map_->setStarttime(NOW);

  /* First lets clear the peers we haven't heard of for long time */
  for (PeerNode *pn = mac_->getPeerNode_head() ; pn ; ) {
    PeerNode *tmp = pn->next_entry(); //next elem
    if (isPeerScanning(pn->getPeerNode())) {
      //since a scanning node cannot send data we push the 
      //timeout while it is scanning
      pn->setRxTime(NOW); 
    } else if (NOW-pn->getRxTime()>mac_->macmib_.client_timeout) {
      mac_->debug ("Client timeout for node %d\n", pn->getPeerNode());
      mac_->removePeerNode(pn);
    }
    pn = tmp;
  }

  /**** Step one : burst allocation ****/
  int nbPS = (int) round((mac_->getFrameDuration()/phy->getPS()));
  int nbPS_left = nbPS - mac_->phymib_.rtg - mac_->phymib_.ttg;
  int nbSymbols = (int) ((phy->getPS()*nbPS_left)/phy->getSymbolTime());
  int dlduration = INIT_DL_DURATION+DL_PREAMBLE;
  int ulduration = 0;
  debug2 ("Frame: duration=%f, PSduration=%e, symboltime=%e, nbPS=%d, rtg=%d, ttg=%d, PSleft=%d, nbSymbols=%d, ", \
    mac_->getFrameDuration(), phy->getPS(), phy->getSymbolTime(), nbPS, mac_->phymib_.rtg, mac_->phymib_.ttg, nbPS_left, nbSymbols);

  //remove control messages
  nbSymbols -= INIT_DL_DURATION+DL_PREAMBLE;
  
  nbSymbols -=10;
  for (Connection *c = mac_->getCManager()->get_down_connection (); c && nbSymbols>0 ; c=c->next_entry()) {
    if (c->getPeerNode() && !isPeerScanning (c->getPeerNode()->getPeerNode())) {
      int queuesize = c->queueByteLength();
      int tmp = (int) round((phy->getTrxTime (queuesize, map_->getDlSubframe()->getProfile (DIUC_PROFILE_1)->getEncoding())/phy->getSymbolTime()));
      if (tmp < nbSymbols) {
	dlduration += tmp;
	nbSymbols -= tmp;
      }else{
	dlduration +=nbSymbols;
	nbSymbols -= nbSymbols;
	break;
      }
    }
  }
  nbSymbols +=10;

  map_->getDlSubframe()->getPdu()->getBurst(0)->setDuration (dlduration);
  map_->getUlSubframe()->setStarttime (dlduration*phy->getSymbolPS()+mac_->phymib_.rtg);
  ul_timer_->resched (map_->getUlSubframe()->getStarttime()*mac_->getPhy()->getPS());

  debug2 ("dlduration=%d, ", dlduration);
  //update the END_OF_MAP start time
  b = (DlBurst*) map_->getDlSubframe()->getPdu ()->getBurst (1);
  b->setStarttime (dlduration);  

  while (map_->getUlSubframe()->getNbPdu()>0) {
    PhyPdu *pdu = map_->getUlSubframe()->getPhyPdu(0);
    map_->getUlSubframe()->removePhyPdu(pdu);
    delete (pdu);
  }
  
  int rangingduration = 0;
  int bwduration = 0;
  int pduIndex = 0;
  debug2 ("In Mac %d Nb symbols left before contention =%d\n", mac_->addr(), nbSymbols);

  int contentionslots = (int) round((contention_size_*((getBWopportunity()+getInitRangingopportunity())*mac_->getPhy()->getPS()/mac_->getPhy()->getSymbolTime())));
  if (nbSymbols > contentionslots) {
    int starttime, slotleft, rangingslot, bwslot;
    //create uplink 
    //start of UL subframe is after DL and TTG and unit is PS
    starttime = map_->getUlSubframe()->getStarttime();
    slotleft = nbPS - starttime - mac_->phymib_.rtg;
    //if there is at least one peer, then use only contention_size_ symbols, 
    //otherwise use all the bw
    if (mac_->getPeerNode_head() || fast_ranging_head_.lh_first) {
      rangingslot = contention_size_*getInitRangingopportunity();
      bwslot = contention_size_*getBWopportunity();
      nbSymbols -= contentionslots;
      debug2 ("nbSymbols after contention=%d\n", nbSymbols);
    }
    else {
      rangingslot = (int) (floor (slotleft/(2.0*getInitRangingopportunity()))*getInitRangingopportunity());
      bwslot = (int) (floor ((slotleft-rangingslot)/getBWopportunity())*getBWopportunity());
      nbSymbols = 0;
    }    
    rangingduration =(int) round(((mac_->getPhy()->getPS()*rangingslot)/mac_->getPhy()->getSymbolTime()));
    bwduration = (int) round (((mac_->getPhy()->getPS()*bwslot)/mac_->getPhy()->getSymbolTime()));
    //we open the uplink to initial ranging and bw requests
    ContentionSlot *slot = map_->getUlSubframe()->getRanging ();
    //create burst to represent the contention slot
    Burst* b2 = map_->getUlSubframe()->addPhyPdu (pduIndex,0)->addBurst (0);
    pduIndex++;
    b2->setIUC (UIUC_INITIAL_RANGING);
    b2->setDuration (rangingduration);
    b2->setStarttime (ulduration); //we put the contention at the begining
    ulduration += rangingduration;
    
    //now the bw request
    slot = map_->getUlSubframe()->getBw_req ();
    b2 = map_->getUlSubframe()->addPhyPdu (pduIndex,0)->addBurst (0);
    pduIndex++;
    b2->setIUC (UIUC_REQ_REGION_FULL);
    b2->setDuration (bwduration);
    b2->setStarttime (ulduration); //start after the ranging slot
    ulduration += bwduration;
  }
  
  //check if there is Fast Ranging allocation to do
  FastRangingInfo *next_info = NULL;
  for (FastRangingInfo *info = fast_ranging_head_.lh_first ; info ; info=next_info) {
    //get next info before the entry is removed from list
    next_info = info->next_entry();
    if (info->frame() == mac_->getFrameNumber()) {
      //we need to include a fast ranging allocation
      b = map_->getUlSubframe()->addPhyPdu (pduIndex,0)->addBurst (0);
      pduIndex++;
      int tmp =(int) round(((mac_->getPhy()->getPS()*getInitRangingopportunity())/mac_->getPhy()->getSymbolTime()));    
      b->setIUC (UIUC_EXT_UIUC);
      b->setDuration (tmp);
      b->setStarttime (ulduration); //start after previous slot
      ((UlBurst*)b)->setFastRangingParam (info->macAddr(), UIUC_INITIAL_RANGING);
      ulduration += tmp;
      mac_->debug ("At %f in Mac %d adding fast ranging for %d\n", NOW, mac_->addr(), info->macAddr());
      info->remove_entry();
    }
  }

  //get the next node to allocate bw. 
  //PB: the node may have been removed. We need to check that
  PeerNode *peer = mac_->getPeerNode_head();
  PeerNode *start_peer = mac_->getPeerNode_head(); 
  int i=0;

  if (start_peer != NULL) {
    //we have at least one element in the list
    for (peer = mac_->getPeerNode_head(); peer->next_entry() ; peer=peer->next_entry());
    debug2 ("start_peer=%d, peer=%d\n", start_peer->getPeerNode(), peer->getPeerNode());
    while (start_peer != peer) {
      if (!isPeerScanning(peer->getPeerNode())) {
	debug2 ("peer %d not scanning take it\n", peer->getPeerNode());
	break; //we found next station
      }
      debug2 ("peer %d scanning move it\n", peer->getPeerNode());
      peer->remove_entry();
      mac_->addPeerNode (peer); //we put it at head of the list
      for (peer = mac_->getPeerNode_head(); peer->next_entry() ; peer=peer->next_entry());
    }
  }
  if (peer)
    debug2 ("final pick is %d\n", peer->getPeerNode());

  //update variables
  bw_node_index_ = i;
  bw_peer_ = peer;
  
  //the next peer node takes the rest of the bw
  if (nbSymbols > 0 && peer && !isPeerScanning(peer->getPeerNode())) {
    debug2 ("Allocate uplink for STA %d\n", peer->getPeerNode());
    peer->remove_entry();
    mac_->addPeerNode (peer); //we put it at head of the list
    //printf ("NbSymbols for node=%d\n", nbSymbols);
    //add burst
    Burst *b3 = map_->getUlSubframe()->addPhyPdu (pduIndex,0)->addBurst (0);
    pduIndex++;
    b3->setIUC (UIUC_PROFILE_1);
    b3->setCid (peer->getPrimary()->get_cid());
    b3->setDuration (nbSymbols);
    b3->setStarttime (ulduration); 
    //add profile if not existing
    Profile *p = map_->getUlSubframe()->getProfile (UIUC_PROFILE_1);
    if (p==NULL) {
      p = map_->getUlSubframe()->addProfile (0, default_mod_);
      p->setIUC (UIUC_PROFILE_1);
    }
  }
  
  //end of map
  Burst *b2 = map_->getUlSubframe()->addPhyPdu (pduIndex,0)->addBurst (0);
  pduIndex++;
  b2->setIUC (UIUC_END_OF_MAP);
  b2->setStarttime (rangingduration+bwduration+nbSymbols);
  


  //we need to fill up the outgoing queues
  for (int index = 0 ; index < map_->getDlSubframe()->getPdu ()->getNbBurst() ; index++) {
    b = map_->getDlSubframe()->getPdu ()->getBurst (index);
    int duration = 0; 

    if (b->getIUC()==DIUC_END_OF_MAP) {
      //consistency check..
      assert (index == map_->getDlSubframe()->getPdu ()->getNbBurst()-1);
      break;
    }
    
    //we can get the next one after we check END_OF_MAP
    //b2 = map_->getDlSubframe()->getPdu ()->getBurst (index+1);

    //if this is the first buffer, add DL_MAP...messages
    if (index==0) {
      p = map_->getDL_MAP();
      ch = HDR_CMN(p);
      txtime = phy->getTrxTime (ch->size(), map_->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
      ch->txtime() = txtime;
      txtime_s = (int) round(txtime/phy->getSymbolTime ()); //in units of symbol 
      assert ((duration+txtime_s) <= b->getDuration());
      ch->timestamp() = NOW; //add timestamp since it bypasses the queue
      b->enqueue(p);      //enqueue into burst
      duration += txtime_s;
      p = map_->getUL_MAP();
      ch = HDR_CMN(p);
      txtime = phy->getTrxTime (ch->size(), map_->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
      ch->txtime() = txtime;
      txtime_s = (int) round(txtime/phy->getSymbolTime ()); //in units of symbol 
      assert ((duration+txtime_s) <= b->getDuration());
      ch->timestamp() = NOW; //add timestamp since it bypasses the queue
      b->enqueue(p);      //enqueue into burst
      duration += txtime_s;

      if (sendDCD || map_->getDlSubframe()->getCCC()!= dlccc_) {
	p = map_->getDCD();
	ch = HDR_CMN(p);
	txtime = phy->getTrxTime (ch->size(), map_->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
	ch->txtime() = txtime;
	txtime_s = (int) round(txtime/phy->getSymbolTime ()); //in units of symbol 
	assert ((duration+txtime_s) <= b->getDuration());
	ch->timestamp() = NOW; //add timestamp since it bypasses the queue
	b->enqueue(p);      //enqueue into burst
	duration += txtime_s;
	sendDCD = false;
	dlccc_ = map_->getDlSubframe()->getCCC();
	//reschedule timer
	dcdtimer_->stop();
	dcdtimer_->start (mac_->macmib_.dcd_interval);
      }
      
      if (sendUCD || map_->getUlSubframe()->getCCC()!= ulccc_) {
	p = map_->getUCD();
	ch = HDR_CMN(p);
	txtime = phy->getTrxTime (ch->size(), map_->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
	ch->txtime() = txtime;
	txtime_s = (int) round(txtime/phy->getSymbolTime ()); //in units of symbol 
	assert ((duration+txtime_s) <= b->getDuration());
	ch->timestamp() = NOW; //add timestamp since it bypasses the queue
	b->enqueue(p);      //enqueue into burst
	duration += txtime_s;
	sendUCD = false;
	ulccc_ = map_->getUlSubframe()->getCCC();
	//reschedule timer
	ucdtimer_->stop();
	ucdtimer_->start (mac_->macmib_.ucd_interval);
      }
    }
      
    //get the packets from the connection with the same CID
    //Connection *c=mac_->getCManager ()->get_connection (b->getCid());
    int nb_down = 0;
    for (Connection *c = mac_->getCManager()->get_down_connection (); c ; c=c->next_entry()){ nb_down++;}
    Connection **tmp_con = (Connection **) malloc (nb_down*sizeof (Connection *));
    int i=0;
    for (Connection *c = mac_->getCManager()->get_down_connection (); c ; c=c->next_entry()){
      tmp_con[i] = c;
      i++;
    }
    //we randomly pick the connection we'll start serving first
    int start_index = (int) round (Random::uniform(0, nb_down));
    debug2 ("Picked connection %d as first\n", start_index);

    //for (Connection *c = mac_->getCManager()->get_down_connection (); c ; c=c->next_entry()) {
    for (i=0; i < nb_down ; i++) {
      int index = (i+start_index)%nb_down;
      Connection *c=tmp_con[index];

      if (c->getPeerNode() && isPeerScanning (c->getPeerNode()->getPeerNode()))
	continue;

      duration = transfer_packets (c, b, duration);
    }
  }

  //change PHY state
  mac_->getPhy()->setMode (OFDM_SEND);

  //start handler of dlsubframe
  map_->getDlSubframe()->getTimer()->sched (0);

  //reschedule for next time (frame duration)
  dl_timer_->resched (mac_->getFrameDuration());  
}

/** Add a new Fast Ranging allocation
 * @param time The time when to allocate data
 * @param macAddr The MN address
 */
void BSScheduler::addNewFastRanging (double time, int macAddr)
{
  //compute the frame where the allocation will be located
  int frame = int ((time-NOW)/mac_->getFrameDuration()) +2 ;
  frame += mac_->getFrameNumber();
  //printf ("Added fast RA for frame %d (current=%d) for time (%f)\n", 
  //	  frame, mac_->getFrameNumber(), time);
  FastRangingInfo *info= new FastRangingInfo (frame, macAddr);
  info->insert_entry(&fast_ranging_head_);
}


/**** Packet processing methods ****/

/**
 * Process a RNG-REQ message
 * @param p The packet containing the ranging request information
 */
void BSScheduler::process_ranging_req (Packet *p)
{
  UlSubFrame *ulsubframe = map_->getUlSubframe();
  mac802_16_rng_req_frame *req = (mac802_16_rng_req_frame *) p->accessdata();

  if (HDR_MAC802_16(p)->header.cid != INITIAL_RANGING_CID) {
    //process request for DIUC
  } else {
    //here we can make decision to accept the SS or not.
    //for now, accept everybody
    //check if CID already assigned for the SS
    PeerNode *peer = mac_->getPeerNode (req->ss_mac_address);
    if (peer==NULL) {
      mac_->debug ("New peer node requesting ranging (%d)\n",req->ss_mac_address);
      //Assign Management CIDs
      Connection *basic = new Connection (CONN_BASIC);
      Connection *upbasic = new Connection (CONN_BASIC, basic->get_cid());
      Connection *primary = new Connection (CONN_PRIMARY);
      Connection *upprimary = new Connection (CONN_PRIMARY, primary->get_cid());
      
      //Create Peer information
      peer = new PeerNode (req->ss_mac_address);
      peer->setBasic (basic);
      peer->setPrimary (primary);
      upbasic->setPeerNode (peer);
      upprimary->setPeerNode (peer);
      mac_->addPeerNode (peer);
      mac_->getCManager()->add_connection (upbasic, true);
      mac_->getCManager()->add_connection (basic, false);
      mac_->getCManager()->add_connection (upprimary, true);
      mac_->getCManager()->add_connection (primary, false);
      //schedule timer in case the node never register
      addtimer17 (req->ss_mac_address);

      //create packet for answers
      Packet *rep = mac_->getPacket ();
      struct hdr_cmn *ch = HDR_CMN(rep);
      rep->allocdata (sizeof (struct mac802_16_rng_rsp_frame));
      mac802_16_rng_rsp_frame *frame = (mac802_16_rng_rsp_frame*) rep->accessdata();
      frame->type = MAC_RNG_RSP;
      frame->uc_id = ulsubframe->getChannelID();
      frame->rng_status = RNG_SUCCESS;
      frame->ss_mac_address = req->ss_mac_address;
      frame->basic_cid = basic->get_cid();
      frame->primary_cid = primary->get_cid();
      ch->size() = RNG_RSP_SIZE;
      //compute transmission time
      Burst *b = map_->getDlSubframe()->getPdu ()->getBurst (0);
      double txtime = mac_->getPhy()->getTrxTime (ch->size(), map_->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
      ch->txtime() = txtime;
      //enqueue packet
      mac_->getCManager()->get_connection (BROADCAST_CID, false)->enqueue (rep);

      if (cl_head_==NULL) {
	cl_head_ = (new_client_t*)malloc (sizeof (new_client_t));
	cl_tail_ = cl_head_;
      } else {
	cl_tail_->next = (new_client_t*)malloc (sizeof (new_client_t));
	cl_tail_=cl_tail_->next;
      }
      cl_tail_->cid = primary->get_cid();
      cl_tail_->next = NULL;

#ifdef USE_802_21
      mac_->send_link_detected (mac_->addr(), peer->getPeerNode(), 1);
#endif

    } else {
      mac_->debug ("Received ranging for known station (%d)\n", req->ss_mac_address);
      //reset invited ranging retries for SS
      //create packet for answers
      Connection *basic = peer->getBasic();
      Connection *primary = peer->getPrimary();
      Packet *rep = mac_->getPacket ();
      struct hdr_cmn *ch = HDR_CMN(rep);
      rep->allocdata (sizeof (struct mac802_16_rng_rsp_frame));
      mac802_16_rng_rsp_frame *frame = (mac802_16_rng_rsp_frame*) rep->accessdata();
      frame->type = MAC_RNG_RSP;
      frame->uc_id = ulsubframe->getChannelID();
      frame->rng_status = RNG_SUCCESS;
      frame->ss_mac_address = req->ss_mac_address;
      frame->basic_cid = basic->get_cid();
      frame->primary_cid = primary->get_cid();
      ch->size() = RNG_RSP_SIZE;
      //compute transmission time
      Burst *b = map_->getDlSubframe()->getPdu ()->getBurst (0);
      double txtime = mac_->getPhy()->getTrxTime (ch->size(), map_->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
      ch->txtime() = txtime;
      //enqueue packet
      mac_->getCManager()->get_connection (BROADCAST_CID, false)->enqueue (rep);
    }
  }
}

/**
 * Add a new timer17 in the list. It also performs cleaning of the list
 * @param index The client address
 */
void BSScheduler::addtimer17 (int index)
{
  //clean expired timers
  T17Element *entry;
  for (entry = t17_head_.lh_first; entry ; ) {
    if (entry->paused ()) {
      T17Element *tmp = entry;
      entry = entry->next_entry();
      tmp->remove_entry();
      free (tmp);
    }
    entry = entry->next_entry();
  }

  entry = new T17Element (mac_, index);
  entry->insert_entry (&t17_head_);
}
/**
 * Cancel and remove the timer17 associated with the node
 * @param index The client address
 */
void BSScheduler::removetimer17 (int index)
{
  //clean expired timers
  T17Element *entry;
  for (entry = t17_head_.lh_first; entry ; entry = entry->next_entry()) {
    if (entry->index ()==index) {
      entry->cancel();
      entry->remove_entry();
      delete (entry);
      break;
    }
  }
}

/**
 * Process bandwidth request
 * @param p The request
 */
void BSScheduler::process_bw_req (Packet *p)
{ 
  hdr_mac802_16 *wimaxHdr = HDR_MAC802_16(p);
  gen_mac_header_t header = wimaxHdr->header;

  bw_req_header_t *req;
  req = (bw_req_header_t *)&header;

  mac_->debug ("received bandwidth request of %d bytes from %d\n", req->br, req->cid); 
  
}

/**
 * Process registration request
 * @param p The request
 */
void BSScheduler::process_reg_req (Packet *req)
{ 
  hdr_mac802_16 *wimaxHdr_req = HDR_MAC802_16(req);
  gen_mac_header_t header_req = wimaxHdr_req->header;
  
  mac_->debug ("received registration request from %d\n", header_req.cid);

  Packet *p;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr;
  mac802_16_reg_rsp_frame *reg_frame;
  PeerNode *peer;

  //create packet for request
  p = mac_->getPacket ();
  ch = HDR_CMN(p);
  wimaxHdr = HDR_MAC802_16(p);
  p->allocdata (sizeof (struct mac802_16_reg_rsp_frame));
  reg_frame = (mac802_16_reg_rsp_frame*) p->accessdata();
  reg_frame->type = MAC_REG_RSP;
  reg_frame->response = 0; //OK
  peer = mac_->getCManager()->get_connection (header_req.cid, false)->getPeerNode();
  Connection *secondary = peer->getSecondary ();
  if (secondary==NULL) {
    //first time 
    secondary = new Connection (CONN_SECONDARY);
    Connection *upsecondary = new Connection (CONN_SECONDARY, secondary->get_cid());
    mac_->getCManager()->add_connection (upsecondary, true);
    mac_->getCManager()->add_connection (secondary, false);
    peer->setSecondary (secondary);
    upsecondary->setPeerNode (peer);
  }
  reg_frame->sec_mngmt_cid = secondary->get_cid();
  wimaxHdr->header.cid = header_req.cid;
  ch->size() = REG_RSP_SIZE;
  
  //compute transmission time
  Burst *b = map_->getDlSubframe()->getPdu ()->getBurst (0);
  double txtime = mac_->getPhy()->getTrxTime (ch->size(), map_->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
  ch->txtime() = txtime;
  //enqueue packet
  mac_->getCManager()->get_connection (BROADCAST_CID, false)->enqueue (p);
  //clear t17 timer for this node
  removetimer17 (peer->getPeerNode());

#ifdef USE_802_21
  mac_->debug ("At %f in Mac %d, send link up\n", NOW, mac_->addr());
  mac_->send_link_up (peer->getPeerNode(),mac_->addr(), -1);
#endif
}

/**
 * Send a neighbor advertisement message
 */
void BSScheduler::send_nbr_adv ()
{
  mac_->debug ("At %f in BS %d send_nbr_adv (nb_neighbor=%d)\n", NOW, mac_->addr(), nbr_db_->getNbNeighbor());
  Packet *p;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr;
  mac802_16_mob_nbr_adv_frame *frame;
  //PeerNode *peer;

  //create packet for request
  p = mac_->getPacket ();
  ch = HDR_CMN(p);
  wimaxHdr = HDR_MAC802_16(p);
  p->allocdata (sizeof (struct mac802_16_mob_nbr_adv_frame));
  frame = (mac802_16_mob_nbr_adv_frame*) p->accessdata();
  frame->type = MAC_MOB_NBR_ADV;
  frame->n_neighbors = nbr_db_->getNbNeighbor();
  frame->skip_opt_field = 0;
  for (int i = 0 ; i < frame->n_neighbors ; i++) {
    frame->nbr_info[i].phy_profile_id.FAindex = 0;
    frame->nbr_info[i].phy_profile_id.bs_eirp = 0;
    frame->nbr_info[i].nbr_bsid= nbr_db_->getNeighbors()[i]->getID();
    frame->nbr_info[i].dcd_included = true;
    memcpy (&(frame->nbr_info[i].dcd_settings), nbr_db_->getNeighbors ()[i]->getDCD(), sizeof(mac802_16_dcd_frame));
    frame->nbr_info[i].ucd_included = true;
    memcpy (&(frame->nbr_info[i].ucd_settings), nbr_db_->getNeighbors ()[i]->getUCD(), sizeof(mac802_16_ucd_frame));
    frame->nbr_info[i].phy_included = false;
  }
  ch->size() = Mac802_16pkt::getMOB_NBR_ADV_size(frame);
  mac_->getCManager()->get_connection (BROADCAST_CID, false)->enqueue (p);
  
}

/** 
 * Finds out if the given station is currently scanning
 * @param nodeid The MS id
 */
bool BSScheduler::isPeerScanning (int nodeid)
{
  ScanningStation *sta;
  for (sta = scan_stations_.lh_first; sta ; sta = sta->next_entry()) {
    if (sta->getNodeId()==nodeid && sta->isScanning(mac_->frame_number_)) {
      //printf ("station %d scanning\n", nodeid);
      return true;
    }
  }
  return false;
}

/**
 * Process handover request
 * @param p The request
 */
void BSScheduler::process_msho_req (Packet *req)
{
  hdr_mac802_16 *wimaxHdr_req = HDR_MAC802_16(req);
  gen_mac_header_t header_req = wimaxHdr_req->header;
  mac802_16_mob_msho_req_frame *req_frame = 
    (mac802_16_mob_msho_req_frame*) req->accessdata();
  
  mac_->debug ("At %f in Mac %d received handover request from %d\n", NOW, mac_->addr(), header_req.cid);

  //check the BS that has stronger power
  int maxIndex = 0;
  int maxRssi = 0; //max value
  for (int i = 0; i < req_frame->n_new_bs_full ; i++) {
    if (req_frame->bs_full[i].bs_rssi_mean >= maxRssi) {
      maxIndex = i;
      maxRssi = req_frame->bs_full[i].bs_rssi_mean;
    }
  }
  //reply with one recommended BS
  Packet *p;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr;
  mac802_16_mob_bsho_rsp_frame *rsp_frame;

  send_nbr_adv (); //to force update with latest information

  //create packet for request
  p = mac_->getPacket ();
  ch = HDR_CMN(p);
  wimaxHdr = HDR_MAC802_16(p);
  p->allocdata (sizeof (struct mac802_16_mob_bsho_rsp_frame)+sizeof (mac802_16_mob_bsho_rsp_rec));
  rsp_frame = (mac802_16_mob_bsho_rsp_frame*) p->accessdata();
  rsp_frame->type = MAC_MOB_BSHO_RSP;
  
  rsp_frame->mode = 0; //HO request
  rsp_frame->ho_operation_mode = 1; //mandatory handover response
  rsp_frame->n_recommended = 1;
  rsp_frame->resource_retain_flag = 0; //release connection information
  rsp_frame->n_rec[0].neighbor_bsid = req_frame->bs_full[maxIndex].neighbor_bs_index;
  rsp_frame->n_rec[0].ho_process_optimization=0; //no optimization

  ch->size() += Mac802_16pkt::getMOB_BSHO_RSP_size(rsp_frame);
  wimaxHdr->header.cid = header_req.cid;
  mac_->getCManager()->get_connection (BROADCAST_CID, false)->enqueue (p);
}
 
/**
 * Process handover indication
 * @param p The indication
 */
void BSScheduler::process_ho_ind (Packet *p)
{
  hdr_mac802_16 *wimaxHdr_req = HDR_MAC802_16(p);
  gen_mac_header_t header_req = wimaxHdr_req->header;
  //mac802_16_mob_ho_ind_frame *req_frame = 
  //  (mac802_16_mob_ho_ind_frame*) p->accessdata();
  
  mac_->debug ("At %f in Mac %d received handover indication from %d\n", NOW, mac_->addr(), header_req.cid);
  

}
 
/**
 * Send a scan response to the MN
 * @param rsp The response from the control
 */
void BSScheduler::send_scan_response (mac802_16_mob_scn_rsp_frame *rsp, int cid)
{
  //create packet for request
  Packet *p = mac_->getPacket ();
  struct hdr_cmn *ch = HDR_CMN(p);
  hdr_mac802_16 *wimaxHdr = HDR_MAC802_16(p);
  p->allocdata (sizeof (struct mac802_16_mob_scn_rsp_frame));
  mac802_16_mob_scn_rsp_frame* rsp_frame = (mac802_16_mob_scn_rsp_frame*) p->accessdata();
  memcpy (rsp_frame, rsp, sizeof (mac802_16_mob_scn_rsp_frame));
  rsp_frame->type = MAC_MOB_SCN_RSP;
  
  wimaxHdr->header.cid = cid;
  ch->size() += Mac802_16pkt::getMOB_SCN_RSP_size(rsp_frame);
  
  //add scanning station to the list
  PeerNode *peer = mac_->getCManager()->get_connection (cid, false)->getPeerNode();

  /* The request is received in frame i, the reply is sent in frame i+1
   * so the frame at which the scanning start is start_frame+2
   */
  ScanningStation *sta = new ScanningStation (peer->getPeerNode(), rsp_frame->scan_duration & 0xFF, 
					     rsp_frame->start_frame+mac_->frame_number_+2, 
					     rsp_frame->interleaving_interval & 0xFF,
					     rsp_frame->scan_iteration & 0xFF);
  sta->insert_entry_head (&scan_stations_);

  //compute transmission time
  Burst *b = map_->getDlSubframe()->getPdu ()->getBurst (0);
  double txtime = mac_->getPhy()->getTrxTime (ch->size(), map_->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
  ch->txtime() = txtime;
  //enqueue packet
  mac_->getCManager()->get_connection (BROADCAST_CID, false)->enqueue (p);
}
