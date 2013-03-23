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
 * $Id: am.cc,v 1.1.1.1 2006/03/08 13:53:01 rouil Exp $
 */


#include "am.h"
#include "mac.h"
#include "flags.h"
#include "random.h"
#include "address.h"
#include <iostream>

//int hdr_rlc::offset_;

// *INDENT-OFF*
static class AMRlcClass:public TclClass {
 public:
   AMRlcClass():TclClass("UMTS/RLC/AM") {
   }
   TclObject *create(int, const char *const *) {
      return (new AM);
   }
} class_am;
// *INDENT-ON*

AM::AM():RLC(), sent_TTI_PDUs_(0), TTI_PDUs_(0), set_poll_(0), send_ack_(0),
send_status_(0), SDU_size_(-1), dupacks_(0), rtt_seq_(-1), poll_seq_(-1),
rtt_active_(0), prohibited_(0), FSN_(-1), b_bal_(0), bitmap_(0), maxseq_(-1),
t_seqno_(0), highest_ack_(-1), maxseen_(-1), next_(0), tx_PDUs_(0), tx_SDUs_(0),
rx_PDUs_(0), rx_SDUs_(0), TTI_time_(-1), rtx_timer_(this, RLC_TIMER_RTX),
poll_timer_((RLC *) this, RLC_TIMER_POLL), delsnd_timer_(this,
                                                         RLC_TIMER_DELSND),
stprob_timer_(this, RLC_TIMER_STPROB), tti_timer_(this, RLC_TIMER_TTI)
{
   bind("win_", &win_);
   bind_bw("maxRBSize_", &maxRBSize_);
   bind("ack_mode_", &ack_mode_);
   bind("poll_PDU_", &poll_PDU_);
   bind_time("rtx_timeout_", &rtx_timeout_);
   bind_time("poll_timeout_", &poll_timeout_);
   bind_time("overhead_", &overhead_);
   bind_time("stprob_timeout_", &stprob_timeout_);
   bind("noFastRetrans_", &noFastRetrans_);
   bind("numdupacks_", &numdupacks_);
   bind("payload_", &payload_);
   bind_bw("bandwidth_", &bandwidth_);
   bind_time("TTI_", &TTI_);
   bind("length_indicator_", &length_indicator_);
   bind("ack_pdu_header_", &ack_pdu_header_);
   bind("status_pdu_header_", &status_pdu_header_);
   bind("min_concat_data_", &min_concat_data_);
   bind_time("max_status_delay_", &max_status_delay_);
   bind_time("max_ack_delay_", &max_ack_delay_);
   bind("macDA_", &macDA_);
   memset(seen_, 0, sizeof(seen_));
   seqno_ = -1;

}

int AM::command(int argc, const char *const *argv)
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
         tti_timer_.sched(TTI_ - 0.001);
         TTI_PDUs_ = (int) (bandwidth_ * TTI_) / (payload_ * 8);
         return TCL_OK;
      }
   } else if (argc == 3) {
      if (strcmp(argv[1], "macDA") == 0) {
         macDA_ = atoi(argv[2]);
         return (TCL_OK);
      } else if (strcmp(argv[1], "addr") == 0) {
         address_ = Address::instance().str2addr(argv[2]);
         return (TCL_OK);
      } else if (strcmp(argv[1], "daddr") == 0) {
         d_address_ = Address::instance().str2addr(argv[2]);
         return (TCL_OK);
      }
   }
   return LL::command(argc, argv);
}

void AM::recv(Packet * p, Handler * h)
{

   hdr_cmn    *ch = HDR_CMN(p);


   /* If direction = UP, then pass it up the stack
    * Otherwise, set direction to DOWN and pass it down the stack */

   if (ch->direction() == hdr_cmn::UP) {

      hdr_rlc    *llh = hdr_rlc::access(p);


      if (llh->dst() != address_ || ch->error() > 0) {
         Packet::free(p);
         /* PDUs with errors, or those meant for another UE */
         return;                /* should have been dropped before this. */
      }

      assert(chk_size(p));
      flowID_ = hdr_ip::access(p)->flowid();

      if (llh->lltype() == RLC_ACK) {
         if (llh->a_seqno() > highest_ack_) {
            newack(p);
         } else if (llh->a_seqno() == highest_ack_) {
            if (++dupacks_ == numdupacks_ && !noFastRetrans_) {
               reset_rtx_timer();
            }
         }
         Packet::free(p);
      } else if (llh->lltype() == RLC_BACK) {
         newback(p);
         Packet::free(p);
      } else if (llh->lltype() == RLC_DATA || llh->lltype() == RLC_PB_ACK
                 || llh->lltype() == RLC_PB_BACK) {
         if (llh->lltype() == RLC_PB_ACK) {
            if (llh->a_seqno() > highest_ack_) {
               newack(p);
            } else if (llh->a_seqno() == highest_ack_) {
               if (++dupacks_ == numdupacks_ && !noFastRetrans_) {
                  reset_rtx_timer();
               }
            }
         } else if (llh->lltype() == RLC_PB_BACK) {
            newback(p);
         }

         /* Figure out if any SDUs can be handed up. */
         int numSDU = update(llh->seqno(), llh->eopno());


         ack(p);
         if (llh->seqno() == llh->eopno() && numSDU >= 0) {
            sduB_.orderedEnque(p);
         } else {
            Packet::free(p);
         }
         /* "make" and send up the following SDUs */
         if (numSDU > 0) {
            makeSDU(numSDU);
         }
      } else {
         drop(p);               /* Unknown type */
      }
      return;
   }
   ch->direction() = hdr_cmn::DOWN;
   sendDown(p);
}


bool AM::chk_size(Packet * p)
{
   hdr_rlc    *llh = hdr_rlc::access(p);


   int size = 0;

   size += llh->lengthInd_;
   size += llh->payload_[0];
   size += llh->payload_[1];
   size += llh->payload_[2];
   size += llh->padding_;

   return size == payload_;
//    if (llh->lengthInd_ == 0) {
//       size = llh->payload_[0];
//    } else {
//       for (int i = 0; i < llh->lengthInd_; i++) {
//          size = size + ((length_indicator_ + 1) / 8) + llh->payload_[i];
//       }
//    }
//    if ((size + llh->padding_) == payload_) {
//       return true;
//    } else {
//       return false;
//    }
}



void AM::sendDown(Packet * p)
{
   if ((rcvB_.size() + hdr_cmn::access(p)->size()) <= maxRBSize_) {
      rcvB_.enque(p);
   } else {
      /* No space in recv buffer for this SDU */
      drop(p);
   }
}

Packet     *AM::makePDU(int PB_bytes)
{
   hdr_cmn    *ch;
   hdr_ip     *iph;
   hdr_tcp    *tcph;
   hdr_rlc    *llh;
   char       *mh;


   int available = payload_;
   int concat_data = 0;
   int end_PDU = 0;
   int padding = 0;
   int force_length_indicator = 0;

   Packet     *c;
   Packet     *p = rcvB_.dequeCopy();


   if (PB_bytes != 0) {
      available = payload_ - ((length_indicator_ + 1) / 8) - PB_bytes;
      if (hdr_cmn::access(p)->size() <= available) {
         end_PDU = 1;
         concat_data = available - hdr_cmn::access(p)->size()
               - ((length_indicator_ + 1) / 8);
         padding = available - hdr_cmn::access(p)->size();
         /* This amount of padding IF we dont concat. */
      }
   } else if (hdr_cmn::access(p)->size() < available) {
      if (hdr_cmn::access(p)->size() <=
          available - ((length_indicator_ + 1) / 8)) {
         end_PDU = 1;
         concat_data = available - hdr_cmn::access(p)->size()
               - (2 * ((length_indicator_ + 1) / 8));
         padding = available - hdr_cmn::access(p)->size()
               - ((length_indicator_ + 1) / 8);
      }
      force_length_indicator = 1;
   } else if (hdr_cmn::access(p)->size() == available) {
      end_PDU = 1;
   }
   if (concat_data < min_concat_data_ || (rcvB_.size(2) <= concat_data)) {
      concat_data = 0;
   } else {
      padding = 0;
   }
   if (end_PDU) {
      Packet     *temp = rcvB_.deque();

      Packet::free(temp);

      c = p;
      ch = HDR_CMN(c);
      llh = hdr_rlc::access(c);

      llh->lptype() = ch->ptype();
      llh->lerror_ = ch->error();
      llh->lts_ = ch->timestamp();

      if (SDU_size_ > 0) {
         llh->lsize_ = SDU_size_;
      } else {
         llh->lsize_ = ch->size();
      }

      SDU_size_ = -1;

      llh->lltype() = RLC_DATA;
      llh->seqno_ = ++seqno_;
      llh->eopno_ = llh->seqno_;
      llh->dst() = d_address_;
      llh->src() = address_;
      llh->lengthInd_ = 0;

      if (PB_bytes || concat_data || padding || force_length_indicator) {
         llh->lengthInd_++;
      }

      for (int i = 1; i < 3; i++) {
         llh->payload_[i] = 0;
      }

      llh->payload_[0] = hdr_cmn::access(c)->size();

      if (concat_data) {
         llh->lengthInd_++;
         llh->payload_[1] = concat_data;

         if (PB_bytes != 0) {
            llh->padding_ = PB_bytes;
         } else {
            llh->padding_ = 0;
         }
         SDU_size_ = rcvB_.red_size(concat_data);
      } else if (PB_bytes != 0) {
         llh->padding_ = PB_bytes + padding;
      } else {
         llh->padding_ = padding;
      }
      ch->ptype() = PT_AMDA;
      ch->error() = 0;
      ch->timestamp() = Scheduler::instance().clock();
      ch->size() = payload_;

      mh = (char *) c->access(hdr_mac::offset_);
      struct hdr_mac *dh = (struct hdr_mac *) mh;

      dh->macDA_ = macDA_;
      dh->macSA_ = -1;
      dh->hdr_type() = ETHERTYPE_RLC;

   } else {
      if (SDU_size_ < 0) {
         SDU_size_ = hdr_cmn::access(p)->size();
      }
      c = allocpkt(hdr_cmn::access(p)->uid());
      ch = HDR_CMN(c);
      iph = hdr_ip::access(c);
      tcph = hdr_tcp::access(c);
      llh = hdr_rlc::access(c);
      hdr_ip     *piph = hdr_ip::access(p);
      hdr_tcp    *ptcph = hdr_tcp::access(p);
      hdr_flags  *phf = hdr_flags::access(p);

      llh->lltype() = RLC_DATA;
      llh->seqno_ = ++seqno_;
      llh->eopno_ = -1;
      llh->dst() = d_address_;
      llh->src() = address_;
      llh->padding_ = 0;
      llh->lengthInd_ = 0;

      if (PB_bytes || force_length_indicator) {
         llh->lengthInd_++;
      }

      for (int i = 1; i < 3; i++) {
         llh->payload_[i] = 0;
      }
      if (force_length_indicator) {
         llh->payload_[0] = payload_ - ((length_indicator_ + 1) / 8);
      } else if (PB_bytes != 0) {
         llh->payload_[0] = payload_ - ((length_indicator_ + 1) / 8) - PB_bytes;
         llh->padding_ = PB_bytes;
      } else {
         llh->payload_[0] = payload_;
      }

      rcvB_.red_size(llh->payload_[0]);

      ch->ptype() = PT_AMDA;
      ch->size() = payload_;

      iph->flowid() = piph->flowid();
      iph->saddr() = piph->saddr();
      iph->sport() = piph->sport();
      iph->daddr() = piph->daddr();
      iph->dport() = piph->dport();
      iph->ttl() = piph->ttl();

      tcph->seqno() = ptcph->seqno();

      hdr_flags  *hf = hdr_flags::access(c);

      hf->ecn_ = phf->ecn_;
      hf->cong_action_ = phf->cong_action_;
      hf->ecn_to_echo_ = phf->ecn_to_echo_;
      hf->fs_ = phf->fs_;
      hf->ecn_capable_ = phf->ecn_capable_;

      mh = (char *) c->access(hdr_mac::offset_);
      struct hdr_mac *dh = (struct hdr_mac *) mh;

      dh->macDA_ = macDA_;
      dh->macSA_ = -1;
      dh->hdr_type() = ETHERTYPE_RLC;

      //NIST: add free call to reduce memory leaks.
      Packet::free(p);
      //end NIST
   }
   return c;
}


int AM::PB_S_PDU()
{
   int position = 0;

   if ((payload_ - StatusPDUSize() - (2 * ((length_indicator_ + 1) / 8)))
       < min_concat_data_) {
      return 0;                 /* Status PDU is too large to piggy back with */
      /* enough data to meet minimum criteria. */
   }
   if (t_seqno_ > highest_ack_ + win_ - b_bal_) {
      return 0;                 /* Cant send Data PDUs because window is full. */
      /* Send status with padding immediately. */
   }
   
   //Neill: checking indexing

   for (int i = 1; i <= b_bal_; i++) {
   //for (int i = 0; i < b_bal_; i++) {
      Packet     *p = rxtB_.dequeCopy(bRxtSeq(i));

      if (S_Piggybackable(p)) {
         Packet::free(p);
         return ++position;
      }
      position++;
      Packet::free(p);
   }

   if (rcvB_.size() != 0) {
      return ++position;
   }
   return 0;
}


int AM::PB_PA_PDU()
{
   int position = 0;

   if (payload_ - AckPDUSize() - (2 * ((length_indicator_ + 1) / 8)) <
       min_concat_data_) {
      return 0;
   } else if (t_seqno_ > highest_ack_ + win_) {
      return 0;
   } else if (t_seqno_ <= maxseq_) {
      for (int i = t_seqno_; i <= maxseq_; i++) {
         Packet     *p = rxtB_.dequeCopy(i);

         if (PA_Piggybackable(p)) {
            Packet::free(p);
            return ++position;
         }
         position++;
         Packet::free(p);
      }
   }

   if (rcvB_.size() != 0) {
      return ++position;
   }
   return 0;
}


bool AM::PA_Piggybackable(Packet * p)
{
   if (hdr_rlc::access(p)->padding()) {
      if (hdr_rlc::access(p)->padding() >=
          (AckPDUSize() + ((length_indicator_ + 1) / 8))) {
         return true;
      }
   }
   return false;
}


int AM::AckPDUSize()
{
   int bytes;

   if (ack_pdu_header_ % 8) {
      bytes = ack_pdu_header_ / 8 + 1;
   } else {
      bytes = ack_pdu_header_ / 8;
   }
   return bytes;
}


bool AM::S_Piggybackable(Packet * p)
{
   if (hdr_rlc::access(p)->padding()) {
      if (hdr_rlc::access(p)->padding() >=
          (StatusPDUSize() + ((length_indicator_ + 1) / 8))) {
         return true;
      }
   }
   return false;
}


int AM::StatusPDUSize()
{
   int first_missing = next_;

   while (seen_[first_missing & MWM]) {
      ++first_missing;
   }

   int fsn = first_missing - 1;
   int bits = maxseen_ - fsn + status_pdu_header_;

   int bytes;

   if (bits % 8) {
      bytes = bits / 8 + 1;
   } else {
      bytes = bits / 8;
   }
   return bytes;
}


double AM::send_time(int position)
{
   int PDUs_left_in_current_TTI = TTI_PDUs_ - sent_TTI_PDUs_;
   int i = 0;

   while (1) {
      if (position <= (i * TTI_PDUs_ + PDUs_left_in_current_TTI)) {
         return (tti_timer_.timeOfExpiry() + TTI_ * i);
      }
      i++;
   }
}


/*
 *
 */
void AM::send_much(int force)
{
   Packet     *p = NULL;

   if (!force && delsnd_timer_.status() == TIMER_PENDING) {
      return;
   }

   while (((t_seqno_ <= highest_ack_ + win_ - b_bal_)
           || set_poll_ || send_status_ || send_ack_)) {

      if ((rcvB_.size() == 0) && (b_bal_ == 0)
          && (!set_poll_) && (!send_status_)
          && (!send_ack_) && (t_seqno_ > maxseq_)) {
         sent_TTI_PDUs_ = 0;
         return;
         /* There are no new PDUs to send or retransmitt */
      } else if (sent_TTI_PDUs_ == TTI_PDUs_) {
         sent_TTI_PDUs_ = 0;
         return;
      }

      if (overhead_ == 0 || force) {
         if (send_status_) {
            int position = PB_S_PDU();

            if (position && ((send_time(position) - earliest_status_send_)
                             <= max_status_delay_)) {
               /* DO nothing. The Status report 
                * will be piggybacked later. */
            } else {
               Packet     *pkt = make_bitmap_ack(NULL);

               sent_TTI_PDUs_++;
               send_status_ = 0;
               set_status_prohibit_timer();
               downtarget_->recv(pkt);
               delsnd_timer_.resched(Random::uniform(overhead_));
               return;
            }
         } else if (send_ack_) {
            int position = PB_PA_PDU();

            if (position && ((send_time(position) - earliest_ack_send_)
                             <= max_ack_delay_)) {
               /* DO nothing. The ack will be piggybacked later. */
            } else {
               Packet     *pkt = make_positive_ack(NULL);

               sent_TTI_PDUs_++;
               send_ack_ = 0;
               downtarget_->recv(pkt);
               delsnd_timer_.resched(Random::uniform(overhead_));
               return;
            }
         }

         int force_set_rtx_timer = 0;

         if (set_poll_ && (rcvB_.size() == 0) && (b_bal_ == 0)) {
            p = rxtB_.dequeTailCopy();
            if (send_status_ && S_Piggybackable(p)) {
               make_bitmap_ack(p);
               set_status_prohibit_timer();
               send_status_ = 0;
            }
         } else if (b_bal_ != 0) {
            int temp_val = bRxtSeq(0);

            
            p = rxtB_.dequeCopy(temp_val); //side-effect: b_bal_--
            if (send_status_ && S_Piggybackable(p)) {
               make_bitmap_ack(p);
               set_status_prohibit_timer();
               send_status_ = 0;
            }
            // for last PDU to be re-transmitted be sure 
            // to trigger polling function
            if (b_bal_ == 0) {
               set_poll_ = 1;
            }
         } else if (t_seqno_ > maxseq_) {
            // for bitmap this is always true
            if (send_status_) {
               p = makePDU(StatusPDUSize()
                           + ((length_indicator_ + 1) / 8));
            } else if (send_ack_) {
               p = makePDU(AckPDUSize()
                           + ((length_indicator_ + 1) / 8));
            } else {
               p = makePDU(0);
            }

            rxtB_.enque(p->copy());

            if (send_status_) {
               make_bitmap_ack(p);
               set_status_prohibit_timer();
               send_status_ = 0;
            } else if (send_ack_) {
               make_positive_ack(p);
               send_ack_ = 0;
            }

            assert(t_seqno_ == hdr_rlc::access(p)->seqno());

            // for last PDU in rcv buffer, 
            // be sure to trigger polling function
            if (rcvB_.size() == 0) {
               set_poll_ = 1;
            }
            if (highest_ack_ == maxseq_) {
               force_set_rtx_timer = 1;
            }
            maxseq_ = t_seqno_;
            t_seqno_++;
         } else {
            p = rxtB_.dequeCopy(t_seqno_);
            if (send_ack_ && PA_Piggybackable(p)) {
               make_positive_ack(p);
               send_ack_ = 0;
            }
            t_seqno_++;
         }

          assert(p);
         ++tx_PDUs_;

         if (ack_mode_ == 1) {
            set_poll_ = 0;
            if (!rtt_active_) {
               rtt_active_ = 1;
               if (t_seqno_ > rtt_seq_) {
                  rtt_seq_ = t_seqno_;
               }
            }
            if (!(rtx_timer_.status() == TIMER_PENDING)
                || force_set_rtx_timer) {
               set_rtx_timer();
               /* No timer pending.  Schedule one. */
            }
         } else if (ack_mode_ == 2) {
            if (!(tx_PDUs_ % poll_PDU_) || set_poll_) {
               hdr_rlc::access(p)->poll() = true;
               if ((hdr_rlc::access(p)->seqno() >= poll_seq_)
                   || (poll_timer_.status() != TIMER_PENDING)) {
                  poll_seq_ = hdr_rlc::access(p)->seqno();
                  set_poll_timer();
               }
               tx_PDUs_ = 0;
            }
         }
         set_poll_ = 0;

         sent_TTI_PDUs_++;
         downtarget_->recv(p);
         delsnd_timer_.resched(Random::uniform(overhead_));
         return;
      } else if (!(delsnd_timer_.status() == TIMER_PENDING)) {
         /*
          * Set a delayed send timeout.
          */
         delsnd_timer_.resched(Random::uniform(overhead_));
         return;
      }

   }
   sent_TTI_PDUs_ = 0;
}


/*
 * returns number of SDUs that can be "delivered" to the Agent
 * also updates the receive window (i.e. next_, maxseen, and seen_ array)
 */
int AM::update(int seq, int eopno)
{
   int numSDU = 0;
   bool just_marked_as_seen = FALSE;

   // start by assuming the segment hasn't been received before

   if (seq - next_ >= MWM) {
      // next_ is next PDU expected; MWM is the maximum
      // window size minus 1; if somehow the seqno of the
      // PDU is greater than the one we're expecting+MWM,
      // then ignore it.
      return -1;
   }

   if (seq > maxseen_) {
      // the PDU is the highest one we've seen so far
      int i;

      for (i = maxseen_ + 1; i < seq; ++i) {
         seen_[i & MWM] = 0;
      }
      // we record the PDUs between the old maximum and
      // the new max as being "unseen" i.e. 0
      maxseen_ = seq;
      if (seq == eopno) {
         seen_[maxseen_ & MWM] = 1;
      } else {
         seen_[maxseen_ & MWM] = 2;
      }
      // set this PDU as being "seen".
      seen_[(maxseen_ + 1) & MWM] = 0;
      // clear the array entry for the PDU immediately
      // after this one
      just_marked_as_seen = TRUE;
      // necessary so this PDU isn't confused as being a duplicate
   }
   int next = next_;

   if (seq < next) {
      // Duplicate PDU case 1: the PDU is to the left edge of
      // the receive window; therefore we must have seen it
      // before
      return -1;
   }

   if (seq >= next && seq <= maxseen_) {
      // next is the left edge of the recv window; maxseen_
      // is the right edge; execute this block if there are
      // missing PDUs in the recv window AND if current
      // PDU falls within those gaps

      if (seen_[seq & MWM] && !just_marked_as_seen) {
         // Duplicate case 2: the segment has already been
         // recorded as being received (AND not because we just
         // marked it as such)
         return -1;
      }
      if (seq == eopno) {
         seen_[seq & MWM] = 1;
      } else {
         seen_[seq & MWM] = 2;
      }
      // record the PDU as being seen

      while (seen_[next & MWM]) {
         // This loop sees if any SDUs can now be deliver to
         // the Agent due to this PDU arriving
         next++;
         if ((seen_[(next - 1) & MWM] == 1)) {
            next_ = next;
            // store the new left edge of the window
            numSDU++;
         }
      }
   }
   return numSDU;
}


/*
 * Process an Positive ACK of previously unacknowleged data.
 */
void AM::newack(Packet * pkt)
{
   hdr_rlc    *llh = hdr_rlc::access(pkt);

   dupacks_ = 0;
   highest_ack_ = llh->a_seqno();

   /* delete all the Acked PDUs from RXT Buff i.e. PDUs till highest_ack_ */
   rxtB_.dropTill(highest_ack_);

   if (t_seqno_ < highest_ack_ + 1) {
      t_seqno_ = highest_ack_ + 1;
   }
   if (rtt_active_ && llh->a_seqno() >= rtt_seq_) {
      rtt_active_ = 0;
   }
   /* Set new retransmission timer if not all outstanding data acked.
    * Otherwise, if a timer is still outstanding, cancel it. */
   if (llh->a_seqno() < maxseq_) {
      set_rtx_timer();
   } else {
      cancel_rtx_timer();
   }
}


/*
 * Process a Bitmap ACK.
 */
void AM::newback(Packet * pkt)
{
   hdr_rlc    *llh = hdr_rlc::access(pkt);

   b_bal_ = 0;
   length_ = llh->length();
   FSN_ = llh->FSN();


   rxtB_.dropTill(FSN_);

   if (bitmap_) {
      delete[]bitmap_;
   }
   bitmap_ = new int[length_];

   for (int i = 0; i < length_; i++) {
      bitmap_[i] = llh->bitmap(i);
      if (bitmap_[i] == 0) {
         b_bal_++;
      } else {
         Packet     *r = rxtB_.deque(FSN_ + i);

         if (r) {
            Packet::free(r);
         }
      }
   }
   delete[]llh->bitmap_;

   highest_ack_ = FSN_ + length_ - 1;
   assert(t_seqno_ > highest_ack_);
   if (highest_ack_ >= poll_seq_) {
      cancel_poll_timer();
      poll_seq_ = -1;
      set_poll_ = 0;
   }
}

int AM::bRxtSeq(int position)
{



   int counter = 0;

   if (position == 0) {
      b_bal_--;
   }
   for (int i = 0; i < length_; i++) {
      if (bitmap_[i] == 0) {
         counter++;
         if (position == 0) {
            bitmap_[i] = 1;
            // Neill(16/07/04): Increment of 1 seems to be wrong here
            // return (i + FSN_ + 1);
            return (i + FSN_);
         } else if (counter == position) {
           // return (i + FSN_ + 1);
            return (i + FSN_);
         }
      }
   }
   return -1;
}


void AM::ack(Packet * opkt)
{
   int first_missing = next_;

   while (seen_[first_missing & MWM]) {
      ++first_missing;
   }
   if (ack_mode_ == 1) {
      /* Do you want to set a timer so you dont send an ack every PDU? */
      if (send_ack_ == 1) {
         /* Do nothing since an ack is already pending. */
      } else {
         if (delsnd_timer_.status() == TIMER_PENDING
             && sent_TTI_PDUs_ < TTI_PDUs_) {
            earliest_ack_send_ = tti_timer_.timeOfExpiry();
         } else {
            earliest_ack_send_ = tti_timer_.timeOfExpiry() + TTI_;
         }
         send_ack_ = 1;
      }
   } else if (ack_mode_ == 2) {
      if (prohibited_) {
         return;
      }
      /* opkt is the "old" PDU that was received */

      if (stprob_timer_.status() == TIMER_PENDING) {
         if (hdr_rlc::access(opkt)->poll()) {
            prohibited_ = 1;
         }
         return;
      }

      if (first_missing < maxseen_ || hdr_rlc::access(opkt)->poll()) {
         if (send_status_ == 1) {
            /* Do nothing since a status report is already pending. */
         } else {
            if (delsnd_timer_.status()
                == TIMER_PENDING && sent_TTI_PDUs_ < TTI_PDUs_) {
               earliest_status_send_ = tti_timer_.timeOfExpiry();
            } else {
               earliest_status_send_ = tti_timer_.timeOfExpiry() + TTI_;
            }
            send_status_ = 1;
         }
      }
   }
}


Packet     *AM::make_positive_ack(Packet * p)
{
   Packet     *npkt;
   hdr_rlc    *nrlc;
   hdr_cmn    *nch;

   int first_missing = next_;

   while (seen_[first_missing & MWM]) {
      ++first_missing;
   }

   if (p == NULL) {
      npkt = allocpkt(0);
      nrlc = hdr_rlc::access(npkt);
      nch = hdr_cmn::access(npkt);
      hdr_ip     *niph = hdr_ip::access(npkt);
      hdr_tcp    *ntcp = hdr_tcp::access(npkt);

      nrlc->lltype() = RLC_ACK;
      nrlc->dst() = d_address_;
      nrlc->src() = address_;
      nrlc->a_seqno() = first_missing - 1;   /* cumulative sequence number */
      nrlc->seqno() = nrlc->a_seqno(); /* For trace plotting. */
      nrlc->lengthInd_ = 0;
      nrlc->padding_ = payload_ - AckPDUSize();

      for (int i = 1; i < 3; i++) {
         nrlc->payload_[i] = 0;
      }

      nrlc->payload_[0] = AckPDUSize();


      nch->ptype() = PT_AMPA;
      nch->size() = payload_;

      niph->flowid() = -1;
      niph->saddr() = -1;
      niph->sport() = -1;
      niph->daddr() = -1;
      niph->dport() = -1;
      niph->ttl() = 32;

      ntcp->seqno() = -1;

      char       *mh = (char *) npkt->access(hdr_mac::offset_);
      struct hdr_mac *dh = (struct hdr_mac *) mh;

      dh->macDA_ = macDA_;
      dh->macSA_ = -1;
      dh->hdr_type() = ETHERTYPE_RLC;

      return npkt;
   }
   npkt = p;

   nrlc = hdr_rlc::access(npkt);
   nch = hdr_cmn::access(npkt);

   nrlc->lengthInd_++;
   nrlc->padding_ = nrlc->padding_ - AckPDUSize()
         - ((length_indicator_ + 1) / 8);
   nrlc->payload_[nrlc->lengthInd_ - 1] = AckPDUSize();

   nrlc->a_seqno() = first_missing - 1;   /* cumulative sequence number */
   nrlc->lltype() = RLC_PB_ACK;

   nch->size() = payload_;
   nch->ptype() = PT_AMPBPA;

   return npkt;
}


Packet     *AM::make_bitmap_ack(Packet * p)
{
   Packet     *npkt;
   hdr_rlc    *nllh;
   hdr_cmn    *nch;

   int first_missing = next_;

   while (seen_[first_missing & MWM]) {
      ++first_missing;
   }


   int fsn = first_missing - 1;

   if (fsn < 0) {
      // When the first packet ever gets a nack we shouldn't try to ack
      // everything until then.
      fsn = 0;
   }

   if (p == NULL) {
      npkt = allocpkt(0);

      nllh = hdr_rlc::access(npkt);
      nch = hdr_cmn::access(npkt);
      hdr_ip     *niph = hdr_ip::access(npkt);
      hdr_tcp    *ntcp = hdr_tcp::access(npkt);
      char       *mh = (char *) npkt->access(hdr_mac::offset_);

      nllh->lltype() = RLC_BACK;
      nllh->dst() = d_address_;
      nllh->src() = address_;
      nllh->seqno() = fsn;
      nllh->lengthInd_ = 0;
      nllh->padding_ = payload_ - StatusPDUSize();

      for (int i = 1; i < 3; i++) {
         nllh->payload_[i] = 0;
      }

      nllh->payload_[0] = StatusPDUSize();

      niph->flowid() = flowID_;
      niph->saddr() = -1;
      niph->sport() = -1;
      niph->daddr() = -1;
      niph->dport() = -1;
      niph->ttl() = 32;

      ntcp->seqno() = -1;

      nch->ptype() = PT_AMBA;
      nch->size() = payload_;

      struct hdr_mac *dh = (struct hdr_mac *) mh;

      dh->macDA_ = macDA_;
      dh->macSA_ = -1;
      dh->hdr_type() = ETHERTYPE_RLC;
   } else {
      npkt = p;
      nllh = hdr_rlc::access(npkt);
      nch = hdr_cmn::access(npkt);

      nllh->lengthInd_++;
      nllh->padding_ = nllh->padding_ - StatusPDUSize()
            - ((length_indicator_ + 1) / 8);
      nllh->payload_[nllh->lengthInd_ - 1] = StatusPDUSize();

      nllh->lltype() = RLC_PB_BACK;
      nch->ptype() = PT_AMPBBA;
   }

   nllh->FSN() = fsn;
   if ((maxseen_ - fsn + 1) > (payload_ * 8 - status_pdu_header_)) {
      nllh->length() = payload_ * 8 - status_pdu_header_;
   } else {
      nllh->length() = maxseen_ - fsn + 1;
   }


   nllh->bitmap_ = new int[nllh->length()];

   for (int i = 0; i < nllh->length(); i++) {
      seen_[(fsn + i) & MWM]
            ? nllh->bitmap(i) = 1 : nllh->bitmap(i) = 0;
   }

   return npkt;
}


/*
 * allocate a PDU and fill in required fields
 */
Packet     *AM::allocpkt(int uid)
{
   Packet     *p = Packet::alloc();
   hdr_cmn    *ch = hdr_cmn::access(p);

/*	ch->ptype() = PT_AMDA; */
   ch->uid() = uid;
   ch->error() = 0;
   ch->timestamp() = Scheduler::instance().clock();
   ch->iface() = UNKN_IFACE.value();
   ch->direction() = hdr_cmn::DOWN;
   //ch->ref_count() = 0;

   return (p);
}


void AM::makeSDU(int numSDU)
{

   Packet     *p;
   hdr_cmn    *ch;
   hdr_rlc    *llh;

   for (int i = 0; i < numSDU; i++) {
      p = sduB_.deque();
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

void AM::CSwitch(double bandwidth, double TTI)
{
   bandwidth_ = bandwidth;
   TTI_ = TTI;
}

/* 
 * Process timeout events.
 */
void AM::timeout(int tno, int flowID)
{
   /* retransmit timer */
   if (tno == RLC_TIMER_POLL) {
      if (maxseq_ == FSN_ && b_bal_ == 0) {
         /*
          * If no outstanding data, then don't do anything.  
          */
         return;
      }
      poll_seq_ = -1;
      set_poll_ = 1;
   } else if (tno == RLC_TIMER_RTX) {
      if (highest_ack_ == maxseq_) {
         /*
          * If no outstanding data, then don't do anything.  
          */
         return;
      }
      reset_rtx_timer();
   } else if (tno == RLC_TIMER_STPROB) {
      if (prohibited_) {
         send_status_ = 1;
         prohibited_ = 0;
         if (delsnd_timer_.status() == TIMER_PENDING
             && sent_TTI_PDUs_ < TTI_PDUs_) {
            earliest_status_send_ = tti_timer_.timeOfExpiry();
         } else {
            earliest_status_send_ = tti_timer_.timeOfExpiry() + TTI_;
         }
      }
   } else if (tno == RLC_TIMER_DELSND) {
      send_much(1);
   } else if (tno == RLC_TIMER_TTI) {
      TTI_time_ = Scheduler::instance().clock();
      TTI_PDUs_ = (int) (bandwidth_ * TTI_) / (payload_ * 8);
      tti_timer_.resched(TTI_);
      send_much(0);
   }
}


/*
 * Set retransmit timer using current provided rtt estimate.  By calling  
 * resched(),it does not matter whether the timer was already running.
 */
void AM::set_rtx_timer()
{
   rtx_timer_.resched(rtx_timeout_);
}


/*
 * We got a timeout or too many duplicate acks.  Clear the retransmit timer.
 * Resume the sequence one past the last packet acked.
 */
void AM::reset_rtx_timer()
{
   cancel_rtx_timer();
   t_seqno_ = highest_ack_ + 1;
   rtt_active_ = 0;
}


void AM::cancel_rtx_timer()
{
   rtx_timer_.force_cancel();
}


void AM::set_poll_timer()
{
   poll_timer_.resched(poll_timeout_);
}


void AM::cancel_poll_timer()
{
   poll_timer_.force_cancel();
}


void AM::set_status_prohibit_timer()
{
   stprob_timer_.resched(stprob_timeout_ -
                         (Scheduler::instance().clock() - TTI_time_)
         );
}


int AM::buff_size()
{
   return (rcvB_.size() + rxtB_.size());
}

