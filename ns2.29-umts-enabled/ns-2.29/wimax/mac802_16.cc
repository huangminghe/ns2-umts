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

#include "mac802_16.h"
#include "scheduling/wimaxscheduler.h"
#include "scheduling/ssscheduler.h" //TBR
//we use mac 802_11 for trace
#include "mac-802_11.h"

/* Defines frequencies for 3.5 GHz band and 7 Mhz freqency bandwidth */
/* Will be removed when a dynamic way is added */
static const int nbFreq = 5;
static const double frequencies[] = { 3.486e+9, 3.493e+9, 3.5e+9, 3.507e+9, 3.514e+9};


int hdr_mac802_16::offset_;
/**
 * TCL Hooks for the simulator for wimax packets
 */
static class Mac802_16HeaderClass : public PacketHeaderClass
{
public:
	Mac802_16HeaderClass() : PacketHeaderClass("PacketHeader/802_16",
					     sizeof(hdr_mac802_16))
	{
		bind_offset(&hdr_mac802_16::offset_);
	}
} class_hdr_mac802_16;

/**
 * TCL Hooks for the simulator for wimax mac
 */
static class Mac802_16Class : public TclClass {
public:
  Mac802_16Class() : TclClass("Mac/802_16") {}
  TclObject* create(int, const char*const*) {
    return (new Mac802_16());
    
  }
} class_mac802_16;

Phy802_16MIB::Phy802_16MIB(Mac802_16 *parent)
{
  parent->bind ("channel_", &channel );
  parent->bind ("fbandwidth_", &fbandwidth );
  parent->bind ("ttg_", &ttg );
  parent->bind ("rtg_", &rtg );
}

Mac802_16MIB::Mac802_16MIB(Mac802_16 *parent)
{
  parent->bind ("queue_length_", &queue_length );
  parent->bind ("frame_duration_", &frame_duration );
  parent->bind ("dcd_interval_", &dcd_interval );
  parent->bind ("ucd_interval_", &ucd_interval );
  parent->bind ("init_rng_interval_", &init_rng_interval );
  parent->bind ("lost_dlmap_interval_", &lost_dlmap_interval );
  parent->bind ("lost_ulmap_interval_", &lost_ulmap_interval );
  parent->bind ("t1_timeout_", &t1_timeout );
  parent->bind ("t2_timeout_", &t2_timeout );
  parent->bind ("t3_timeout_", &t3_timeout );
  parent->bind ("t6_timeout_", &t6_timeout );
  parent->bind ("t12_timeout_", &t12_timeout );
  parent->bind ("t16_timeout_", &t16_timeout );
  parent->bind ("t17_timeout_", &t17_timeout );
  parent->bind ("t21_timeout_", &t21_timeout );
  parent->bind ("contention_rng_retry_", &contention_rng_retry );
  parent->bind ("invited_rng_retry_", &invited_rng_retry );
  parent->bind ("request_retry_", &request_retry );
  parent->bind ("reg_req_retry_", &reg_req_retry );
  parent->bind ("tproc_", &tproc );
  parent->bind ("dsx_req_retry_", &dsx_req_retry );
  parent->bind ("dsx_rsp_retry_", &dsx_rsp_retry );

  parent->bind ("rng_backoff_start_", &rng_backoff_start);
  parent->bind ("rng_backoff_stop_", &rng_backoff_stop);
  parent->bind ("bw_backoff_start_", &bw_backoff_start);
  parent->bind ("bw_backoff_stop_", &bw_backoff_stop);
 
  //mobility extension
  parent->bind ("scan_duration_", &scan_duration );
  parent->bind ("interleaving_interval_", &interleaving );
  parent->bind ("scan_iteration_", &scan_iteration );
  parent->bind ("t44_timeout_", &t44_timeout );
  parent->bind ("max_dir_scan_time_", &max_dir_scan_time );
  parent->bind ("nbr_adv_interval_", &nbr_adv_interval );
  parent->bind ("scan_req_retry_", &scan_req_retry );

  parent->bind ("client_timeout_", &client_timeout );
  parent->bind ("rxp_avg_alpha_", &rxp_avg_alpha);
  parent->bind ("lgd_factor_", &lgd_factor_);
}

/**
 * Creates a Mac 802.16
 */
Mac802_16::Mac802_16() : Mac (), macmib_(this), phymib_(this), rxTimer_(this)
{
  //init variable
  LIST_INIT(&classifier_list_);
  peer_list_ = (struct peerNode *) malloc (sizeof(struct peerNode));
  LIST_INIT(peer_list_);
  
  collision_ = false;
  pktRx_ = NULL;
  pktBuf_ = NULL;

  connectionManager_ = new ConnectionManager (this);
  scheduler_ = NULL;
  /* the following will be replaced by dynamic adding of service flow */
  serviceFlowHandler_ = new ServiceFlowHandler ();
  serviceFlowHandler_->setMac (this);
  bs_id_ = BS_NOT_CONNECTED;
  type_ = STA_UNKNOWN;
  frame_number_ = 0;
  state_ = MAC802_16_DISCONNECTED;
  notify_upper_ = true;

  last_tx_time_ = 0;
  last_tx_duration_ = 0;

  Tcl& tcl = Tcl::instance();
  tcl.evalf ("Phy/WirelessPhy set RXThresh_");
  macmib_.RXThreshold_ = atof (tcl.result());
  
#ifdef USE_802_21
  linkType_ = LINK_802_16;
  eventList_ = 0x1BF;
  commandList_ = 0xF;
#endif  

  /* Initialize Stats variables */
  bind_bool ("print_stats_", &print_stats_);
  last_tx_delay_ = 0;
  double tmp;
  bind ("delay_avg_alpha_", &tmp);
  delay_watch_.set_alpha(tmp);
  bind ("jitter_avg_alpha_", &tmp);
  jitter_watch_.set_alpha(tmp);
  bind ("loss_avg_alpha_", &tmp);
  loss_watch_.set_alpha(tmp);
  bind ("throughput_avg_alpha_", &tmp);
  rx_data_watch_.set_alpha(tmp);
  rx_data_watch_.set_pos_gradient (false);
  rx_traffic_watch_.set_alpha(tmp);
  rx_traffic_watch_.set_pos_gradient (false);
  tx_data_watch_.set_alpha(tmp);
  tx_data_watch_.set_pos_gradient (false);
  tx_traffic_watch_.set_alpha(tmp);
  tx_traffic_watch_.set_pos_gradient (false);
  bind ("throughput_delay_", &tmp);
  rx_data_watch_.set_delay (tmp);
  rx_traffic_watch_.set_delay (tmp);
  tx_data_watch_.set_delay (tmp);
  tx_traffic_watch_.set_delay (tmp);
  //timers for stats
  rx_data_timer_ = new StatTimer (this, &rx_data_watch_);
  rx_traffic_timer_ = new StatTimer (this, &rx_traffic_watch_);
  tx_data_timer_ = new StatTimer (this, &tx_data_watch_);
  tx_traffic_timer_ = new StatTimer (this, &tx_traffic_watch_);

}

/*
 * Interface with the TCL script
 * @param argc The number of parameter
 * @param argv The list of parameters
 */
int Mac802_16::command(int argc, const char*const* argv)
{
  if (argc == 2) {
    if (strcmp(argv[1], "dump-classifiers") == 0) {
      for (SDUClassifier *n=classifier_list_.lh_first;n;n=n->next_entry()) {
	//printf ("Classifier %x priority=%d\n", (int)n, n->getPriority());
      }
      return TCL_OK;
    }
  }
  else if (argc == 3) {
    /*
    if (strcmp(argv[1], "set-bs") == 0) {
      bs_id_ = atoi (argv[2]);
    }
    else*/
    if (strcmp(argv[1], "add-classifier") == 0) {
      SDUClassifier *clas = (SDUClassifier*) TclObject::lookup(argv[2]);
      if (clas == 0)
	return TCL_ERROR;
      //add classifier to the list
      addClassifier (clas);
      return TCL_OK;
    } else if (strcmp(argv[1], "set-scheduler") == 0) {
      scheduler_ = (WimaxScheduler*) TclObject::lookup(argv[2]);
      if (scheduler_ == 0)
	return TCL_ERROR;
      scheduler_->setMac (this); //register the mac
      setStationType (scheduler_->getNodeType()); //get the node type (BS or MN)
      return TCL_OK;
    } else if (strcmp(argv[1], "set-servicehandler") == 0) {
      serviceFlowHandler_ = (ServiceFlowHandler*) TclObject::lookup(argv[2]);
      if (serviceFlowHandler_ == 0)
	return TCL_ERROR;
      serviceFlowHandler_->setMac (this);
      return TCL_OK;
    } else if (strcmp(argv[1], "set-channel") == 0) {
      assert (netif_); //to make sure we can update the phy
      phymib_.channel = atoi (argv[2]);
      double tmp = frequencies[phymib_.channel];
      getPhy ()->setFrequency (tmp);
      return TCL_OK;
    } else if (strcmp(argv[1], "log-target") == 0) { 
      logtarget_ = (NsObject*) TclObject::lookup(argv[2]);
      if(logtarget_ == 0)
	return TCL_ERROR;
      return TCL_OK;
    }
  }
  return Mac::command(argc, argv);
}

/**
 * Set the type of STA. This will be called when adding the scheduler
 * It is used to create the default connections
 * @param type The station type
 */
void Mac802_16::setStationType (station_type_t type)
{
  assert (type_ == STA_UNKNOWN && type != STA_UNKNOWN);
  type_ = type;

  init_default_connections ();
}

/**
 * Initialize default connections
 */
void Mac802_16::init_default_connections ()
{
  Connection * con;

  //create initial ranging and padding connection
  con = new Connection (CONN_INIT_RANGING);
  connectionManager_->add_connection (con, true); //uplink
  con = new Connection (CONN_INIT_RANGING);
  connectionManager_->add_connection (con, false); //downlink
  con = new Connection (CONN_PADDING);
  connectionManager_->add_connection (con, true);
  con = new Connection (CONN_PADDING);
  connectionManager_->add_connection (con, false);

  if (type_ == STA_BS) {
    //the BS is always connected
    setMacState (MAC802_16_CONNECTED);
    //we need to create a Broadcast connection and AAS init ranging CIDs
    con = new Connection (CONN_BROADCAST);
    connectionManager_->add_connection (con, false);
    con = new Connection (CONN_AAS_INIT_RANGING);
    connectionManager_->add_connection (con, true);
  } else if (type_ == STA_MN) {
    //create connection to receive broadcast packets from BS
    con = new Connection (CONN_BROADCAST);
    connectionManager_->add_connection (con, false);
  }
}

/**
 * Return the peer node that has the given address
 * @param index The address of the peer
 * @return The peer node that has the given address
 */
PeerNode* Mac802_16::getPeerNode (int index)
{
  for (PeerNode *p=peer_list_->lh_first;p;p=p->next_entry()) {
    if (p->getPeerNode ()==index)
      return p;
  }
  return NULL;
}

/**
 * Add the peer node
 * @param The peer node to add
 */
void Mac802_16::addPeerNode (PeerNode *node)
{
  node->insert_entry (peer_list_);
  //update Rx time so for default value
  node->setRxTime(NOW);
  node->getStatWatch()->set_alpha(macmib_.rxp_avg_alpha);
}

/**
 * Remove the peer node
 * @param The peer node to remove
 */
void Mac802_16::removePeerNode (PeerNode *peer)
{
  if (peer->getBasic()) {
    getCManager()->remove_connection (peer->getBasic()->get_cid());
    delete (peer->getBasic());
  }
  if (peer->getPrimary()) {
    getCManager()->remove_connection (peer->getPrimary()->get_cid());
    delete (peer->getPrimary());
  }
  if (peer->getSecondary()) {
    getCManager()->remove_connection (peer->getSecondary()->get_cid());
    delete (peer->getSecondary());
  }
  if (peer->getInData()) {
    getCManager()->remove_connection (peer->getInData()->get_cid());
    delete (peer->getInData());
  }
  if (peer->getOutData()) {
    getCManager()->remove_connection (peer->getOutData()->get_cid());
    delete (peer->getOutData());
  }
  peer->remove_entry ();
  delete (peer);
}

/**
 * Set the mac state
 * @param state The new mac state
 */  
void Mac802_16::setMacState (Mac802_16State state)
{
  // debug("In Mac %d, setting Mac State from state %d to state %d\n", index_, state_, state);
  state_ = state;
}

/**
 * Return the mac state
 * @return The new mac state
 */  
Mac802_16State Mac802_16::getMacState ()
{
  return state_;
}

/**
 * Return the PHY layer
 * @return The PHY layer
 */
OFDMPhy* Mac802_16::getPhy () { 
  return (OFDMPhy*) netif_;
}

/**
 * Change the channel
 * @param channel The new channel
 */
void Mac802_16::setChannel (int channel)
{
  assert (channel < nbFreq);
  phymib_.channel = channel;
  double tmp = frequencies[phymib_.channel];
  getPhy ()->setFrequency (tmp);
}

/**
 * Return the channel number for the given frequency
 * @param freq The frequency
 * @return The channel number of -1 if the frequency does not match
 */
int Mac802_16::getChannel (double freq)
{
  for (int i = 0 ; i < nbFreq ; i++) {
    if (frequencies[i]==freq)
      return i;
  }
  return -1;
}

/**
 * Return the channel index
 * @return The channel
 */
int Mac802_16::getChannel ()
{
  return phymib_.channel;
}

/**
 * Set the channel to the next from the list
 * Used at initialisation and when loosing signal
 */
void Mac802_16::nextChannel ()
{
  debug ("At %f in Mac %d Going to channel %d\n", NOW, index_, (phymib_.channel+1)%nbFreq);
  setChannel ((phymib_.channel+1)%nbFreq);
}

/**
 * Add a flow
 * @param qos The QoS required
 * @param handler The entity that requires to add a flow
 */
void Mac802_16::addFlow (ServiceFlowQoS * qos, void * handler) 
{

}

/**
 * Backup the state of the Mac
 */
state_info* Mac802_16::backup_state ()
{
  state_info *backup_state = (state_info*) malloc (sizeof (state_info));
  backup_state->bs_id = bs_id_;
  backup_state->state = state_;
  backup_state->frameduration = getFrameDuration();
  backup_state->frame_number = frame_number_;
  backup_state->channel = getChannel();
  backup_state->connectionManager = connectionManager_;
  connectionManager_ = new ConnectionManager (this);
  init_default_connections ();
  backup_state->serviceFlowHandler = serviceFlowHandler_;
  serviceFlowHandler_ = new ServiceFlowHandler();
  backup_state->peer_list = peer_list_;
  peer_list_ = (struct peerNode *) malloc (sizeof(struct peerNode));
  LIST_INIT(peer_list_);
  return backup_state;
}

/**
 * Restore the state of the Mac
 */
void Mac802_16::restore_state (state_info *backup_state)
{
  bs_id_ = backup_state->bs_id;
  state_ = backup_state->state;
  setFrameDuration(backup_state->frameduration);
  frame_number_ = backup_state->frame_number;
  setChannel (backup_state->channel);
  delete (connectionManager_);
  connectionManager_ = backup_state->connectionManager;
  delete (serviceFlowHandler_);
  serviceFlowHandler_ = backup_state->serviceFlowHandler;
  while (getPeerNode_head()!=NULL) {
    removePeerNode (getPeerNode_head());
  }
  peer_list_ = backup_state->peer_list;
}
  
/**
 * Set the variable used to find out if upper layers
 * must be notified to send packets. During scanning we
 * do not want upper layers to send packet to the mac.
 */
void Mac802_16::setNotify_upper (bool notify) { 
  notify_upper_ = notify; 
  if (notify_upper_ && pktBuf_) {
    sendDown (pktBuf_);
    pktBuf_ = NULL;
  }
}

/**** Packet processing methods ****/

/*
 * Process packets going out
 * @param p The packet to send out
 */
void Mac802_16::sendDown(Packet *p)
{
  //We first send it through the CS
  int cid = -1;

  if (!notify_upper_) {
    assert (!pktBuf_);
    pktBuf_ = p;
    return;
  } 

  for (SDUClassifier *n=classifier_list_.lh_first; n && cid==-1; n=n->next_entry()) {
    cid = n->classify (p);
  }

  if (cid == -1) {
    debug ("At %f in Mac %d drop packet because no classification were found\n", \
	    NOW, index_);
    drop(p, "CID");
    //Packet::free (p);
  } else {
    //enqueue the packet 
    Connection *connection = connectionManager_->get_connection (cid, type_ == STA_MN);
    if (connection == NULL) {
      debug ("Warning: At %f in Mac %d connection with cid = %d does not exist. Please check classifiers\n",\
	      NOW, index_, cid);
      //Packet::free (p);
      update_watch (&loss_watch_, 1);
      drop(p, "CID");
    }
    else {

      if (connection->queueLength ()==macmib_.queue_length) {
	//queue full 
	update_watch (&loss_watch_, 1);
	drop (p, "QWI");
      } else {
	//update mac header information
	//set header information
	hdr_mac802_16 *wimaxHdr = HDR_MAC802_16(p);
	wimaxHdr->header.ht = 0;
	wimaxHdr->header.ec = 1;
	wimaxHdr->header.type = 0; //no subheader
	wimaxHdr->header.ci = 0;
	wimaxHdr->header.eks = 0;
	wimaxHdr->header.cid = cid; //default
	wimaxHdr->header.hcs = 0;
	HDR_CMN(p)->size() += HDR_MAC802_16_SIZE;
	//moved stamping into enqueue function of Connection class
	//HDR_CMN(p)->timestamp() = NOW; //set timestamp for delay
	connection ->enqueue (p);
	//printf ("At %f in Mac %d Enqueue packet to cid=%d queue size=%d(max=%d)\n", NOW, index_, cid,connection->queueLength (), macmib_.queue_length);
      }
    }
  }

  //inform upper layer that it can send another packet
  //if (notify_upper_) 
  resume (NULL);  
}

/*
 * Transmit a packet to the physical layer
 * @param p The packet to send out
 */
void Mac802_16::transmit(Packet *p)
{
  if (NOW < last_tx_time_+last_tx_duration_) {
    //still sending
    //printf ("MAC is already transmitting. Drop packet.\n");
    Packet::free (p);
    return;
  }

  struct hdr_cmn *ch = HDR_CMN(p);
  /*
  debug ("At %f in Mac %d sending packet (type=%s, size=%d) ", NOW, index_, packet_info.name(ch->ptype()), ch->size());
  if (ch->ptype()==PT_MAC) {
    if (HDR_MAC802_16(p)->header.ht == 0)
      debug ("mngt=%d\n", ((mac802_16_dl_map_frame*) p->accessdata())->type);
    else
      debug ("bwreq\n");
  } else {
    debug ("\n");
  }
  */
  //update stats for delay and jitter
  double delay = NOW-ch->timestamp();
  update_watch (&delay_watch_, delay);
  double jitter = fabs (delay - last_tx_delay_);
  update_watch (&jitter_watch_, jitter);
  last_tx_delay_ = delay;
  if (ch->ptype()!=PT_MAC) {
    update_throughput (&tx_data_watch_, 8*ch->size());
  } 
  update_throughput (&tx_traffic_watch_, 8*ch->size());
  
  last_tx_time_ = NOW;
  last_tx_duration_ = ch->txtime();
  downtarget_->recv (p, (Handler*)NULL);
}

/*
 * Process incoming packets
 * @param p The incoming packet
 */
void Mac802_16::sendUp (Packet *p)
{
  struct hdr_cmn *ch = HDR_CMN(p);

#ifdef DEBUG_WIMAX
  debug ("At %f in Mac %d receive first bit..over at %f(txtime=%f) (type=%s) ", NOW, index_, NOW+ch->txtime(),ch->txtime(), packet_info.name(ch->ptype()));
  if (ch->ptype()==PT_MAC) {
    if (HDR_MAC802_16(p)->header.ht == 0)
      debug ("mngt=%d\n", ((mac802_16_dl_map_frame*) p->accessdata())->type);
    else
      debug ("bwreq\n");
  } else {
    debug ("\n");
  }
#endif

  if (pktRx_ !=NULL) {
    /*
     *  If the power of the incoming packet is smaller than the
     *  power of the packet currently being received by at least
     *  the capture threshold, then we ignore the new packet.
     */
    if(pktRx_->txinfo_.RxPr / p->txinfo_.RxPr >= p->txinfo_.CPThresh) {
      Packet::free(p);
    } else {
      /*
       *  Since a collision has occurred, figure out
       *  which packet that caused the collision will
       *  "last" the longest.  Make this packet,
       *  pktRx_ and reset the Recv Timer if necessary.
       */
      if(txtime(p) > rxTimer_.expire()) {
	rxTimer_.stop();
	//printf ("\t drop pktRx..collision\n");
	drop(pktRx_, "COL");
	update_watch (&loss_watch_, 1);
	pktRx_ = p;
	//mark the packet with error
	ch->error() = 1;
	collision_ = true;

	rxTimer_.start(ch->txtime()-0.000000001);
      }
      else {
	//printf ("\t drop new packet..collision\n");
	drop(p, "COL");
	//mark the packet with error
	HDR_CMN(pktRx_)->error() = 1;
	collision_ = true;
      }
    }
    return;
  }
  assert (pktRx_==NULL);
  assert (rxTimer_.busy()==0);
  pktRx_ = p;
  //create a timer to wait for the end of reception
  //since the packets are received by burst, the beginning of the new packet 
  //is the same time as the end of this packet..we process this packet 1ns 
  //earlier to make room for the new packet.
  rxTimer_.start(ch->txtime()-0.000000001);
}

/**
 * Process the fully received packet
 */
void Mac802_16::receive ()
{
  assert (pktRx_);
  struct hdr_cmn *ch = HDR_CMN(pktRx_);

#ifdef DEBUG_WIMAX
  printf ("At %f in Mac %d packet received (type=%s) ", NOW, index_, packet_info.name(ch->ptype()));
  if (ch->ptype()==PT_MAC) {
    if (HDR_MAC802_16(pktRx_)->header.ht == 0)
      printf ("mngt=%d\n", ((mac802_16_dl_map_frame*) pktRx_->accessdata())->type);
    else
      printf ("bwreq\n");
  } else {
    printf ("\n");
  }
#endif
    

  //drop the packet if corrupted
  if (ch->error()) {
    if (collision_) {
      //printf ("\t drop new pktRx..collision\n");
      drop (pktRx_, "COL");
      collision_ = false;
    } else {
      //error in the packet, the Mac does not process
      Packet::free(pktRx_);
    }
    //update drop stat
    update_watch (&loss_watch_, 1);
    pktRx_ = NULL;
    return;
  }

  //process packet
  hdr_mac802_16 *wimaxHdr = HDR_MAC802_16(pktRx_);
  gen_mac_header_t header = wimaxHdr->header;
  int cid = header.cid;
  Connection *con = connectionManager_->get_connection (cid, type_==STA_BS);
  mac802_16_dl_map_frame *frame;

  if (con == NULL) {
    //This packet is not for us
    //printf ("At %f in Mac %d Connection null\n", NOW, index_);
    update_watch (&loss_watch_, 1);
    Packet::free(pktRx_);
    pktRx_=NULL;
    return;
  }
  //printf ("CID=%d\n", cid);

  //update rx time of last packet received
  PeerNode *peer;
  if (type_ == STA_MN)
    peer = getPeerNode_head(); //MN only has one peer
  else
    peer = con->getPeerNode(); //BS can have multiple peers

  if (peer) {
    peer->setRxTime (NOW);
    
    //collect receive signal strength stats
    peer->getStatWatch()->update(10*log10(pktRx_->txinfo_.RxPr*1e3));
    //debug ("At %f in Mac %d weighted RXThresh: %e rxp average %e\n", NOW, index_, macmib_.lgd_factor_*macmib_.RXThreshold_, pow(10,peer->getStatWatch()->average()/10)/1e3);
    double avg_w = pow(10,(peer->getStatWatch()->average()/10))/1e3;
    
    if ( avg_w < (macmib_.lgd_factor_*macmib_.RXThreshold_)) {
      //Removed the condition on going down to allow sending multiple events with different confidence level
      //if (!peer->isGoingDown () && type_ == STA_MN && state_==MAC802_16_CONNECTED) {
      if (type_ == STA_MN && state_==MAC802_16_CONNECTED) {
#ifdef USE_802_21
	if(mih_){
	  double probability = ((macmib_.lgd_factor_*macmib_.RXThreshold_)-avg_w)/((macmib_.lgd_factor_*macmib_.RXThreshold_)-macmib_.RXThreshold_);
	  Mac::send_link_going_down (addr(), getPeerNode_head()->getPeerNode(), -1, (int)(100*probability), LGD_RC_LINK_PARAM_DEGRADING, eventId_++);
	}else{
#endif
	  if (!peer->isGoingDown ()) //when we don't use 802.21, we only want to send the scan request once
	    ((SSscheduler*) scheduler_)->send_scan_request ();
#ifdef USE_802_21
	}
#endif
	peer->setGoingDown (true);
      }
      if(type_ == STA_BS){
#ifdef USE_802_21
	double probability = ((macmib_.lgd_factor_*macmib_.RXThreshold_)-avg_w)/((macmib_.lgd_factor_*macmib_.RXThreshold_)-macmib_.RXThreshold_);
	Mac::send_link_going_down (peer->getPeerNode(), addr(), -1, (int)(100*probability), LGD_RC_LINK_PARAM_DEGRADING, eventId_++);
#endif
	if (peer->getPrimary()!=NULL) { //check if node registered
	  peer->setGoingDown (true);
	}
      }
    }
    else {
      if (peer->isGoingDown()) {
#ifdef USE_802_21
	Mac::send_link_rollback (addr(), getPeerNode_head()->getPeerNode(), eventId_-1);
#endif
	peer->setGoingDown (false);
      }
    }
  }
  
  //process reassembly
  if (wimaxHdr->frag_subheader) {
    bool drop_pkt = true;
    bool frag_error = false;
    //printf ("Frag type = %d\n",wimaxHdr->fc & 0x3);
    switch (wimaxHdr->fc & 0x3) {
    case FRAG_NOFRAG: 
      if (con->getFragmentationStatus()!=FRAG_NOFRAG)
	con->updateFragmentation (FRAG_NOFRAG, 0, 0); //reset
      drop_pkt = false;
      break; 
    case FRAG_FIRST: 
      //when it is the first fragment, it does not matter if we previously
      //received other fragments, since we reset the information
      assert (wimaxHdr->fsn == 0);
      //printf ("\tReceived first fragment\n");
      con->updateFragmentation (FRAG_FIRST, 0, ch->size()-(HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE));
      break; 
    case FRAG_CONT: 
      if ( (con->getFragmentationStatus()!=FRAG_FIRST
	    && con->getFragmentationStatus()!=FRAG_CONT)
	   || ((wimaxHdr->fsn&0x7) != (con->getFragmentNumber ()+1)%8) ) {
	frag_error = true;
	con->updateFragmentation (FRAG_NOFRAG, 0, 0); //reset
      } else {
	//printf ("\tReceived cont fragment\n");
	con->updateFragmentation (FRAG_CONT, wimaxHdr->fsn&0x7, con->getFragmentBytes()+ch->size()-(HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE));	
      }
      break; 
    case FRAG_LAST: 
      if ( (con->getFragmentationStatus()==FRAG_FIRST
	    || con->getFragmentationStatus()==FRAG_CONT)
	   && ((wimaxHdr->fsn&0x7) == (con->getFragmentNumber ()+1)%8) ) {
	//printf ("\tReceived last fragment\n");
	ch->size() += con->getFragmentBytes()-HDR_MAC802_16_FRAGSUB_SIZE;
	drop_pkt = false;
      } else {
	//printf ("Error with last frag seq=%d (expected=%d)\n", wimaxHdr->fsn&0x7, (con->getFragmentNumber ()+1)%8);
	frag_error = true;
      }     
      con->updateFragmentation (FRAG_NOFRAG, 0, 0); //reset
      break; 
    default:
      fprintf (stderr,"Error, unknown fragmentation type\n");
      exit (-1);
    }
    //if we got an error, or it is a fragment that is not the last, free the packet
    if (drop_pkt) {
      if (frag_error) {
	//update drop stat
	update_watch (&loss_watch_, 1);
	drop (pktRx_, "FRG"); //fragmentation error
      } else {
	//silently discard this fragment.
	Packet::free(pktRx_);
      }
      pktRx_=NULL;
      return;
    } 
  }

  //check packet type. If it is a bandwidth request, it could be for any CID
  //therefore we need to catch it before checking the connection type.
  if (header.ht == 1) {
    //This is a bandwidth request..give it to scheduler
    scheduler_->process (pktRx_);
  } else {
    //Generic MAC header
    switch (con->getType()) {
    case CONN_INIT_RANGING:
      scheduler_->process (pktRx_);
      break;
    case CONN_AAS_INIT_RANGING:
      break;
    case CONN_MULTICAST_POLLING: 
      break;
    case CONN_PADDING:
      //padding is sent by a SS that does not have data
      //to send is required to send a signal.
      //TBD: Find the SS that sent the padding
      scheduler_->process (pktRx_);
      break; 
    case CONN_BROADCAST:
      if (HDR_CMN(pktRx_)->ptype()==PT_MAC)
	scheduler_->process (pktRx_);
      else {
	//Richard: only send to upper layer if connected
	if (state_ == MAC802_16_CONNECTED)
	  goto send_upper;
	else {
	  //update drop stat, could be used to detect disconnect
	  update_watch (&loss_watch_, 1);
	  Packet::free(pktRx_);
	  pktRx_=NULL;
	  return;
	}
      }
      break; 
    case CONN_BASIC:
      scheduler_->process (pktRx_);
      break; 
    case CONN_PRIMARY:
      assert (HDR_CMN(pktRx_)->ptype()==PT_MAC);
      //we cast to this frame because all management frame start with a type
      if (wimaxHdr->header.ht==1) {
	//bw request
	scheduler_->process (pktRx_);
      } else {
	frame = (mac802_16_dl_map_frame*) pktRx_->accessdata();
	switch (frame->type) {
	case MAC_DSA_REQ: 
	case MAC_DSA_RSP: 
	case MAC_DSA_ACK: 
	  serviceFlowHandler_->process (pktRx_);
	  break;
	default:
	  scheduler_->process (pktRx_);
	}
      }
      break;   
    case CONN_SECONDARY:
      //fall through
    case CONN_DATA:
      //send to upper layer
      goto send_upper;
      break; 
    default:
      fprintf (stderr,"Error: unknown connection type\n");
      exit (0);
    }
  }
  goto sent_mac; //default jump

 send_upper:
  update_throughput (&rx_data_watch_, 8*ch->size());    
  update_throughput (&rx_traffic_watch_, 8*ch->size());
  ch->size() -= HDR_MAC802_16_SIZE;
  uptarget_->recv(pktRx_, (Handler*) 0);
  goto done;

 sent_mac:
  update_throughput (&rx_traffic_watch_, 8*ch->size());

  mac_log(pktRx_);
  Packet::free(pktRx_);

 done:
  update_watch (&loss_watch_, 0);
  pktRx_=NULL;
}

/**** Helper methods ****/

/**
 * Return the frame number
 * @return the frame number
 */
int Mac802_16::getFrameNumber () {
  return frame_number_;
}

/*
 * Return the code for the frame duration
 * @return the code for the frame duration
 */
int Mac802_16::getFrameDurationCode () {
  if (macmib_.frame_duration == 0.0025)
    return 0;
  else if (macmib_.frame_duration == 0.004)
    return 1;
  else if (macmib_.frame_duration == 0.005)
    return 2;
  else if (macmib_.frame_duration == 0.008)
    return 3;
  else if (macmib_.frame_duration == 0.01)
    return 4;
  else if (macmib_.frame_duration == 0.0125)
    return 5;
  else if (macmib_.frame_duration == 0.02)
    return 6;
  else {
    fprintf (stderr, "Invalid frame duration %f\n", macmib_.frame_duration);
    exit (1);
  }
}

/*
 * Set the frame duration using code
 * @param code The frame duration code
 */
void Mac802_16::setFrameDurationCode (int code) 
{
  switch (code) {
  case 0:
    macmib_.frame_duration = 0.0025;
    break;
  case 1:
    macmib_.frame_duration = 0.004;
    break;
  case 2:
    macmib_.frame_duration = 0.005;
    break;
  case 3:
    macmib_.frame_duration = 0.008;
    break;
  case 4:
    macmib_.frame_duration = 0.01;
    break;
  case 5:
    macmib_.frame_duration = 0.0125;
    break;
  case 6:
    macmib_.frame_duration = 0.02;
    break;
  default:
  fprintf (stderr, "Invalid frame duration code %d\n", code);
    exit (1);
  }
}


/**
 * Return a packet 
 * @return a new packet
 */
Packet *Mac802_16::getPacket ()
{
  Packet *p = Packet::alloc ();
  
  hdr_mac802_16 *wimaxHdr= HDR_MAC802_16(p);

  //set header information
  wimaxHdr->header.ht = 0;
  wimaxHdr->header.ec = 1;
  wimaxHdr->header.type = 0; //no subheader
  wimaxHdr->header.ci = 0;
  wimaxHdr->header.eks = 0;
  wimaxHdr->header.cid = BROADCAST_CID; //default
  wimaxHdr->header.hcs = 0;
  HDR_CMN(p)->ptype() = PT_MAC;

  HDR_CMN(p)->size() = HDR_MAC802_16_SIZE;

  return p;
}

/**** Internal methods ****/


/*
 * Add a classifier
 * @param clas The classifier to add
 */
void Mac802_16::addClassifier (SDUClassifier *clas) 
{
  SDUClassifier *n=classifier_list_.lh_first;
  SDUClassifier *prev=NULL;
  int i = 0;
  if (!n || (n->getPriority () >= clas->getPriority ())) {
    //the first element
    //debug ("Add first classifier\n");
    clas->insert_entry_head (&classifier_list_);
  } else {
    while ( n && (n->getPriority () < clas->getPriority ()) ) {
      prev=n;
      n=n->next_entry();
      i++;
    }
    //debug ("insert entry at position %d\n", i);
    clas->insert_entry (prev);
  }
  //Register this mac with the classifier
  clas->setMac (this);
}

#ifdef USE_802_21

/* 
 * Configure/Request configuration
 * The upper layer sends a config object with the required 
 * new values for the parameters (or PARAMETER_UNKNOWN_VALUE).
 * The MAC tries to set the values and return the new setting.
 * For examples if a MAC does not support a parameter it will
 * return  PARAMETER_UNKNOWN_VALUE
 * @param config The configuration object
 */ 
void Mac802_16::link_configure (link_parameter_config_t* config)
{
	assert (config);
	config->bandwidth = 15000000; //TBD use phy (but depend on modulation)
	config->type = LINK_802_16;
	//we set the rest to PARAMETER_UNKNOWN_VALUE
	config->ber = PARAMETER_UNKNOWN_VALUE;
	config->delay = PARAMETER_UNKNOWN_VALUE;
	config->macPoA = PARAMETER_UNKNOWN_VALUE;
}

/*
 * Disconnect from the PoA
 */
void Mac802_16::link_disconnect ()
{
  if (type_ == STA_MN) {
    //force losing synchronization
    ((SSscheduler*) scheduler_)->lost_synch ();
    set_mode (POWER_DOWN); //not sure if we should turn it off
  }
}

/*
 * Connect to the PoA
 */
void Mac802_16::link_connect (int poa)
{
  if (type_ == STA_MN) {
    set_mode (NORMAL_MODE);
    ((SSscheduler*) scheduler_)->link_connect(poa);  
  }
}

/*
 * Set the operation mode
 * @param mode The new operation mode
 * @return true if transaction succeded
 */
bool Mac802_16::set_mode (mih_operation_mode_t mode)
{
  switch (mode) {
  case NORMAL_MODE:
    if (op_mode_ != NORMAL_MODE) {
      getPhy()->node_on(); //turn on phy
      debug ("Turning on mac\n");
    }
    op_mode_ = mode;
    return true;
    break;
  case POWER_SAVING:
    //not yet supported 
    return false;
    break;
  case POWER_DOWN:
    if (op_mode_ != POWER_DOWN) {
      getPhy()->node_off(); //turn off phy
      debug ("Turning off mac\n");
    }
    op_mode_ = mode;
    return true;
    break;
  default:
    return false;
  }
}



/*
 * Scan chanels
 */
void Mac802_16::link_scan (void *req)
{
  if (type_ == STA_MN) {
    if(! ((SSscheduler*) scheduler_)->isScanRunning()){
      ((SSscheduler*) scheduler_)->setScanFlag();  
      ((SSscheduler*) scheduler_)->send_scan_request ();
    }
  }
}


/* 
 * Configure the threshold values for the given parameters
 * @param numLinkParameter number of parameter configured
 * @param linkThresholds list of parameters and thresholds
 */
struct link_param_th_status * Mac802_16::link_configure_thresholds (int numLinkParameter, struct link_param_th *linkThresholds)
{
  struct link_param_th_status *result = (struct link_param_th_status *) malloc(numLinkParameter * sizeof (struct link_param_th_status));
  StatWatch *watch=NULL;
  for (int i=0 ; i < numLinkParameter ; i++) {
    result[i].parameter = linkThresholds[i].parameter;
    result[i].status = 1; //accepted..default
    switch (linkThresholds[i].parameter.parameter_type){
    case LINK_GEN_FRAME_LOSS: 
      watch = &loss_watch_;
      break;
    case LINK_GEN_PACKET_DELAY:
      watch = &delay_watch_;
      break;
    case LINK_GEN_PACKET_JITTER:
      watch = &jitter_watch_;
      break;
    case LINK_GEN_RX_DATA_THROUGHPUT:
      watch = &rx_data_watch_;
      break;
    case LINK_GEN_RX_TRAFFIC_THROUGHPUT:
      watch = &rx_traffic_watch_;
      break;
    case LINK_GEN_TX_DATA_THROUGHPUT:
      watch = &tx_data_watch_;
      break;
    case LINK_GEN_TX_TRAFFIC_THROUGHPUT:
      watch = &tx_traffic_watch_;
      break;
    default:
      fprintf (stderr, "Parameter type not supported %d/%d\n", 
	       linkThresholds[i].parameter.link_type, 
	       linkThresholds[i].parameter.parameter_type);
      result[i].status = 0; //rejected
    }
    watch->set_thresholds (linkThresholds[i].initActionTh.data_d, 
			   linkThresholds[i].rollbackActionTh.data_d ,
			   linkThresholds[i].exectActionTh.data_d);
  }
  return result;
}
 
#endif

/**
 * Update the given timer and check if thresholds are crossed
 * @param watch the stat watch to update
 * @param value the stat value
 */
void Mac802_16::update_watch (StatWatch *watch, double value)
{
  char *name;

#ifdef USE_802_21 //Switch to activate when using 802.21 modules (external package)
  threshold_action_t action = watch->update (value);

  if (action != NO_ACTION_TH) {
    link_parameter_type_s param;
    union param_value old_value, new_value;

    if (watch == &loss_watch_) {
      param.link_type = LINK_GENERIC;
      param.parameter_type = LINK_GEN_FRAME_LOSS;
    } else if (watch == &delay_watch_) {
      param.link_type = LINK_GENERIC;
      param.parameter_type = LINK_GEN_PACKET_DELAY;
    } else if (watch == &jitter_watch_) {
      param.link_type = LINK_GENERIC;
      param.parameter_type = LINK_GEN_PACKET_JITTER;
    }
    old_value.data_d = watch->old_average();
    new_value.data_d = watch->average();
    if (type_ == STA_BS) 
      send_link_parameters_report (addr(), addr(), param, old_value, new_value);      
    else
      send_link_parameters_report (addr(), getPeerNode_head()->getPeerNode(), param, old_value, new_value);      
  }
#else
  watch->update (value);
#endif

  if (watch == &loss_watch_) {
    name = "loss";
  } else if (watch == &delay_watch_) {
    name = "delay";
  } else if (watch == &jitter_watch_) {
    name = "jitter";
  } else {
    name = "other";
  }
  if (print_stats_)
    printf ("At %f in Mac %d, updating stats %s: %f\n", NOW, addr(), name, watch->average());
}

/**
 * Update the given timer and check if thresholds are crossed
 * @param watch the stat watch to update
 * @param value the stat value
 */
void Mac802_16::update_throughput (ThroughputWatch *watch, double size)
{
  char *name;

#ifdef USE_802_21 //Switch to activate when using 802.21 modules (external package)
  threshold_action_t action = watch->update (size, NOW);
  if (action != NO_ACTION_TH) {
    link_parameter_type_s param;
    union param_value old_value, new_value;
    if (watch == &rx_data_watch_) {
      param.link_type = LINK_GENERIC;
      param.parameter_type = LINK_GEN_RX_DATA_THROUGHPUT;
    } else if (watch == &rx_traffic_watch_) {
      param.link_type = LINK_GENERIC;
      param.parameter_type = LINK_GEN_RX_TRAFFIC_THROUGHPUT;
    } else if (watch == &tx_data_watch_) {
      param.link_type = LINK_GENERIC;
      param.parameter_type = LINK_GEN_TX_DATA_THROUGHPUT;
    } else if (watch == &tx_traffic_watch_) {
      param.link_type = LINK_GENERIC;
      param.parameter_type = LINK_GEN_TX_TRAFFIC_THROUGHPUT;
    }
    old_value.data_d = watch->old_average();
    new_value.data_d = watch->average();
    if (type_ == STA_BS) 
      send_link_parameters_report (addr(), addr(), param, old_value, new_value);      
    else
      send_link_parameters_report (addr(), getPeerNode_head()->getPeerNode(), param, old_value, new_value);      
  }
#else
  watch->update (size, NOW);
#endif 

  if (watch == &rx_data_watch_) {
    name = "rx_data";
    rx_data_timer_->resched (watch->get_timer_interval());
  } else if (watch == &rx_traffic_watch_) {
    rx_traffic_timer_->resched (watch->get_timer_interval());
    name = "rx_traffic";
  } else if (watch == &tx_data_watch_) {
    tx_data_timer_->resched (watch->get_timer_interval());
    name = "tx_data";
  } else if (watch == &tx_traffic_watch_) {
    tx_traffic_timer_->resched (watch->get_timer_interval());
    name = "tx_traffic";
  }

  if (print_stats_)
    printf ("At %f in Mac %d, updating stats %s: %f\n", NOW, addr(), name, watch->average());
}
