/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1997 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Daedalus Research
 *	Group at the University of California Berkeley.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Contributed by Giao Nguyen, http://daedalus.cs.berkeley.edu/~gnguyen
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /var/lib/cvs/ns-2.29/mac/mac.cc,v 1.11 2007/02/27 15:54:40 rouil Exp $ (UCB)";
#endif

//#include "classifier.h"

#include <channel.h>
#include <mac.h>
#include <address.h>

//NIST: add include
#include "mih.h"
//end NIST

int hdr_mac::offset_;
static class MacHeaderClass : public PacketHeaderClass {
public:
	MacHeaderClass() : PacketHeaderClass("PacketHeader/Mac",
					     sizeof(hdr_mac)) {
		bind_offset(&hdr_mac::offset_);
	}
	void export_offsets() {
		field_offset("macSA_", OFFSET(hdr_mac, macSA_));
		field_offset("macDA_", OFFSET(hdr_mac, macDA_));
	}
} class_hdr_mac;

static class MacClass : public TclClass {
public:
	MacClass() : TclClass("Mac") {}
	TclObject* create(int, const char*const*) {
		return (new Mac);
	}
} class_mac;


void
MacHandlerResume::handle(Event*)
{
	mac_->resume();
}

void
MacHandlerSend::handle(Event* e)
{
	mac_->sendDown((Packet*)e);
}

/* =================================================================
   Mac Class Functions
  ==================================================================*/
static int MacIndex = 0;

Mac::Mac() : 
	BiConnector(), abstract_(0), netif_(0), tap_(0), ll_(0), channel_(0), callback_(0), 
	hRes_(this), hSend_(this), state_(MAC_IDLE), pktRx_(0), pktTx_(0)	
	/*NIST: add mih init*/, mih_(0), eventId_(0), linkType_(LINK_LAST_TYPE), eventList_(0), commandList_(0), subscribedEventList_(0), op_mode_(NORMAL_MODE) /*end NIST*/
{
	index_ = MacIndex++;
	bind_bw("bandwidth_", &bandwidth_);
	bind_time("delay_", &delay_);
	bind_bool("abstract_", &abstract_);
}

int Mac::command(int argc, const char*const* argv)
{
	if(argc == 2) {
		Tcl& tcl = Tcl::instance();
		
		if(strcmp(argv[1], "id") == 0) {
			tcl.resultf("%d", addr());
			return TCL_OK;
		} else if (strcmp(argv[1], "channel") == 0) {
			tcl.resultf("%s", channel_->name());
			return (TCL_OK);
		}
		
	} else if (argc == 3) {
		TclObject *obj;
		if( (obj = TclObject::lookup(argv[2])) == 0) {
			fprintf(stderr, "%s lookup failed\n", argv[1]);
			return TCL_ERROR;
		}
		else if (strcmp(argv[1], "netif") == 0) {
			netif_ = (Phy*) obj;
			return TCL_OK;
		}
		else if (strcmp(argv[1], "log-target") == 0) {
                        logtarget_ = (NsObject*) obj;
                        if(logtarget_ == 0)
                                return TCL_ERROR;
                        return TCL_OK;
                }
		//NIST: add MIH registration
                else if (strcmp(argv[1], "mih") == 0) {
                        mih_ = (MIHAgent*) obj;
                        return TCL_OK;
                }
		//end NIST
	}
	
	return BiConnector::command(argc, argv);
}

void Mac::recv(Packet* p, Handler* h)
{
	if (hdr_cmn::access(p)->direction() == hdr_cmn::UP) {
		sendUp(p);
		return;
	}

	callback_ = h;
	hdr_mac* mh = HDR_MAC(p);
	mh->set(MF_DATA, index_);
	state(MAC_SEND);
	sendDown(p);
}

void Mac::sendUp(Packet* p) 
{
	char* mh = (char*)p->access(hdr_mac::offset_);
	int dst = this->hdr_dst(mh);

	state(MAC_IDLE);
	if (((u_int32_t)dst != MAC_BROADCAST) && (dst != index_)) {
		if(!abstract_){
			drop(p);
		}else {
			//Dont want to creat a trace
			Packet::free(p);
		}
		return;
	}
	Scheduler::instance().schedule(uptarget_, p, delay_);
}

void Mac::sendDown(Packet* p)
{
	Scheduler& s = Scheduler::instance();
	double txt = txtime(p);
	downtarget_->recv(p, this);
	if(!abstract_)
		s.schedule(&hRes_, &intr_, txt);
}


void Mac::resume(Packet* p)
{
	if (p != 0)
		drop(p);
	state(MAC_IDLE);
	callback_->handle(&intr_);
}


//Mac* Mac::getPeerMac(Packet* p)
//{
//return (Mac*) mcl_->slot(hdr_mac::access(p)->macDA());
//}

/*NIST: add functions to send events */
                                                                                                                
/*
 * Send a link detected to MIHF
 * @param macTerminal The Mobile Terminal address
 * @param macPoA The new PoA detected
 * @param mihCap Indicates if the new PoA supports MIH
 */
void Mac::send_link_detected (int macTerminal, int macPoA, int mihCap)
{
	//check if MIH present and event subscribed
        if (mih_ && ((subscribedEventList_ >> MIH_LINK_DETECTED)& 0x1) ) {		
                link_detected_t *e = (link_detected_t*) malloc (sizeof (link_detected_t));
		e->linkIdentifier.type = linkType_;
                e->linkIdentifier.macMobileTerminal = macTerminal;
                e->linkIdentifier.macPoA = macPoA;
                e->mihCapability = mihCap;
                mih_->recv_event (MIH_LINK_DETECTED, e);
        }
        //else we don't need to do anything
}
                                                                                                                
/*
 * Send a link up to MIHF
 * @param macTerminal The Mobile Terminal address
 * @param macPoA The new PoA we are connected to
 * @param macOldPoA old access router.
 */
void Mac::send_link_up (int macTerminal, int macPoA, int macOldPoA)
{
        if (mih_ && ((subscribedEventList_ >> MIH_LINK_UP)& 0x1) ) {
                link_up_t *e = (link_up_t*) malloc (sizeof (link_up_t));
		e->linkIdentifier.type = linkType_;
                e->linkIdentifier.macMobileTerminal = macTerminal;
                e->linkIdentifier.macPoA = macPoA;
		//in NS, the AP is also the AR
		e->macOldAccessRouter = macOldPoA;
		e->macNewAccessRouter = macPoA;
		e->ipRenewal = 0; //each AP has different subnet so we need renew address
		e->mobilityManagement = 0; //not supported
                mih_->recv_event (MIH_LINK_UP, e);
        }
        //else we don't need to do anything
}
                                                                                                                
/*
 * Send a link down to MIHF
 * @param macTerminal The Mobile Terminal address
 * @param reason The reason for loosing the link
 */
void Mac::send_link_down (int macTerminal, int oldMacAR, link_down_reason_t reason)
{
        if (mih_ && ((subscribedEventList_ >> MIH_LINK_DOWN)& 0x1) ) {
                link_down_t *e = (link_down_t*) malloc (sizeof (link_down_t));
                e->linkIdentifier.type = linkType_;
                e->linkIdentifier.macMobileTerminal = macTerminal;
		e->linkIdentifier.macPoA = oldMacAR; 
		e->macOldAccessRouter = oldMacAR; 
                e->reason = reason;
                mih_->recv_event (MIH_LINK_DOWN, e);
        }
        //else we don't need to do anything
}

/*
 * Send a link going down to MIHF
 * @param macTerminal The Mobile Terminal address
 * @param macPoA The PoA we are connected to
 * @param interval Time we expect to loose connection
 * @param confidence The level of confidance (1-100%)
 * @param id The unique event id
 */
void Mac::send_link_going_down (int macTerminal, int macPoA, double interval, int confidence, link_going_down_reason_t reason, unsigned int id)
{
        if (mih_ && ((subscribedEventList_ >> MIH_LINK_GOING_DOWN)& 0x1) ) {		
                link_going_down_t *e = (link_going_down_t*) malloc (sizeof (link_going_down_t));
                //e->linkIdentifier.eventSource = this;
                e->linkIdentifier.type = linkType_;
                e->linkIdentifier.macMobileTerminal = macTerminal;
                e->linkIdentifier.macPoA = macPoA;
                e->timeInterval = (unsigned int)(interval*1000);
                e->confidenceLevel = confidence;
		e->reasonCode = reason;
                e->uniqueEventIdentifier = id;
                mih_->recv_event (MIH_LINK_GOING_DOWN, e);
        }
        //else we don't need to do anything
}
                                                                                                                
/*
 * Send an event to cancel a previous event to MIHF
 * @param macTerminal The Mobile Terminal address
 * @param macPoA The PoA we are connected to
 * @param id The id of the event to cancel
 */
void Mac::send_link_rollback (int macTerminal, int macPoA ,unsigned int id)
{
        if (mih_ && ((subscribedEventList_ >> MIH_LINK_EVENT_ROLLBACK)& 0x1) ) {
                link_rollback_t *e = (link_rollback_t*) malloc (sizeof (link_rollback_t));
                //e->linkIdentifier.eventSource = this;
                e->linkIdentifier.type = linkType_;
                e->linkIdentifier.macMobileTerminal = macTerminal;
		e->linkIdentifier.macPoA = macPoA; //set value because not used		
                e->uniqueEventIdentifier = id;
                mih_->recv_event (MIH_LINK_EVENT_ROLLBACK, e);		
        } 
        //else we don't need to do anything
}

/*
 * Send a link parameter change to MIHF
 * @param macTerminal The Mobile Terminal address
 * @param macPoA The MAC address of PoA 
 * @param event_type The type of event to configure
 * @param old_value The old value
 * @param new_value The new value
 */
void Mac::send_link_parameters_report (int macTerminal, int macPoA, link_parameter_type_s event_type, union param_value old_value, union param_value new_value)
{
        if (mih_ && ((subscribedEventList_ >> MIH_LINK_PARAMETERS_REPORT)& 0x1) ) {
                link_parameters_report_t *e = (link_parameters_report_t*) malloc (sizeof (link_parameters_report_t));
                e->linkIdentifier.type = linkType_;
                e->linkIdentifier.macMobileTerminal = addr();
		e->linkIdentifier.macPoA = macPoA;
		//currently only support 1 parameter in list
		e->numLinkParameters = 1;
                e->linkParameterList[0].parameter = event_type;
                e->linkParameterList[0].old_value = old_value;
                e->linkParameterList[0].new_value = new_value;
                mih_->recv_event (MIH_LINK_PARAMETERS_REPORT, e);
        }
	//else we don't need to do anything
}

/*
 * Send a link SDU transmit status to MIHF
 * @param macTerminal The Mobile Terminal address
 * @param packetID packet identifier
 * @param status transmission status
 */
void Mac::send_link_sdu_transmit_status (int macTerminal, int packetID, transmit_status_t status)
{
        if (mih_ && ((subscribedEventList_ >> MIH_LINK_SDU_TRANSMIT_STATUS)& 0x1) ) {
                link_sdu_transmit_status_t *e = (link_sdu_transmit_status_t*) malloc (sizeof (link_sdu_transmit_status_t));
                //e->linkIdentifier.eventSource = this;
                e->linkIdentifier.type = linkType_;
                e->linkIdentifier.macMobileTerminal = macTerminal;
		e->linkIdentifier.macPoA = -1; //set value because not used
		e->packetIdentifier = packetID;
		e->transmitStatus = status;
                mih_->recv_event (MIH_LINK_DOWN, e);
        }
        //else we don't need to do anything
}


/*
 * Send a link handoff imminent to MIHF
 * @param macTerminal The Mobile Terminal address
 * @param oldPoA The old PoA (which is also the AccessRouter for us)
 * @param macPoA The new PoA we are connected to
 */
void Mac::send_link_handover_imminent (int macTerminal, int oldPoA, int macPoA)
{
        if (mih_ && ((subscribedEventList_ >> MIH_LINK_HANDOVER_IMMINENT)& 0x1) ) {
                link_handover_imminent_t *e = (link_handover_imminent_t*) malloc (sizeof (link_handover_imminent_t));
		e->linkIdentifier.type = linkType_;
                e->linkIdentifier.macMobileTerminal = macTerminal;
                e->linkIdentifier.macPoA = oldPoA;
		e->newLinkIdentifier.type = linkType_;
                e->newLinkIdentifier.macMobileTerminal = macTerminal;
                e->newLinkIdentifier.macPoA = macPoA;
                e->macOldAccessRouter = oldPoA;
                e->macNewAccessRouter = macPoA;
                mih_->recv_event (MIH_LINK_HANDOVER_IMMINENT, e);
        }
        //else we don't need to do anything
}

/*
 * Send a link handoff complete to MIHF
 * @param macTerminal The Mobile Terminal address
 * @param oldPoA The old PoA (which is also the AccessRouter for us)
 * @param macPoA The new PoA we are connected to
 */
void Mac::send_link_handover_complete (int macTerminal, int oldPoA, int macPoA)
{
        if (mih_ && ((subscribedEventList_ >> MIH_LINK_HANDOVER_COMPLETE)& 0x1) ) {
                link_handover_complete_t *e = (link_handover_complete_t*) malloc (sizeof (link_handover_complete_t));
		e->linkIdentifier.type = linkType_;
                e->linkIdentifier.macMobileTerminal = macTerminal;
                e->linkIdentifier.macPoA = macPoA;
                e->macOldAccessRouter = oldPoA;
                e->macNewAccessRouter = macPoA;
                mih_->recv_event (MIH_LINK_HANDOVER_COMPLETE, e);
        }
        //else we don't need to do anything
}


/*
 * Sends the results of the scanning
 * @param res The media-dependent results
 */
void Mac::send_scan_result (void *res, int size) {
        if (mih_) {
		mih_->process_scan_response (this, res, size);
        }
}


/*
 * Connect to the given PoA
 * @param The MAC address of the PoA to connect
 */
void Mac::link_connect (int macPoA)
{
        //to be overwritten by subclass
}
        
/*
 * Connect to the given PoA
 * @param The MAC address of the PoA to connect
 * @param channel The channel to use
 */
void Mac::link_connect (int macPoA, int channel)
{
        //to be overwritten by subclass
}

/*
 * Disconnect from the current PoA
 */
void Mac::link_disconnect ()
{
        //to be overwritten by subclass
}

/*
 * Scan to retreive lists of potential PoA
 * @param request The technology-dependent request
 */
void Mac::link_scan (void *request) {
        //to be overwritten by subclass
}


//MIH-LINK SAP functions

/*
 * Return the link capability (supported events and commands)
 * @return link capability
 */
capability_list_s* Mac::link_capability_discover () {
	capability_list_s *caps = (capability_list_s*) malloc (sizeof (capability_list_s));
	caps->events = eventList_;
	caps->commands = commandList_;
	return caps;
}

/*
 * Return the link capability (supported events and commands)
 * @return link capability
 */
link_event_list Mac::link_event_discover () {
	return eventList_;
}

/*
 * Register to receive the notifications for the given events
 * @param events The list of events to subscribe
 * @return the subscription status
 */
link_event_status* Mac::link_event_subscribe (link_event_list events) {
	link_event_status *status = (link_event_status *) malloc (sizeof (link_event_status));
	//status to 1 only if supported and subscription is requested
	*status = events & eventList_;
	subscribedEventList_ = subscribedEventList_ | *status;
	return status;
}

/*
 * Unregister to receive the notifications for the given events
 * @param events The list of events to subscribe
 * @return the unsubscription status
 */
link_event_status* Mac::link_event_unsubscribe (link_event_list events) {
	link_event_status *status = (link_event_status *) malloc (sizeof (link_event_status));
	//status to 0 only if already register and unsubscription is requested
	*status = events & subscribedEventList_;
	subscribedEventList_ = subscribedEventList_ & !*status;
	return status;
}

/*
 * Configure the thresholds
 * @param numLinkParameter number of thresholds to configure
 * @param config The configuration to apply
 */
struct link_param_th_status * Mac::link_configure_thresholds (int numLinkParameter, struct link_param_th *thresholdList)
{
	//to be overwritten by subclass
	return NULL;
}

/*
 * Return the value of the given parameters 
 * @param parameter The list of parameters requested
 * @return the parameters' values or NULL
 */
struct link_get_param_s* Mac::link_get_parameters (link_parameter_type_s* parameter)
{
	//to be overwritten by subclass
	return NULL;
}

/* 
 * Set the operation mode for this MAC
 * Note: by default the operation mode cannot be changed
 *       subclasses will define and implement proper operation
 * @param mode The new operation mode
 * @return true if the operation is successful
 */
bool Mac::set_mode (mih_operation_mode_t mode)
{
	return false;
}


//end NIST






