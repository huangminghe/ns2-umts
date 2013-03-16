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
 * $Id: am-hs.h,v 1.1.1.1 2006/03/08 13:53:01 rouil Exp $
 */

#ifndef ns_am_hs_h
#define ns_am_hs_h

#include "rlc.h"
#include "umts-timers.h"
#include "umts-queue.h"
#include <vector>

#define MWS 4096             // max window size
#define MWM (MWS-1)          // The "win_" parameter, representing the RLC window,
                     // should be less than equal to MWM.

#define EOPNO_NOT_EOP -1
#define EOPNO_TO_SEQNO -2
// When PDUs are formed and stored into the transmission buffer, the sequence
// number is not yet known (for example, it is possible that data with higher
// priority, or status information is send before this PDU). The eopno field
// cannot be set to the seqno in this case at that moment, so in case the PDU
// is the last PDU in the SDU the eopno field is set to a temporary value
// (EOPNO_TO_SEQNO), which is replaced by the sequence number as soon as that
// sequence number is known.

#define PAYLOAD_FLD1 0
#define PAYLOAD_FLD2 1
#define PAYLOAD_FLD3 2



class AM_HS;

class temporaryPacket {
public:
   temporaryPacket(RLC * rlc):tempPDUTimer(rlc, RLC_TIMER_TEMP_PDU) {
   } Packet   *p;
   int         concat_data;
   UmtsTimer   tempPDUTimer;
};

class am_flow_information {
public:
   am_flow_information(AM_HS * amhs, int flowID):set_poll_(0), seqno_(0),
         send_status_(0), SDU_size_(0), poll_seq_(0), prohibited_(0), FSN_(-1),
         b_bal_(0), length_(0), maxseq_(-1), highest_ack_(0), maxseen_(0),
         next_(0), tx_PDUs_(0), d_address_(0), macDA_(0),
         poll_timer_((RLC *) amhs, RLC_TIMER_POLL, flowID),
         stprob_timer_((RLC *) amhs, RLC_TIMER_STPROB, flowID),
         sinfo_timer_((RLC *) amhs, RLC_TIMER_STATUS, flowID) {
   } umtsQueue retransmissionBuffer_;

   // We need a separate queue per flow-id.

   vector < umtsQueue * >transmissionBuffer_;
   // For our Transmission Buffer we use a vector of rlcQueue, for each
   // flow-id and priority-pair one queue exists.

   int         set_poll_;
   int         seqno_;

   int         send_status_;
   int         SDU_size_;       // stores the original size of the SDU when a part already

   // was concatenated


   int         poll_seq_;       // stores the highest sequence number of a packet including

   // a poll bit
   int         prohibited_;

   int         FSN_;            // First Sequence Number of the bitmap
   int         b_bal_;          // number of 0s in bitmap
   int         length_;         // length of bitmap

   int         maxseq_;         // highest seqno transmitted so far
   int         highest_ack_;    // highest ack recieved by sender
   int         maxseen_;        // max PDU (seqno)number seen by reciever
   int         seen_[MWS];      // array of PDUs seen by reciever
   int         next_;           // next PDU expected by receiver

   int         tx_PDUs_;        // Number of PDUs sent after the last poll-bit has been set

   int         d_address_;      // destination address of this RLC Entity
   int         macDA_;          // mac destination address for packets created by this RLC

   // Entity

   umtsQueue   sduB_;

   UmtsTimer   poll_timer_;
   UmtsTimer   stprob_timer_;
   UmtsTimer   sinfo_timer_;

   umtsQueue   rxtB_;
};

class AM_HS:public RLC_HS {
public:
   AM_HS();
   virtual void recv(Packet *, Handler *);
   int         command(int, const char *const *);
   void        credit_update(vector < int >new_rlc_credits);
   void        timeout(int tno, int flowID = -1);
protected:
   void        enqueInBackOfTransmissionBuffer(Packet * p);
   int         enqueInFrontOfTransmissionBuffer(Packet * p);
   void        sendDown(Packet * p);
   void        StoreTemporaryPacket(Packet * p, int concat_data);
   void        handleTemporaryPacket(Packet * p);
   void        completePDU(Packet * p);
   void        insertStatusInformation(Packet * p);
   void        checkForMissingPDU(Packet * opkt);
   Packet     *createBitmapAck(int flowID, Packet * p = NULL);
   void        processBitmapAck(Packet * p);
   int         StatusPDUSize(int flowID);
   int         update(int, int, int flowID);
   void        makeSDU(int numSDU, int flowID);

               vector < am_flow_information * >flowInfo_;
   // For each flow information has to be kept. All this information is kept in
   // this struct.

   int         lengthIndicatorSize_;
   // The size of the Length indicator + the
   // extention bit. The size is in bytes. The
   // size is 1 or 2 bytes depending on the size
   // of the PDUs.

   int         win_;
   // defines the RLC Window Size, which is the amount of packets that can be
   // sent after the last acknowledged packet.

   int         flowMax_;
   // The maximum number of flows

   int         priorityMax_;
   // The maximum number of priorities

   int         bufferLevelMax_;
   // The maximum number of PDUs for each flow-id and
   // priority pair in the Transmission Buffer.

   int         creditAllocationInterval_;
   // The number of TTIs between a new
   // credit allocation.

   double      tempPDUTimeOutTime_;
   // When a PDU is constructed that has some
   // space left, the PDU is stored in the vector
   // temporaryPackets_. A timeout will be set
   // to ensure that when no further SDU arrives
   // with the same flow-id and priority, the
   // not-full PDU will be sent and will not
   // wait indefinitely.

               vector < temporaryPacket * >temporaryPackets_;
   // In this vector Temporary PDUs are stored. These are PDUs that a not
   // completely full, and they can be possibly concatenated with a part of
   // a new SDU. However, this concatenation should be done quick enough,
   // otherwise timeouts and unnesessary delays may occur. So, a timer will
   // be set, when the concatenation can be done before the timer times out,
   // the PDU will be concatenated with a part of a new SDU. When the timer
   // times out, the Temporary Packet is padded and sent without concatenation.

   UmtsTimer   creditTimer_;
   // This timer is used for the timout once every
   // creditAllocationTimeoutInterval_. Then the RLC will start a new credit
   // allocation.

   double      creditAllocationTimeoutInterval_;
   // = TTI_ * credit_allocation_interval_

   int         poll_PDU_;       // number of PDUs before a bitmap poll should be sent
   double      overhead_;       // Time that is needed to contruct SDUs
   double      poll_timeout_;   // rtt value to be used for timeout to recv status PDU
   double      stprob_timeout_; // value for the status prohibit timer
   double      status_timeout_;
   // Value for the length the status information should be postponed and
   // tried to be piggybacked before they are forced to be send.

   int         payloadSize_;    // user data per DATA PDU
   double      TTI_;

   int         ack_pdu_header_;
   int         status_pdu_header_;
   int         length_indicator_;
   int         min_concat_data_;

   int         address_;        // address of this RLC Entity

};

#endif
