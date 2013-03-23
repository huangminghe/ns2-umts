/*
 * Copyright (c) 2003 Ericsson Telecommunicatie B.V.
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
 * 3. Neither the name of Ericsson Telecommunicatie B.V. may be used
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
 * $Id: um-hs.cc,v 1.2 2006/03/10 14:16:45 rouil Exp $
 */


// UM-HS is basically AM-HS with all the acknowledgements and retransmissions
// taken out.

#include "um-hs.h"
#include "mac.h"
#include "flags.h"
#include "random.h"
#include "address.h"
#include "hsdpalink.h"
#include "timer-handler.h"

// These variables are used as 'super-globals', in this way the UM-HS object
// and the MAC-hs object at the BS can be reached.

extern RLC_HS *rlc_address;     //= this;
extern HsdpaMac *mac_address;   //defined in hsdpalink.cc

static class UM_HSRlcClass:public TclClass {
public:
   UM_HSRlcClass():TclClass("UMTS/RLC/UMHS") {
   } TclObject *create(int, const char *const *) {
      return (new UM_HS);
   }
}

class_um_hs;

UM_HS::UM_HS():RLC_HS(), creditTimer_(this, RLC_TIMER_CREDIT)
{
   rlc_address = this;

   bind("macDA_", &macDA_);
   bind("win_", &win_);
   bind_time("temp_pdu_timeout_time_", &tempPDUTimeOutTime_);
   bind("credit_allocation_interval_", &creditAllocationInterval_);
   bind("flow_max_", &flowMax_);
   bind("priority_max_", &priorityMax_);
   bind("buffer_level_max_", &bufferLevelMax_);
   bind("payload_", &payloadSize_);
   bind_time("TTI_", &TTI_);
   bind("length_indicator_", &length_indicator_);
   bind("min_concat_data_", &min_concat_data_);

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
      um_flow_information *temp = new um_flow_information(this, i);

      for (int j = 0; j < priorityMax_; j++) {
         umtsQueue  *temp2 = new umtsQueue;

         temp->transmissionBuffer_.push_back(temp2);
      }
      flowInfo_.push_back(temp);
   }

   // schedule credit allocation timer for the first time
   creditTimer_.sched(creditAllocationTimeoutInterval_);

}


int UM_HS::command(int argc, const char *const *argv)
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

void UM_HS::recv(Packet * p, Handler * h)
{


   hdr_cmn    *ch = HDR_CMN(p);

   // If the direction is up, we don't have to change anything, and the packet
   // can be processed.
   if (ch->direction() == hdr_cmn::UP) {
      hdr_rlc    *llh = hdr_rlc::access(p);
      //NIST: removed warning for unused variable
      //int flowID = hdr_ip::access(p)->flowid();
      //end NIST

      if (llh->dst() != address_ || ch->error() > 0) {
         // The packet was meant for another UE, or it contained errors. In
         // both cases it will be dropped.
         Packet::free(p);
         return;
      }
      // Check the type op packets
      if (llh->lltype() == RLC_DATA) {
         // We only pass the last PDU of an SDU. The rest of the PDUs are not
         // needed for the reassembly and can be freed.

         int flowID = hdr_ip::access(p)->flowid();

         hdr_tcp    *tcph = hdr_tcp::access(p);
         hdr_rlc    *llh = hdr_rlc::access(p);
         int temp_sdu = tcph->seqno();
         int temp_seqno = llh->seqno();
         int temp_segment = llh->segment();


         if (temp_seqno == llh->eopno()) {
            // The packet is the last PDU of an SDU. Now, check whether the
            // complete SDU has been received. If so, pass the SDU up. In either
            // case, the next expected packet will be the first PDU of the next
            // SDU.
            if (flowInfo_.at(flowID)->nextExpectedSeqno_ == temp_seqno
                && flowInfo_.at(flowID)->nextExpectedSDU_ == temp_sdu
                && flowInfo_.at(flowID)->errorInSDU_ == 0) {
               makeSDU(p);
            } else {
               Packet::free(p);
            }
            flowInfo_.at(flowID)->errorInSDU_ = 0;
            flowInfo_.at(flowID)->nextExpectedSDU_ = temp_sdu + 1;
            flowInfo_.at(flowID)->nextExpectedSeqno_ = temp_seqno + 1;
         } else {
            // The packet is not the last PDU of an SDU. When the packet is the
            // expected packet, increase the next expected packet. If not, the
            // next packet is the next packet after this one. In that case,
            // set errorInSDU so that this current SDU will not be passed up,
            // unless this is the first segment of the SDU. In that case, the
            // missed PDU didn't belong to this SDU but to the previous one.
            if (flowInfo_.at(flowID)->nextExpectedSeqno_ == temp_seqno) {
               flowInfo_.at(flowID)->nextExpectedSeqno_++;
            } else {
               flowInfo_.at(flowID)->nextExpectedSDU_ = temp_sdu;
               flowInfo_.at(flowID)->nextExpectedSeqno_ = temp_seqno + 1;
               if (temp_segment != 0) {
                  flowInfo_.at(flowID)->errorInSDU_++;
               }
            }
            Packet::free(p);
         }

      } else {
         // In case the type of the packet is something else, just drop it.
         drop(p);               // Unknown type
      }
      return;
   } else {
      ch->direction() = hdr_cmn::DOWN;
      sendDown(p);
   }

}

int UM_HS::enqueInBackOfTransmissionBuffer(Packet * p)
{

   int r_value = 0;


   int flowID = hdr_ip::access(p)->flowid();
   int priority = hdr_ip::access(p)->prio();



   if ((flowInfo_.at(flowID)->transmissionBuffer_.at(priority)->length())
       >= bufferLevelMax_) {
      drop(p);
   } else {
      flowInfo_.at(flowID)->transmissionBuffer_.at(priority)->enque(p);
      r_value = 1;
   }


   return r_value;
}



//---------------------------
// sendDown processes a SDU into PDUs for transmission to MAC-HS
// if possible, waiting temporary packets are filled up.
//---------------------------

void UM_HS::sendDown(Packet * p)
{

   // First check whether the SDU fits into the transmission buffer completely

   int numPDUs =
         (int) ceil((double) hdr_cmn::access(p)->size() /
                    (double) payloadSize_);

   if (numPDUs + 1 +
       flowInfo_.at(hdr_ip::access(p)->flowid())->transmissionBuffer_.
       at(hdr_ip::access(p)->prio())->length()
       >= bufferLevelMax_) {
      Packet::free(p);
      return;
   }

   int segment = 0;

   // First check whether there is still a not-finished PDU available that can
   // be used for concatenation.
   handleTemporaryPacket(p);

   if (hdr_rlc::access(p)->segment() == 0) {
      // There was something that could be concatenated, and thus the first
      // segment of the packet was already sent, so continue with the next.
      segment++;
   }

   while (p != NULL) {
      // Each loop the front of the SDU is removed and send in a PDU. This
      // results in a smaller and smaller SDU, until at some point in time
      // it is completely gone. In that case p equals to NULL and the
      // complete SDU is segmented.

      int concat_data = 0;
      bool close_PDU = false;
      int padding = 0;
      bool force_length_indicator = false;
      Packet     *c;

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


      //initialise shortcuts to c's header info
      hdr_cmn    *c_ch = HDR_CMN(c);
      hdr_rlc    *c_llh = hdr_rlc::access(c);
      hdr_ip     *c_iph = hdr_ip::access(c);
      int flowID = c_iph->flowid();
      hdr_cmn    *p_ch = NULL;
      hdr_ip     *p_iph = NULL;

      if (p != NULL) {
         p_ch = HDR_CMN(p);
         p_iph = hdr_ip::access(p);
      }

      c_llh->lptype() = c_ch->ptype();
      c_llh->lerror_ = c_ch->error();
      c_llh->lts_ = c_ch->timestamp();
      c_llh->segment() = segment;
      segment++;

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
      c_llh->seqno() = flowInfo_.at(flowID)->seqno_;
      flowInfo_.at(flowID)->seqno_++;
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

      c_ch->ptype() = PT_UM;
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
      }

      assert(c_ch->error() == 0);


      c_ch->size() = payloadSize_;


      //----------
      if (enqueInBackOfTransmissionBuffer(c) == 0) {
      } else {
      }

   }


}

// This method is called when a PDU is created that is not full, and just stores
// the PDU into a vector that will be inspected

void UM_HS::StoreTemporaryPacket(Packet * p, int concat_data)
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


void UM_HS::handleTemporaryPacket(Packet * p)
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
            // this variable was previously set in makePDU
            flowInfo_.at(hdr_ip::access(p)->flowid())->SDU_size_ =
                  hdr_cmn::access(p)->size();
            hdr_cmn::access(p)->size() =
                  hdr_cmn::access(p)->size() -
                  temporaryPackets_.at(i)->concat_data;

            // Set the segemnt to 0, in this way it is known that segmentation
            // did occur.
            hdr_rlc::access(p)->segment() = 0;

         } else {

            hdr_rlc::access(temporaryPackets_.at(i)->p)->lengthInd_--;
            hdr_rlc::access(temporaryPackets_.at(i)->p)->padding_ =
                  temporaryPackets_.at(i)->concat_data + lengthIndicatorSize_;
         }
         enqueInBackOfTransmissionBuffer(temporaryPackets_.at(i)->p);

         // Now the temporary PDU has been sent, delete the timer
         // so that the timeout will not occur anymore.
         temporaryPackets_.at(i)->tempPDUTimer.cancel();
         temporaryPackets_.erase((temporaryPackets_.begin() + i));
         return;

      }
   }
}


void UM_HS::timeout(int tno, int flowID)
{
   vector < int >temp_vect;


   switch (tno) {
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
     default:

        break;
   }

}

void UM_HS::completePDU(Packet * p)
{


   hdr_rlc    *llh = hdr_rlc::access(p);
   int flowID = hdr_ip::access(p)->flowid();

   // If the packet is transmitted for the first time, set the sequence number
   // and possibly the eop number.
   if (llh->seqno() == -1) {
      llh->seqno() = flowInfo_.at(flowID)->seqno_;
      flowInfo_.at(flowID)->seqno_++;
      if (llh->eopno() == EOPNO_TO_SEQNO) {
         llh->eopno() = llh->seqno();
      }
   }
   return;
}

// This method is called by the MACHS to update the current credit allocation
// values. After that, schedule the first packet.

void UM_HS::credit_update(vector < int >new_rlc_credits)
{



   vector < int >highestPriority;
   int        *highestNonEmptyQueue = new int;

   // Check in what PDU the poll bit should be set. It should be set in the
   // last packet that will be sent for each flow.
   for (int i = 0; i < flowMax_; i++) {
      *highestNonEmptyQueue = 0;
      for (int j = 0; j < priorityMax_; j++) {
         if (new_rlc_credits.at(i * priorityMax_ + j) > 0) {
            *highestNonEmptyQueue = j;
         }
      }
      highestPriority.push_back(*highestNonEmptyQueue);
   }
   // For each buffer, transmit the number of PDUs defined in the credit
   // allocation vector.
   for (int i = 0; i < priorityMax_; i++) {
      for (int j = 0; j < flowMax_; j++) {
         for (int k = 0; k < new_rlc_credits.at(i * flowMax_ + j); k++) {
            //Note: Buffers indexed: f1p1,f2p1,f3p1,f1p2,f2p2,f3p2,f1p3...

            Packet     *p;


            // The next sequence number will be inside the transmission
            // window, so we can send the first packet.
            p = flowInfo_.at(j)->transmissionBuffer_.at(i)->deque();


            if (p != NULL) {


               // Check whether the packet that has been chosen
               // has some space left that can be used for
               // biggybacking.
               completePDU(p);

               // Actually send the packet down.
               downtarget_->recv(p);

               // Increase the number of PDUs sent. This is used for adding the
               // poll bit each poll_pdu_ packets.
            } else {
               // The number of packets available in the queue was smaller than
               // the maximum number of packets that we were allowed to send to
               // the Mac-hs.
               break;
            }
         }
      }
   }
}


void UM_HS::makeSDU(Packet * p)
{
   hdr_cmn    *ch;
   hdr_rlc    *llh;

   assert(p);
   ch = HDR_CMN(p);
   llh = hdr_rlc::access(p);

   ch->ptype() = llh->lptype();
   ch->error() = llh->lerror();
   ch->timestamp() = llh->lts();
   ch->size() = llh->lsize();

   uptarget_ ? sendUp(p) : Packet::free(p);
}


