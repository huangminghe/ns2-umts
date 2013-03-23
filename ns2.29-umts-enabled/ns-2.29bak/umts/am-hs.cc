/*
 * Copyright (c) 2003 Ericsson Telecommunicatie B.V.
 * Copyright (c) 2005 Twente Institute for Wireless and Mobile
 *		Communications B.V.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the
 *     distribution.
 * 3. Neither the name of Ericsson Telecommunicatie B.V. or WMC may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY ERICSSON TELECOMMUNICATIE B.V. AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESSED OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ERICSSON TELECOMMUNICATIE B.V., THE AUTHOR OR HIS
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * 
 * Contact for feedback on EURANE: eurane@ti-wmc.nl
 * EURANE = Enhanced UMTS Radio Access Network Extensions
 * website: http://www.ti-wmc.nl/eurane/
 */

/*
 * $Id: am-hs.cc,v 1.2 2006/03/10 14:16:45 rouil Exp $
 */

#include "am-hs.h"
#include "mac.h"
#include "flags.h"
#include "random.h"
#include "address.h"
#include "hsdpalink.h"
#include "timer-handler.h"

// These variables are used as 'super-globals', in this way the AM-HS object
// and the MAC-hs object at the BS can be reached.

RLC_HS     *rlc_address;        //= this;
extern HsdpaMac *mac_address;   //defined in hsdpalink.cc

static class AM_HSRlcClass:public TclClass {
public:
   AM_HSRlcClass():TclClass("UMTS/RLC/AMHS") {
   } TclObject *create(int, const char *const *) {
      return (new AM_HS);
   }
}

class_am_hs;

AM_HS::AM_HS():RLC_HS(), creditTimer_(this, RLC_TIMER_CREDIT)
{
   rlc_address = this;

   bind("win_", &win_);
   bind_time("temp_pdu_timeout_time_", &tempPDUTimeOutTime_);
   bind("credit_allocation_interval_", &creditAllocationInterval_);
   bind("flow_max_", &flowMax_);
   bind("priority_max_", &priorityMax_);
   bind("buffer_level_max_", &bufferLevelMax_);
   bind("status_timeout_", &status_timeout_);
   bind("payload_", &payloadSize_);
   bind_time("TTI_", &TTI_);
   bind("poll_PDU_", &poll_PDU_);
   bind_time("poll_timeout_", &poll_timeout_);
   bind_time("stprob_timeout_", &stprob_timeout_);
   bind("length_indicator_", &length_indicator_);
   bind("status_pdu_header_", &status_pdu_header_);
   bind("min_concat_data_", &min_concat_data_);
   bind("macDA_", &macDA_);


   // Calculate the size in bytes of Length Indicator including the
   // Extention bit. If a PDU consists of multiple parts, for each
   // part these two fields are added to the header. If the PDU consists
   // of only one part, these fields are omitted (9.2.1.4 + 9.2.2.5 from
   // 3GPP TS 25.322 v5.6.0).
   lengthIndicatorSize_ = ((length_indicator_ + 1) / 8);

   // credit_allocation_interval_ is specified in numbers of TTIs, for our
   // timer, we need it in seconds.
   creditAllocationTimeoutInterval_ = TTI_ * creditAllocationInterval_;

   // Initialize the vectors for the Flow Information and Transmission Buffers
   for (int i = 0; i < flowMax_; i++) {
      am_flow_information *temp = new am_flow_information(this, i);

      memset(temp->seen_, 0, sizeof(temp->seen_));
      for (int j = 0; j < priorityMax_; j++) {
         umtsQueue  *temp2 = new umtsQueue;

         temp->transmissionBuffer_.push_back(temp2);
      }
      flowInfo_.push_back(temp);
   }

   // schedule credit allocation timer for the first time
   creditTimer_.sched(creditAllocationTimeoutInterval_);

}


int AM_HS::command(int argc, const char *const *argv)
{
   if (argc == 2) {
      Tcl & tcl = Tcl::instance();
      if (strcmp(argv[1], "TTI") == 0) {
         tcl.resultf("%f", TTI_);
         return TCL_OK;
      } else if (strcmp(argv[1], "BW") == 0) {
         tcl.resultf("%f", bandwidth_);
         return TCL_OK;
      } else if (strcmp(argv[1], "start_TTI") == 0) {
         // do nothing (so ignore this methodcall, the timers are started by the
         // constructor)
         return TCL_OK;
      }
   } else if (argc == 3) {
      if (strcmp(argv[1], "macDA") == 0) {
         int nodeID = Address::instance().str2addr(argv[2]);

         // The node-ids of the UEs start at 2, so decrease the node-id by two
         // to get the flow-id
         flowInfo_.at(nodeID - 2)->macDA_ = nodeID;
         return (TCL_OK);
      } else if (strcmp(argv[1], "addr") == 0) {
         address_ = Address::instance().str2addr(argv[2]);
         return (TCL_OK);
      } else if (strcmp(argv[1], "daddr") == 0) {
         int nodeID = Address::instance().str2addr(argv[2]);

         // The node-ids of the UEs start at 2, so decrease the node-id by two
         // to get the flow-id
         flowInfo_.at(nodeID - 2)->d_address_ = nodeID;
         return (TCL_OK);
      }
   }
   int returnvalue = LL::command(argc, argv);


   return returnvalue;
}

void AM_HS::recv(Packet * p, Handler * h)
{


   hdr_cmn    *ch = HDR_CMN(p);

   // If the direction is up, we don't have to change anything, and the packet
   // can be processed.
   if (ch->direction() == hdr_cmn::UP) {
      hdr_rlc    *llh = hdr_rlc::access(p);
      int flowID = hdr_ip::access(p)->flowid();


      if (llh->dst() != address_ || ch->error() > 0) {
         // The packet was meant for another UE, or it contained errors. In
         // both cases it will be dropped.
         Packet::free(p);
         return;
      }
      // Check the type op packets
      if (llh->lltype() == RLC_BACK) {
         // In case the packet is a bitmap acknowledgement, process it and then
         // free it.
         processBitmapAck(p);
         Packet::free(p);
      } else if (llh->lltype() == RLC_DATA || llh->lltype() == RLC_PB_BACK) {
         // In case the packet is a piggybacked bitmap acknowledgement or a
         // datapacket the data should be handled, possibly after the
         // piggybacked bitmap acknowledgement is processed.
         if (llh->lltype() == RLC_PB_BACK) {
            processBitmapAck(p);
         }
         // Figure out if any SDUs can be handed up.
         int numSDU = update(llh->seqno(), llh->eopno(), flowID);

         // Check whether we can see that a PDU is missing. If that is the case
         // we can already send a bitmap acknowledgement and we do not need to
         // wait until the sender sends a poll for a bitmap acknowledgement.
         checkForMissingPDU(p);

         // We only store the last PDU of an SDU. The rest of the PDUs are not
         // needed for the reassembly and can be freed.
         if (llh->seqno() == llh->eopno() && numSDU >= 0) {
            flowInfo_.at(flowID)->sduB_.orderedEnque(p);
         } else {
            Packet::free(p);
         }
         // Reassemble and send up the SDUs that are ready
         if (numSDU > 0) {
            makeSDU(numSDU, flowID);
         }
      } else {
         // In case the type of the packet is something else, just drop it.
         drop(p);               // Unknown type, inclusing Positive Acknowledgements
      }
      return;
   } else {
      // We think that this should always be the case, so why set it again? For
      // testing purposes we should replace the following line by an assert
      // statement to check it.
      ch->direction() = hdr_cmn::DOWN;
      sendDown(p);
   }

}

void AM_HS::enqueInBackOfTransmissionBuffer(Packet * p)
{

  //NIST: suppress warning for unused variable
  //hdr_cmn *p_ch = HDR_CMN(p);
  //end NIST


   int flowID = hdr_ip::access(p)->flowid();
   int priority = hdr_ip::access(p)->prio();



   assert(!
          ((flowInfo_.at(flowID)->transmissionBuffer_.at(priority)->length()) >=
           bufferLevelMax_));

   flowInfo_.at(flowID)->transmissionBuffer_.at(priority)->enque(p);


}


// This method will always enque the packet in the front of the queue. This
// method is only called when packets are retransmitted.
// For this reason we do not use the maximum buffer size, so in fact, the
// maximum buffer size does not guarantee that the size of the buffer will
// be smaller or equal to the maxmum buffersize, only that when the buffer
// is full (has more or equal packets in it as the maximum buffersize) no
// *new* packets are admitted.
//
// When this approach was not followed and packets were dropped from the back
// of the queue when the buffersize was larger or equal to the maximum
// buffersize the following occurs:
//  - The buffer is full
//  - A burst of packets is sent
//  - Many new packets arrive and fill up the buffer again
//  - Negative acknowledgements from the sent burst arrive, dropping many
//    of the newly arived new packets, which will need to be retransmitted
//    again.

int AM_HS::enqueInFrontOfTransmissionBuffer(Packet * p)
{
// return 1 when succesfully enqueued, -1 when buffer is full and
// a packet is dropped

   int r_value = 1;


   int flowID = hdr_ip::access(p)->flowid();
   int priority = hdr_ip::access(p)->prio();


   flowInfo_.at(flowID)->transmissionBuffer_.at(priority)->enqueUniqueFront(p);

   return r_value;
}

//---------------------------
// sendDown processes a SDU into PDUs for transmission to MAC-HS
// if possible, waiting temporary packets are filled up.
//---------------------------

void AM_HS::sendDown(Packet * p)
{

   // First check whether the SDU fits into the transmission buffer completely

   int numPDUs =
         (int) ceil((double) hdr_cmn::access(p)->size() /
                    (double) payloadSize_);

   // check includes possible space for a temporary packet (+1).
   if (1 + numPDUs +
       flowInfo_.at(hdr_ip::access(p)->flowid())->transmissionBuffer_.
       at(hdr_ip::access(p)->prio())->length() >= bufferLevelMax_) {
      Packet::free(p);
      return;
   }

   // Second check whether there is still a not-finished PDU available that can
   // be used for concatenation.
   handleTemporaryPacket(p);

   while (p != NULL) {
      // Each loop the front of the SDU is removed and send in a PDU. This
      // results in a smaller and smaller SDU, until at some point in time
      // it is completely gone. In that case p equals to NULL and the
      // complete SDU is segmented.

      int concat_data = 0;      // Number of bytes that can be used for concatenation
      bool close_PDU = false;   // Defines whether this is the last PDU of an SDU.
      int padding = 0;          // Number of bytes that have to be padded
      bool force_length_indicator = false;
      Packet     *c;            // The newly constructed PDU.

      int p_size = hdr_cmn::access(p)->size();

      close_PDU = p_size <= payloadSize_;
      force_length_indicator = p_size < payloadSize_;

      if (p_size <= (payloadSize_ - lengthIndicatorSize_)) {
         // decide whether the remaining data is too big too keep
         // in which case we send it on with padding
         concat_data = payloadSize_ - p_size - 2 * lengthIndicatorSize_;
         if (concat_data < min_concat_data_) {
            concat_data = 0;
            padding = payloadSize_ - p_size - lengthIndicatorSize_;
         } else {
            padding = 0;
         }
      }

      if (close_PDU) {
         // reuse packet p
         c = p;
         p = NULL;
      } else {
         // copy p's data into c and force concat_data and padding to 0
         concat_data = 0;
         padding = 0;
         c = p->copy();
      }


      //initialise shortcuts to header info of packets c (and p)
      hdr_cmn    *c_ch = HDR_CMN(c);
      hdr_rlc    *c_llh = hdr_rlc::access(c);
      hdr_ip     *c_iph = hdr_ip::access(c);
      hdr_tcp    *c_tcph = hdr_tcp::access(c);
      int flowID = c_iph->flowid();

      hdr_cmn    *p_ch = NULL;
      hdr_ip     *p_iph = NULL;
      hdr_tcp    *p_tcph = NULL;

      if (p != NULL) {
         p_ch = HDR_CMN(p);
         p_iph = hdr_ip::access(p);
         p_tcph = hdr_tcp::access(p);
      }

      c_llh->lptype() = c_ch->ptype();
      c_llh->lerror_ = c_ch->error();
      c_llh->lts_ = c_ch->timestamp();

      if (close_PDU) {

         if (flowInfo_.at(flowID)->SDU_size_ > 0) {
            c_llh->lsize_ = flowInfo_.at(flowID)->SDU_size_;
         } else {
            c_llh->lsize_ = c_ch->size();
         }

         flowInfo_.at(flowID)->SDU_size_ = -1;

      } else if (flowInfo_.at(flowID)->SDU_size_ < 0) {
         assert(p != NULL);
         flowInfo_.at(flowID)->SDU_size_ = hdr_cmn::access(p)->size();
      }

      c_llh->lltype() = RLC_DATA;
      c_llh->seqno() = -1;      // will be set at the moment the packet is realy sent
      c_llh->dst() = flowInfo_.at(flowID)->d_address_;
      c_llh->src() = address_;
      c_llh->lengthInd_ = 0;

      if (close_PDU) {
         // c_llh->eopno_ needs to be set to c_llh->seqno_. However, at this
         // moment the seqno is not known yet. So, set it to EOPNO_TO_SEQNO
         // and check when seqno is set whether eopno is EOPNO_TO_SEQNO. If this
         // is the case eopno can be replaced by seqno.
         c_llh->eopno_ = EOPNO_TO_SEQNO;
      } else {
         c_llh->eopno_ = EOPNO_NOT_EOP;
      }

      if (concat_data || padding || force_length_indicator) {
         c_llh->lengthInd_++;
      }
      // The payload_ field is used to define the size of the parts between the
      // different length indicators. The first element is used for the size of
      // the part for the first SDU. In case concatenation occurs, the second
      // element indicates the size of the part used for the second SDU. In case
      // a status PDU is piggybacked, the last element (so, without concatenation
      // the second, with concatenation the third) is used to indicate the size
      // of the status PDU.

      if (close_PDU) {
         c_llh->payload_[PAYLOAD_FLD1] = c_ch->size();
      } else {
         if (force_length_indicator) {
            c_llh->payload_[PAYLOAD_FLD1] = payloadSize_ - lengthIndicatorSize_;
         } else {
            c_llh->payload_[PAYLOAD_FLD1] = payloadSize_;
         }
      }
      c_llh->payload_[PAYLOAD_FLD2] = 0;
      c_llh->payload_[PAYLOAD_FLD3] = 0;

      c_llh->padding_ = padding; //padding will always have the correct value here

      // WTF? this is really ugly, bad things will happen here!
      char       *mh = (char *) c->access(hdr_mac::offset_);
      struct hdr_mac *c_dh = (struct hdr_mac *) mh;

      c_ch->ptype() = PT_AMDA;
      c_dh->macDA_ = flowInfo_.at(flowID)->macDA_;
      c_dh->macSA_ = -1;
      c_dh->hdr_type() = ETHERTYPE_RLC;
      c_ch->timestamp() = Scheduler::instance().clock();

      // Do we have space left over for concatenation?
      if (close_PDU) {
         if (concat_data) {
            // Store the current packet and do not send it down.
            // Because this is also the last part of the SDU we can
            // stop this method.
            c_llh->lengthInd_++;

            StoreTemporaryPacket(c, concat_data);
            break;              //exit while loop
         }
      } else {
         // reduce the size of the SDU by an amount of the
         // payload that will be sent in this PDU.
         p_ch->size() = p_ch->size() - c_llh->payload_[PAYLOAD_FLD1];

         assert(c_iph->flowid() == p_iph->flowid());
         assert(c_iph->saddr() == p_iph->saddr());

         assert(c_tcph->seqno() == p_tcph->seqno());
      }

      assert(c_ch->error() == 0);


      c_ch->size() = payloadSize_;




      // Finally, enque the packet in the transmission buffer.
      enqueInBackOfTransmissionBuffer(c);

   }


}

// This method is called when a PDU is created that is not full, and just stores
// the PDU into a vector that will be inspected

void AM_HS::StoreTemporaryPacket(Packet * p, int concat_data)
{


   temporaryPacket *temp = new temporaryPacket(this);

   temp->p = p;
   temp->concat_data = concat_data;
   // We can't wait forever, because then we have a problem at the end of a
   // burst of traffic: then the last PDU won't be send, causing timeouts and
   // retransmissions. So, we do want to send the incomplete packet anyway
   // at some point in time.
   temp->tempPDUTimer.sched(tempPDUTimeOutTime_);
   temporaryPackets_.push_back(temp);
}


// This method is called at the beginning of each arriving SDU. It checks
// whether a packet with the same flow-id and priority that isn't complete
// yet is waiting for concatenation. If it finds one, concatenation occurs
// if possible. If this is not possible the waiting PDU is stored in the
// transmission buffer. When concatenation occurs, the waiting packet is
// filled, and is sent; the size of the newly arrived SDU is decreased by
// the number of concatenated bytes. After this method, sendSDU continues
// segmenting the SDU into PDUs.

void AM_HS::handleTemporaryPacket(Packet * p)
{


   if (p == NULL) {
      return;
   }
   // get the flow-id and priority for the current PDU
   int flowID = hdr_ip::access(p)->flowid();
   int priority = hdr_ip::access(p)->prio();


   // Search all current temporary PDUs to find a temporary PDU with
   // the same flow-id and priority
   for (unsigned int i = 0; i < temporaryPackets_.size(); i++) {


      if (hdr_ip::access(temporaryPackets_.at(i)->p)->flowid() == flowID
          && hdr_ip::access(temporaryPackets_.at(i)->p)->prio() == priority) {
         // We have found one

         // Fill the temporary PDU with data from the new SDU, and delete that
         // part of the SDU. Then store the temporary PDU in the Transmission
         // and Retransmission Buffers and return the remaining part of the SDU.

         // Concatenation (3GPP TS 25.301 5.3.2.2): If the contents of an RLC
         // SDU cannot be carried by one RLC PDU, the first segment of the next
         // RLC SDU may be put into the RLC PDU in concatenation with the last
         // segment of the previous RLC SDU.

         if (hdr_cmn::access(p)->size() > payloadSize_) {

            hdr_rlc::access(temporaryPackets_.at(i)->p)->payload_[1] =
                  temporaryPackets_.at(i)->concat_data;

            // this variable was previously set in makePDU, it holds the
            // original SDU size.
            flowInfo_.at(hdr_ip::access(p)->flowid())->SDU_size_ =
                  hdr_cmn::access(p)->size();
            hdr_cmn::access(p)->size() =
                  hdr_cmn::access(p)->size() -
                  temporaryPackets_.at(i)->concat_data;

         } else {
            // We cannot concatenate: decrease the number of length indicators
            // (it has already been updated because we would expect that
            // concatenation would happen), and add padding to the packet.
            hdr_rlc::access(temporaryPackets_.at(i)->p)->lengthInd_--;
            hdr_rlc::access(temporaryPackets_.at(i)->p)->padding_ =
                  temporaryPackets_.at(i)->concat_data + lengthIndicatorSize_;
         }

         //NEILL: Set temp packet to full payload size??
         hdr_cmn::access(temporaryPackets_.at(i)->p)->size() = payloadSize_;
         //****************************************************************


         enqueInBackOfTransmissionBuffer(temporaryPackets_.at(i)->p);

         // Now the temporary PDU has been sent, delete the timer
         // so that the timeout will not occur anymore.
         temporaryPackets_.at(i)->tempPDUTimer.cancel();
         temporaryPackets_.erase((temporaryPackets_.begin() + i));
         return;

      }
   }
}


void AM_HS::timeout(int tno, int flowID)
{
   vector < int >temp_vect;


   switch (tno) {
     case RLC_TIMER_POLL:
        for (unsigned int i = 0; i < flowInfo_.size(); i++) {
           if (Scheduler::instance().clock() >=
               flowInfo_.at(i)->poll_timer_.timeOfExpiry()
               && (flowInfo_.at(i)->poll_timer_.status() == TIMER_PENDING
                   || flowInfo_.at(i)->poll_timer_.status() ==
                   flowInfo_.at(i)->poll_timer_.TIMER_HANDLING)) {
              if (flowInfo_.at(i)->maxseq_ == flowInfo_.at(i)->FSN_
                  && flowInfo_.at(i)->b_bal_ == 0) {
                 // If no outstanding data, then don't do anything.
                 return;
              }
              flowInfo_.at(i)->poll_seq_ = -1;
              flowInfo_.at(i)->set_poll_ = 1;
           }
        }

        break;
     case RLC_TIMER_STPROB:
        for (unsigned int i = 0; i < flowInfo_.size(); i++) {
           if (Scheduler::instance().clock() >=
               flowInfo_.at(i)->stprob_timer_.timeOfExpiry()
               && (flowInfo_.at(i)->stprob_timer_.status() == TIMER_PENDING
                   || flowInfo_.at(i)->stprob_timer_.status() ==
                   flowInfo_.at(i)->poll_timer_.TIMER_HANDLING)) {
              if (flowInfo_.at(i)->prohibited_) {
                 flowInfo_.at(i)->send_status_ = 1;
                 flowInfo_.at(i)->prohibited_ = 0;
                 flowInfo_.at(i)->sinfo_timer_.sched(status_timeout_);
              }
           }
        }
        break;
     case RLC_TIMER_TEMP_PDU:
        // Walk through all temporary PDUs and check whether their
        // timeout-time is the same as the current time or lies in the
        // past (actually, this shouldn't happen). If that is the case
        // send those PDUs and remove them from the vector.
        for (unsigned int i = 0; i < temporaryPackets_.size(); i++) {

           // For performance reasons it might be a good idea to
           // not only select the temporary PDUs that have to be sent
           // now, but also the PDUs that are due within a small
           // period in time, so that timeouts do not occur that
           // often anymore.
           if (Scheduler::instance().clock() >=
               temporaryPackets_.at(i)->tempPDUTimer.timeOfExpiry()) {
              // We have found one

              // Before we stored the temporary PDU in the
              // vector, we set the number of Length Indicators
              // and the number of bytes that could be
              // concatenated. Because we are not concatenating,
              // but padding we need one Length Indicator less,
              // thus we have to pad more.
              hdr_rlc::access(temporaryPackets_.at(i)->p)->lengthInd_--;
              hdr_rlc::access(temporaryPackets_.at(i)->p)->padding_ =
                    temporaryPackets_.at(i)->concat_data + lengthIndicatorSize_;

              //NEILL: set packet size to full payload size
              hdr_cmn::access(temporaryPackets_.at(i)->p)->size() = payloadSize_;
              //****************************************************************

              enqueInBackOfTransmissionBuffer(temporaryPackets_.at(i)->p);
              temporaryPackets_.erase((temporaryPackets_.begin() + i));
              i--;              // otherwise the next temporary packet will be skipped
           }
        }
        break;
     case RLC_TIMER_CREDIT:
        // Send a credit allocation request to the MAC-hs. And reschedule
        // the timer

        // Create a vector with the length of the Transmission Buffers, and
        // send this vector to the MAC-hs.

        //Note: Buffers indexed: f1p1,f2p1,f3p1,f1p2,f2p2,f3p2,f1p3...


        for (int i = 0; i < priorityMax_; i++) {
           for (int j = 0; j < flowMax_; j++) {
              temp_vect.push_back(flowInfo_.at(j)->transmissionBuffer_.at(i)->
                                  length());


           }
        }

        mac_address->cred_alloc(&temp_vect);


        // Reschedule
        creditTimer_.resched(creditAllocationTimeoutInterval_);
        break;
     case RLC_TIMER_STATUS:
        // When this timer times out, the status information that needs to be sent
        // can't be delayed anymore. So, now we have to construct a Status PDU and
        // send it using the highest priority.
        for (unsigned int i = 0; i < flowInfo_.size(); i++) {
           if (Scheduler::instance().clock() >=
               flowInfo_.at(i)->sinfo_timer_.timeOfExpiry()
               && (flowInfo_.at(i)->sinfo_timer_.status() == TIMER_PENDING
                   || flowInfo_.at(i)->sinfo_timer_.status() ==
                   flowInfo_.at(i)->poll_timer_.TIMER_HANDLING)) {
              Packet     *p = createBitmapAck(i);

              enqueInBackOfTransmissionBuffer(p);
              flowInfo_.at(i)->stprob_timer_.resched(stprob_timeout_);
           }
        }
        break;
     default:

        break;
   }

}


// This method is called just before a packet is really sent. It does:
//   - It checks whether the poll bit needs to be set
//   - It sets the sequence number

void AM_HS::completePDU(Packet * p)
{


   hdr_rlc    *llh = hdr_rlc::access(p);
   int flowID = hdr_ip::access(p)->flowid();

   // Check whether we have to poll for a bitmap status update. This is true
   // when it is explicitly defined by the field set_poll_ that can be set by
   // other methods, or when the number of transmitted PDUs is a multiplicity
   // of poll_PDU_ (the 'Poll every X PDU' setting).
   if (flowInfo_.at(flowID)->set_poll_ == 1
       || flowInfo_.at(flowID)->tx_PDUs_ > poll_PDU_) {
      hdr_rlc::access(p)->poll() = true;
      flowInfo_.at(flowID)->set_poll_ = 0;
      flowInfo_.at(flowID)->tx_PDUs_ = 0;
      if ((hdr_rlc::access(p)->seqno() >= flowInfo_.at(flowID)->poll_seq_)
          || (flowInfo_.at(flowID)->poll_timer_.status() != TIMER_PENDING)) {
         flowInfo_.at(flowID)->poll_seq_ = hdr_rlc::access(p)->seqno();
         // Because a poll will be send, cancel the poll timer and reschedule
         // it.
         flowInfo_.at(flowID)->poll_timer_.resched(poll_timeout_);
      }
   }
   // If the packet is transmitted for the first time, set the sequence number
   // and possibly the End Of Packet (eop) number.
   if (llh->seqno() == -1) {
      llh->seqno() = flowInfo_.at(flowID)->seqno_;
      flowInfo_.at(flowID)->seqno_++;
      if (llh->eopno() == EOPNO_TO_SEQNO) {
         llh->eopno() = llh->seqno();
      }
   }
   return;
}

// This method is called just before a packet is really sent. It does:
//   - It tries to piggyback status information

void AM_HS::insertStatusInformation(Packet * p)
{


   hdr_cmn    *ch = hdr_cmn::access(p);
   hdr_rlc    *llh = hdr_rlc::access(p);
   int flowID = hdr_ip::access(p)->flowid();

   // If the type of the packet is something else than AM_Data, don't do
   // anything with it, otherwise we might piggyback a bitmap ack on top
   // of a bitmap ack.
   if (ch->ptype() == PT_AMDA) {
      // Check whether a Bitmap Ack is due to be sent and if so, whether it can
      // be piggybacked.
      if (flowInfo_.at(flowID)->send_status_ == 1) {

         if (llh->padding_ >= StatusPDUSize(flowID)) {
            p = createBitmapAck(flowID, p);
            flowInfo_.at(flowID)->send_status_ = 0;
            if (flowInfo_.at(flowID)->sinfo_timer_.status() == TIMER_PENDING) {
               flowInfo_.at(flowID)->sinfo_timer_.cancel();
            }
            flowInfo_.at(flowID)->stprob_timer_.resched(stprob_timeout_);
         }
      }
   }
}


// This method is called by the MACHS to update the current credit allocation
// values. After that, schedule the first packet.

void AM_HS::credit_update(vector < int >new_rlc_credits)
{



   vector < int >highestPriority;
   int        *highestNonEmptyQueue = new int;

   // Check in what PDU the poll bit should be set. It should be set in the
   // last packet that will be sent for each flow. This loop is constructed
   // different from the normal loops that loop through the flow-priority
   // combinations, because we now have to calculate things per flow, so this
   // flow needs to be in the outer-loop.
   for (int f_count = 0; f_count < flowMax_; f_count++) {
      *highestNonEmptyQueue = -1;   //-1 no full prio queue detected yet
      for (int p_count = 0; p_count < priorityMax_; p_count++) {
         if (new_rlc_credits.at(p_count * flowMax_ + f_count) > 0) {
            *highestNonEmptyQueue = p_count; //set highest to p_count.
         }
      }
      highestPriority.push_back(*highestNonEmptyQueue);
   }

   // For each buffer, transmit the number of PDUs defined in the credit
   // allocation vector.
   for (int p_count = 0; p_count < priorityMax_; p_count++) {
      for (int f_count = 0; f_count < flowMax_; f_count++) {
         for (int k = 1; k <= new_rlc_credits.at(p_count * flowMax_ + f_count);
              k++) {
            //Note: Buffers indexed: f1p1,f2p1,f3p1,f1p2,f2p2,f3p2,f1p3...

            Packet     *p;

            // AM-HS may not send packets that have a sequence number that
            // is more than the windowsize more than the sequence number of the
            // first packet that hasn't been acknowledged yet.
            if (flowInfo_.at(f_count)->FSN_ + win_ <
                flowInfo_.at(f_count)->seqno_) {
               // Ignore all packets but acknowledgements (the packets that
               // already have their sequence number set)
               p = flowInfo_.at(f_count)->transmissionBuffer_.at(p_count)->
                     dequeFirstSendable();
            } else {
               // The next sequence number will be inside the transmission
               // window, so we can just send the first packet.
               p = flowInfo_.at(f_count)->transmissionBuffer_.at(p_count)->
                     deque();
            }

            if (p != NULL) {

               // Check whether we should set the poll-bit in this PDU
               if (k == new_rlc_credits.at(p_count * flowMax_ + f_count)
                   && p_count == highestPriority.at(f_count)) {
                  flowInfo_.at(f_count)->set_poll_ = 1;
               }
               // Check whether the packet that has been chosen
               // has some space left that can be used for
               // biggybacking.
               completePDU(p);

               // store a copy for possible retransmissions
               flowInfo_.at(f_count)->retransmissionBuffer_.enque(p->copy());

               insertStatusInformation(p);

               // Actually send the packet down.
               downtarget_->recv(p);

               // Increase the number of PDUs sent. This is used for adding the
               // poll bit each poll_pdu_ packets.
               flowInfo_.at(f_count)->tx_PDUs_++;
            } else {
               // The number of packets available in the queue was smaller than
               // the maximum number of packets that we were allowed to send to
               // the Mac-hs. Or the RLC Window has blocked the packet from being sent.
	       // We therefore deallocate the previously allocated credit.
	       
	          mac_address->remove_credit_allocation(p_count * flowMax_ + f_count);
	                                    
               // break;
            }
         }
      }
   }
}


// This method is used for the receiver based 'Check for missing PDJU' status
// timer setting. See 9.7.2 of 3GPP TS 25.322 v5.6.0.

void AM_HS::checkForMissingPDU(Packet * opkt)
{



   int flowID = hdr_ip::access(opkt)->flowid();


   int first_missing = 0;

   if (flowInfo_.at(flowID)->prohibited_) {
      return;
   }
   // opkt is the "old" PDU that was received
   if (flowInfo_.at(flowID)->stprob_timer_.status() == TIMER_PENDING) {
      if (hdr_rlc::access(opkt)->poll()) {
         flowInfo_.at(flowID)->prohibited_ = 1;
      }
      return;
   }
   // find first missing PDU in seen_ from next_ expected PDU
   int i;

   // WARNING: possible endless loop
   for (i = flowInfo_.at(flowID)->next_; flowInfo_.at(flowID)->seen_[i & MWM]
        != 0; i++) {
      // do nothing
   }
   first_missing = i;

   if (first_missing < flowInfo_.at(flowID)->maxseen_
       || hdr_rlc::access(opkt)->poll()) {
      if (flowInfo_.at(flowID)->send_status_ == 1) {
         // Do nothing since a status report is already pending.
      } else {
         // Schedule a timeout for the status PDU to be sent. During the current
         // time and the timeout the status information can be piggybacked, then
         // this timer should be canceled.
         flowInfo_.at(flowID)->sinfo_timer_.sched(status_timeout_);
         flowInfo_.at(flowID)->send_status_ = 1;
      }
   }
}

Packet     *AM_HS::createBitmapAck(int flowID, Packet * p)
{



   hdr_rlc    *llh;
   hdr_cmn    *ch;

   int first_missing = flowInfo_.at(flowID)->next_;

   while (flowInfo_.at(flowID)->seen_[first_missing & MWM]) {
      ++first_missing;
   }


   int fsn = first_missing - 1;

   if (fsn < 0) {
      // When the first packet ever gets a nack we shouldn't try to ack
      // everything until then.
      fsn = 0;
   }

   if (p == NULL) {
      p = Packet::alloc();

      ch = hdr_cmn::access(p);

      ch->uid() = 0;
      ch->error() = 0;
      ch->timestamp() = Scheduler::instance().clock();
      ch->iface() = UNKN_IFACE.value();
      ch->direction() = hdr_cmn::DOWN;

      llh = hdr_rlc::access(p);



      hdr_ip     *iph = hdr_ip::access(p);
      hdr_tcp    *tcph = hdr_tcp::access(p);
      char       *mh = (char *) p->access(hdr_mac::offset_);

      llh->lltype() = RLC_BACK;
      llh->dst() = flowInfo_.at(flowID)->d_address_;
      llh->src() = address_;
      llh->seqno() = fsn;
      llh->lengthInd_ = 0;
      llh->padding_ = payloadSize_ - StatusPDUSize(flowID);


      for (int i = 1; i < 3; i++) {
         llh->payload_[i] = 0;
      }

      llh->payload_[0] = StatusPDUSize(flowID);

      iph->flowid() = flowID;   // set it to the current flowID
      iph->prio() = 0;          // set it to the highest priority

      iph->saddr() = -1;        // Because Bitmap acknowledgements are not passed
      iph->sport() = -1;        // up until the IP layer, these fields do not need
      iph->daddr() = -1;        // to be set.
      iph->dport() = -1;
      iph->ttl() = 32;

      tcph->seqno() = -1;

      ch->ptype() = PT_AMBA;
      ch->size() = payloadSize_;

      struct hdr_mac *dh = (struct hdr_mac *) mh;

      dh->macDA_ = flowInfo_.at(flowID)->macDA_;
      dh->macSA_ = -1;
      dh->hdr_type() = ETHERTYPE_RLC;

   } else {
      llh = hdr_rlc::access(p);
      ch = hdr_cmn::access(p);


      // Look for the first payload-field that is empty and insert the
      // status information there.
      if (llh->payload_[PAYLOAD_FLD2] == 0) {
         llh->payload_[PAYLOAD_FLD2] = StatusPDUSize(flowID);
      } else {
         llh->payload_[PAYLOAD_FLD3] = StatusPDUSize(flowID);
      }
      llh->padding_ = llh->padding_ - StatusPDUSize(flowID);
      llh->lltype() = RLC_PB_BACK;
      ch->ptype() = PT_AMPBBA;
   }

   llh->FSN() = fsn;
   if ((flowInfo_.at(flowID)->maxseen_ - fsn + 1) >
       (payloadSize_ * 8 - status_pdu_header_)) {
      llh->length() = payloadSize_ * 8 - status_pdu_header_;
   } else {
      llh->length() = flowInfo_.at(flowID)->maxseen_ - fsn + 1;
   }


   llh->bitmap_ = new int[llh->length()];

   // Basically, copy the seen_ array to the bitmap
   for (int i = 0; i < llh->length(); i++) {
      flowInfo_.at(flowID)->seen_[(fsn + i) & MWM]
            ? llh->bitmap(i) = 1 : llh->bitmap(i) = 0;
   }

   return p;
}


// This method does not comply to the Bitmap acknowledgement mode defined in
// 3GPP  TS 25.322 V5.6.0 (page 34).

void AM_HS::processBitmapAck(Packet * p)
{


   hdr_rlc    *llh = hdr_rlc::access(p);
   int flowID = hdr_ip::access(p)->flowid();

   flowInfo_.at(flowID)->b_bal_ = 0;
   flowInfo_.at(flowID)->length_ = llh->length();
   flowInfo_.at(flowID)->FSN_ = llh->FSN();


   flowInfo_.at(flowID)->retransmissionBuffer_.dropTill(flowInfo_.at(flowID)->
                                                        FSN_);


   // Start at the end of the bitmap and work your way through it. The reason
   // why we start at the end is because packets that are retransmitted should
   // have higher priority than packets that are transmitted for the first time.
   // Packets that are retransmitted are inserted at the front of the
   // transmission queue. When we start at the end of the bitmap, at the end
   // the new packets that need to be retransmitted are then inserted in
   // the correct order in the front of the queue. Otherwise, packets with
   // higher sequence numbers are transmitted earlier, causing new bitmap
   // acknowledgements to be send.

   for (int i = flowInfo_.at(flowID)->length_ - 1; i >= 0; i--) {
      if (llh->bitmap(i) == 0) {
         Packet     *r =
               flowInfo_.at(flowID)->retransmissionBuffer_.dequeCopy(flowInfo_.
                                                                     at
                                                                     (flowID)->
                                                                     FSN_ + i);
         flowInfo_.at(flowID)->b_bal_++;
         assert(r != NULL);
         enqueInFrontOfTransmissionBuffer(r);
      }
   }
   delete[]llh->bitmap_;

   flowInfo_.at(flowID)->highest_ack_ =
         flowInfo_.at(flowID)->FSN_ + flowInfo_.at(flowID)->length_ - 1;
   if (flowInfo_.at(flowID)->highest_ack_ >= flowInfo_.at(flowID)->poll_seq_) {
      flowInfo_.at(flowID)->poll_timer_.force_cancel();
      flowInfo_.at(flowID)->poll_seq_ = -1;
      flowInfo_.at(flowID)->set_poll_ = 0;
   }
}

// Returns the size of a Status PDU when it would have been constructed now.

int AM_HS::StatusPDUSize(int flowID)
{



   int first_missing = flowInfo_.at(flowID)->next_;

   while (flowInfo_.at(flowID)->seen_[first_missing & MWM]) {
      ++first_missing;
   }

   int fsn = first_missing - 1;
   int bits = flowInfo_.at(flowID)->maxseen_ - fsn + status_pdu_header_;

   int bytes;

   if (bits % 8) {
      bytes = bits / 8 + 1;
   } else {
      bytes = bits / 8;
   }
   return bytes;
}

// returns number of SDUs that can be "delivered" to the Agent
// also updates the receive window (i.e. next_, maxseen, and seen_ array)

int AM_HS::update(int seq, int eopno, int flowID)
{
   int numSDU = 0;
   bool just_marked_as_seen = FALSE;

   // start by assuming the segment hasn't been received before

   if (seq - flowInfo_.at(flowID)->next_ >= MWM) {
      // next_ is next PDU expected; MWM is the maximum
      // window size minus 1; if somehow the seqno of the
      // PDU is greater than the one we're expecting+MWM,
      // then ignore it.
      return -1;
   }

   if (seq > flowInfo_.at(flowID)->maxseen_) {
      // the PDU is the highest one we've seen so far
      int i;

      for (i = flowInfo_.at(flowID)->maxseen_ + 1; i < seq; ++i) {
         flowInfo_.at(flowID)->seen_[i & MWM] = 0;
      }
      // we record the PDUs between the old maximum and
      // the new max as being "unseen" i.e. 0
      flowInfo_.at(flowID)->maxseen_ = seq;
      if (seq == eopno) {
         flowInfo_.at(flowID)->seen_[flowInfo_.at(flowID)->maxseen_ & MWM] = 1;
      } else {
         flowInfo_.at(flowID)->seen_[flowInfo_.at(flowID)->maxseen_ & MWM] = 2;
      }
      // set this PDU as being "seen".
      flowInfo_.at(flowID)->seen_[(flowInfo_.at(flowID)->maxseen_ + 1) & MWM] =
            0;
      // clear the array entry for the PDU immediately
      // after this one
      just_marked_as_seen = TRUE;
      // necessary so this PDU isn't confused as being a duplicate
   }
   int next = flowInfo_.at(flowID)->next_;

   if (seq < next) {
      // Duplicate PDU case 1: the PDU is to the left edge of
      // the receive window; therefore we must have seen it
      // before
      return -1;
   }

   if (seq >= next && seq <= flowInfo_.at(flowID)->maxseen_) {
      // next is the left edge of the recv window; maxseen_
      // is the right edge; execute this block if there are
      // missing PDUs in the recv window AND if current
      // PDU falls within those gaps

      if (flowInfo_.at(flowID)->seen_[seq & MWM] && !just_marked_as_seen) {
         // Duplicate case 2: the segment has already been
         // recorded as being received (AND not because we just
         // marked it as such)
         return -1;
      }
      if (seq == eopno) {
         flowInfo_.at(flowID)->seen_[seq & MWM] = 1;
      } else {
         flowInfo_.at(flowID)->seen_[seq & MWM] = 2;
      }
      // record the PDU as being seen

      while (flowInfo_.at(flowID)->seen_[next & MWM]) {
         // This loop sees if any SDUs can now be deliver to
         // the Agent due to this PDU arriving
         next++;
         if ((flowInfo_.at(flowID)->seen_[(next - 1) & MWM] == 1)) {
            flowInfo_.at(flowID)->next_ = next;
            // store the new left edge of the window
            numSDU++;
         }
      }
   }
   return numSDU;
}


// Reassembles the received PDUs into an SDU and sends it up.

void AM_HS::makeSDU(int numSDU, int flowID)
{
   Packet     *p;
   hdr_cmn    *ch;
   hdr_rlc    *llh;

   for (int i = 0; i < numSDU; i++) {
      p = flowInfo_.at(flowID)->sduB_.deque();
      assert(p);
      ch = HDR_CMN(p);
      llh = hdr_rlc::access(p);

      ch->ptype() = llh->lptype();
      ch->error() = llh->lerror();
      ch->timestamp() = llh->lts();
      ch->size() = llh->lsize();

      uptarget_ ? sendUp(p) : Packet::free(p);
   }
}


