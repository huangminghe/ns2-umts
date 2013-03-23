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
 * $Id: hsdpalink.cc,v 1.1.1.1 2006/03/08 13:53:01 rouil Exp $
 */

#include "hsdpalink.h"
#include "random.h"
#include "am-hs.h"
#include "cqi.h"
#include "error_model.h"
#include "math.h"
#include <assert.h>
#include <cassert>
#include <stdlib.h>

// These variables are used as 'super-globals', in this way the AM-HS object
// and the MAC-hs object at the BS can be reached.

extern RLC_HS *rlc_address;      // defined in am-hs.cc
HsdpaMac   *mac_address;        // points to the object in the BS

/*
 * HsdpaMac
 */

int hdr_mac_hs::offset_;

static class MAC_HSHeaderClass:public PacketHeaderClass {

public:
   MAC_HSHeaderClass():PacketHeaderClass("PacketHeader/MAC_HS",
                                         sizeof(hdr_mac_hs)) {

      bind_offset(&hdr_mac_hs::offset_);

   }
} class_machhshdr;




static class HsdpaMacClass:public TclClass {
public:
   HsdpaMacClass():TclClass("Mac/Hsdpa") {
   } TclObject *create(int, const char *const *) {
      return (new HsdpaMac);
   }
}

hsdpa_class_mac;

HsdpaMac::HsdpaMac():VirtualUmtsMac(), current_proc_num_(0), frame_timer_((VirtualUmtsMac *) this,
                                                     MACHS_TIMER_FRAME),
cred_alloc_timer_((VirtualUmtsMac *) this, MACHS_TIMER_CREDALLOC),
scheduler_timer_((VirtualUmtsMac *) this, MACHS_TIMER_SCHEDULE)
{

   // We set this global variable so that AM-HS can read this value and use the
   // pointer to this object. Because multiple Mac-HS-es are instantiated,
   // because we want the AM-HS to have the address of the Mac-HS located in the
   // RLC, and because the Mac-HS located in the RLC is instantiated the first,
   // we set this mac_address only when it isn't set already.
   if (mac_address == 0) {
      mac_address = this;
   }

   bind_time("TTI_", &TTI_);
   bind("credit_allocation_interval_", &creditAllocationInterval_);
   bind_time("flow_control_rtt_", &flow_control_rtt_);
   bind_time("ack_process_delay_", &ack_process_delay_);
   bind_time("stall_timer_delay_", &stall_timer_delay_);
   bind("flow_max_", &flow_max_);
   bind("priority_max_", &priority_max_);
   bind("scheduler_type_", &scheduler_type_);
   bind("max_mac_hs_buffer_level_", &max_mac_hs_buffer_level_);
   bind("mac_hs_headersize_", &mac_hs_headersize_);
   bind("flow_control_mode_", &flow_control_mode_);
   bind("nr_harq_rtx_", &nr_harq_rtx_);
   bind("nr_harq_processes_", &nr_harq_processes_);
   bind("reord_buf_size_", &reord_buf_size_);
   bind("alpha_", &alpha_);
  
   // credit_allocation_interval_ is specified in numbers of TTIs, for our
   // timer, we need it in seconds.
   creditAllocationTimeoutInterval_ = TTI_ * creditAllocationInterval_;

   // init harq process_
   for (int i = 0; i < nr_harq_processes_; i++) {
      harq_process *temp_hp = new harq_process(this);

      temp_hp->isFree_ = true;
      process_.push_back(temp_hp);
   }


   // init filtered/relative power and cqi values (for fair scheduling)
   // set to -999.0 so that we can determine the first setting of p_filtered
   // and the variance
   for (int i = 0; i < flow_max_; i++) {
     p_filtered_.push_back(-999.0);
     var_filtered_.push_back(-999.0);
   }

   
   // init q_
   for (int i = 0; i < priority_max_ * flow_max_; i++) {
      umtsQueue  *temp_hq = new umtsQueue;

      temp_hq->tx_seq_nr_ = 0;
      q_.push_back(temp_hq);
   }

   rcwWindow_Size_ = reord_buf_size_ / 2;

   // init reord_
   for (int i = 0; i < priority_max_; i++) {
      reord_mac_hs *temp_mh = new reord_mac_hs(this);

      temp_mh->next_expected_TSN_ = 0;
      temp_mh->T1_TSN_ = -1;
      temp_mh->rcvWindow_UpperEdge_ = -1; // reord_buf_size_ - 1;
      // TODO: This upperEdge value is not according to the standard, but this works.
      // a better solution would be to rewrite some of the reordering code to store
      // wrapped ( modulo reord_buf_size_ ) TSNs to access the reordering buffer.
      for (int j = 0; j < reord_buf_size_; j++) {
         vector < Packet * >*temp_pv = new vector < Packet * >;

         temp_mh->reord_buffer_.push_back(temp_pv);
      }
      reord_.push_back(temp_mh);
   }

   // init credits_allocated_
   for (int i = 0; i < priority_max_ * flow_max_; i++) {
      credits_allocated_.push_back(0);
   }

   // init new_rlc_credits_
   for (int i = 0; i < priority_max_ * flow_max_; i++) {
      new_rlc_credits_.push_back(0);
   }


   for (int i = 0; i < flow_max_; i++) {
      errorModel_.push_back( new UmtsErrorModel(TTI_) );
      activated_.push_back(false);
   }


}

int HsdpaMac::command(int argc, const char *const *argv)
{
   if (argc == 2) {
      Tcl & tcl = Tcl::instance();
      if (strcmp(argv[1], "set_access_delay") == 0) {
         // do nothing
         return TCL_OK;
      } else if (strcmp(argv[1], "TTI") == 0) {
         tcl.resultf("%f", TTI_);
         return TCL_OK;
      } else if (strcmp(argv[1], "BW") == 0) {
         tcl.resultf("%f", bandwidth_);
         return TCL_OK;
      } else if (strcmp(argv[1], "start_TTI") == 0) {
         // Start both the Frame Timer (which is resonsible for sending packets
         // to the physical layer every TTI) and the Scheduler Timer (which is
         // responsible for the scheduling every TTI). The scheduling will
         // always happen just before the sending.
         frame_timer_.sched(TTI_);
         return TCL_OK;
      } else if (strcmp(argv[1], "start_sched") == 0) {
         scheduler_timer_.sched(TTI_ - 0.0002);
         return TCL_OK;
      }
   } else if (argc == 3) {
      if (strcmp(argv[1], "SA") == 0) {
         index_ = atoi(argv[2]);
         // index_ is defined in mac.h and is the MAC address.

         return TCL_OK;
      } else if (strcmp(argv[1], "loadSnrBlerMatrix") == 0) {
         // Loads the SNR-BLER-Matix
         char       *filename = new char[strlen(argv[2]) + 1];

         strcpy(filename, argv[2]);
         loadSnrBlerMatrix(filename);
         return TCL_OK;
      }
   } else if (argc == 4) {
      if (strcmp(argv[1], "setErrorTrace") == 0) {
         // Loads the error trace for a certain user
         int flowID = atoi(argv[2]);
         char       *filename = new char[strlen(argv[3]) + 1];

         strcpy(filename, argv[3]);
         errorModel_.at(flowID)->attachTraceFile(filename);
         activated_.at(flowID) = true;
         return TCL_OK;
      }
   }
   return Mac::command(argc, argv);
}


/////////////////////////////////////////////////////
//                                                 //
//  Timer - Timeout actions                        //
//                                                 //
/////////////////////////////////////////////////////

void HsdpaMac::timeout(int tno)
{

   assert(current_proc_num_ >= 0 && current_proc_num_ < nr_harq_processes_);

   switch (tno) {
     case MACHS_TIMER_FRAME:
        // Send scheduled MAC-hs PDU to downtarget_ (Physical layer object)

        //1. send pdu to downtarget
        //2. update next_proc_num
        //3. reschedule a new TTI timer event
        
        if (!(process_.at(current_proc_num_)->isFree_)) {
           Packet     *temp_p =
                 (process_.at(current_proc_num_)->proc_pdu_)->copy();

           // Mark MAC-hs PDU according to HARQ transmission number 
           hdr_cmn   *ch;
           ch = HDR_CMN(temp_p);
           if ( process_.at(current_proc_num_)->tx_number_ == 1) {
             ch->ptype() = PT_AMDA_H1;
           } else if (process_.at(current_proc_num_)->tx_number_ == 2) {
             ch->ptype() = PT_AMDA_H2;
           } else if (process_.at(current_proc_num_)->tx_number_ == 3) {
             ch->ptype() = PT_AMDA_H3;
           }
           //assert(hdr_cmn::access(temp_p)->direction() == hdr_cmn::DOWN);

           hdr_cmn::access(temp_p)->direction() = hdr_cmn::DOWN;
           downtarget_->recv(temp_p);
        }
        current_proc_num_ = (current_proc_num_ + 1) % nr_harq_processes_;
        frame_timer_.resched(TTI_);
        break;
     case MACHS_TIMER_CREDALLOC:
        // Perform Credit Allocation to RLC


	//umhs_address->credit_update(new_rlc_credits_);
        rlc_address->credit_update(new_rlc_credits_);

        break;
     case MACHS_TIMER_SCHEDULE:
        // Schedule PDUs in priority buffers
        HS_schedule();
        // Schedule a new PDU schedule for every TTI
        scheduler_timer_.resched(TTI_);
        break;
     case MACHS_TIMER_REORDER_STALL:
        // Reordering Queue Stall Timer
        for (unsigned int i = 0; i < reord_.size(); i++) {
           if (Scheduler::instance().clock() >=
               reord_.at(i)->stall_timer_.timeOfExpiry()
               &&
               ((reord_.at(i)->stall_timer_.status() ==
                 reord_.at(i)->stall_timer_.TIMER_HANDLING)
                || (reord_.at(i)->stall_timer_.status() == TIMER_PENDING))) {
              t1_expire(i);
           }
        }

        break;
     case MACHS_TIMER_PROCESS_CLEAR:
        for (unsigned int i = 0; i < process_.size(); i++) {

           if (Scheduler::instance().clock()
               >= process_.at(i)->clear_process_timer.timeOfExpiry()
               &&
               ((process_.at(i)->clear_process_timer.status() ==
                 process_.at(i)->clear_process_timer.TIMER_HANDLING)
                || (process_.at(i)->clear_process_timer.status() ==
                    TIMER_PENDING))
                 ) {

              // Mark process as free
              process_.at(i)->isFree_ = true;
              Packet::free(process_.at(i)->proc_pdu_);
              process_.at(i)->proc_pdu_ = NULL;
           }
        }

        break;
     default:
        break;

   }
}

void HsdpaMac::recv(Packet * p, Handler * h)
{
   struct hdr_cmn *hdr = HDR_CMN(p);

   switch (hdr->direction()) {
     case hdr_cmn::DOWN:
        sendDown(p);
        return;
     case hdr_cmn::UP:
        sendUp(p);
        break;
     default:
        sendUp(p);
   }
}

/////////////////////////////////////////////////////
//                                                 //
//  SendUp - UE ACK/NACK and Reordering            //
//                                                 //
//  -- process packets coming from phy-layer       //
/////////////////////////////////////////////////////

void HsdpaMac::t1_expire(int priority_id)
{
   int tsn;

   for (tsn = reord_.at(priority_id)->next_expected_TSN_;
        tsn <= reord_.at(priority_id)->T1_TSN_ - 1; tsn++) {
      if (reord_.at(priority_id)->reord_buffer_.at(tsn % reord_buf_size_)->
          size() != 0) {
         ro_passUp(priority_id, tsn);
      }
   }

   tsn = reord_.at(priority_id)->T1_TSN_;
   while (reord_.at(priority_id)->reord_buffer_.at(tsn % reord_buf_size_)->
          size() != 0) {
      ro_passUp(priority_id, tsn);
      tsn++;
   }

   reord_.at(priority_id)->next_expected_TSN_ = tsn;

   t1_stop(priority_id);
}

void HsdpaMac::t1_stop(int priority_id)
{
   int tsn = reord_.at(priority_id)->rcvWindow_UpperEdge_;

   while (ro_isInsideWindow(priority_id, tsn)) {
      if (reord_.at(priority_id)->reord_buffer_.at(tsn % reord_buf_size_)->
          size() != 0) {
         t1_schedule(priority_id, tsn);
         break;
      }
      if (tsn > 0) {            // prevent negative TSNs
         tsn--;
      } else {
         break;
      }
   }
}

void HsdpaMac::t1_cancel(int priority_id)
{
   reord_.at(priority_id)->stall_timer_.cancel();
   t1_stop(priority_id);
}

void HsdpaMac::t1_schedule(int priority_id, int tsn)
{
   if (reord_.at(priority_id)->stall_timer_.status() != TIMER_PENDING) {
      reord_.at(priority_id)->T1_TSN_ = tsn;
      reord_.at(priority_id)->stall_timer_.resched(stall_timer_delay_);
   }
}



void HsdpaMac::sendUp(Packet * p)
{
   // UE MAC-hs Functionality

   // Reordering: Implemented from method defined in 3GPP TS25.321 v6.0.0
   //             Section 11.6.2.3, in v5.6.0 this method contains bugs


   hdr_mac_hs *hsh = hdr_mac_hs::access(p);
   hdr_cmn    *ch = hdr_cmn::access(p);
   int priority_id = hsh->priority_id();
   int tsn = hsh->ts_num();
   int harq_process_id = hsh->proc_num();
   harq_process *process = mac_address->getHARQProcess(harq_process_id);
   hdr_mac    *mh = HDR_MAC(p);


   if ((this->hdr_dst((char *) mh)) != index_) {
      // Free packet when it is not for us.
      Packet::free(p);
      return;
   }

   if (ch->error() == 0) {


      // Check if stall timer is required and none exists
      if (tsn > reord_.at(priority_id)->next_expected_TSN_) {
         t1_schedule(priority_id, tsn);
      }

      if (ro_isInsideWindow(priority_id, tsn)) {

         // Place MAC-hs PDU in reorder buffer if not less than next expected TSN
         if (tsn >= reord_.at(priority_id)->next_expected_TSN_) {
            ro_placeInBuffer(priority_id, tsn, process);
         }

      } else {

         // If TSN is located outside of receiver window
         ro_placeInBuffer(priority_id, tsn, process);

         //this is the only time the UpperEdge_ is updated
         reord_.at(priority_id)->rcvWindow_UpperEdge_ = tsn;

         int upperEdge = reord_.at(priority_id)->rcvWindow_UpperEdge_;


	 // Define the upper and lower limits of the space outside the Reordering window
	 int lower_index = max(0, (upperEdge - reord_buf_size_ + 1));
	 int upper_index = max(0, (upperEdge - rcwWindow_Size_));

          // Pass any PDUs in this space up to be disassembled
         for (int i = lower_index; i< upper_index; i++) {
            if (reord_.at(priority_id)->reord_buffer_.at(i % reord_buf_size_)->
                size() != 0) {
               ro_passUp(priority_id, tsn);
            }
         }

	 // If next_expected_TSN is outside the Reordering window, set it to be equal to TSN of last in window
	 if (reord_.at(priority_id)->next_expected_TSN_ <= (upperEdge - rcwWindow_Size_)) {
	   
	   reord_.at(priority_id)->next_expected_TSN_ =
	     upperEdge + 1 - rcwWindow_Size_;
	   
	   int sn = reord_.at(priority_id)->next_expected_TSN_;
    	   
	   // Pass up all consecutive waiting PDUs in the Reordering window after update of next_expected_TSN 
	   while (reord_.at(priority_id)->reord_buffer_.at(sn % reord_buf_size_)->
		  size() != 0) {
	     ro_passUp(priority_id, sn);
	     sn++;
	   }
	   
	   // Update next expected TSN to the first not received PDU
	   reord_.at(priority_id)->next_expected_TSN_ = sn;
	 }
         
      }
         
      //Clear HARQ process
      if (process->clear_process_timer.status() != TIMER_PENDING) {
         process->clear_process_timer.sched(ack_process_delay_);
      }

   } else {
      // The PDU was not received properly
      // Is this the last possible transmission of this PDU?
      if (process->tx_number_ == nr_harq_rtx_) {
         ro_stopTransmission(process);

      } else {
	// Is the TSN of the PDU outside the reordering window?
	// If it is, we dont want to retransmit it, so clear the associated process 
	if (tsn <= (reord_.at(priority_id)->rcvWindow_UpperEdge_ - rcwWindow_Size_)){
	  
	  // This is done here for the transmitter operation. The transmitter
	  // should not retransmit packets that are outside the window. It
	  // would be nicer when this part was implemented at the transmitter,
	  // but it's much easier to implement it here.
	  ro_stopTransmission(process);
	  
         } else {
            // Increment number of transmissions for this HARQ process
            process->tx_number_++;
         }
      }
   }                            // End of non-acked PDU conditional check

   Packet::free(p);             // delete the mac-hs pdu after processing.
}

bool HsdpaMac::ro_isInsideWindow(int priority_id, int tsn)
{
   bool inwin = (tsn <= reord_.at(priority_id)->rcvWindow_UpperEdge_);
   inwin = inwin && (tsn > (reord_.at(priority_id)->rcvWindow_UpperEdge_
                              - rcwWindow_Size_));
   return inwin;
}

void HsdpaMac::ro_placeInBuffer(int priority_id, int tsn,
                                harq_process * process)
{
   // Place each individual Mac-d PDU in the reordering. We can't place whole
   // Mac-hs PDUs in this buffer, because these Mac-hs PDUs are re-used.
   if (reord_.at(priority_id)->reord_buffer_.at(tsn % reord_buf_size_)->size() != 0){
	   printf("ro_placeInBuffer: OVERWRITING\n");
   }
   reord_.at(priority_id)->reord_buffer_.at(tsn % reord_buf_size_)->clear();
   for (unsigned int i = 0; i < process->pdu_vector_.size(); i++) {
      reord_.at(priority_id)->reord_buffer_.at(tsn %
                                               reord_buf_size_)->
            push_back(process->pdu_vector_.at(i));
   }
   if (tsn == reord_.at(priority_id)->next_expected_TSN_) {
     // Disassemble all consecutive received PDUs
     int sn = reord_.at(priority_id)->next_expected_TSN_;
     
     while (reord_.at(priority_id)->reord_buffer_.at(sn % reord_buf_size_)->
	    size() != 0) {
       ro_passUp(priority_id, sn);
       sn++;
     }
     
     // Update next expected TSN to the first not received PDU
     reord_.at(priority_id)->next_expected_TSN_ = sn;
   }
   
}

void HsdpaMac::ro_passUp(int priority_id, int tsn)
{
   // Dissamble PDU
   hspdu_disassemble(reord_.at(priority_id)->reord_buffer_.
                     at(tsn % reord_buf_size_));
   //Remove pduvector from reordering vector
   reord_.at(priority_id)->reord_buffer_.at(tsn % reord_buf_size_)->clear();
   if (tsn == reord_.at(priority_id)->T1_TSN_) {
      t1_cancel(priority_id);
   }
}

void HsdpaMac::ro_stopTransmission(harq_process * process)
{
   // Clear HARQ process as it is outside the receiver window
   if (process->clear_process_timer.status() != TIMER_PENDING) {
      process->clear_process_timer.sched(ack_process_delay_);
   }
   //unpack the mac-hs pdu and free all the included mac-d pdus
   for (unsigned int i = 0; i < process->pdu_vector_.size(); i++) {
      Packet::free(process->pdu_vector_.at(i));
   }
}

// This method returns a pointer to the actual harq_process. This method is
// called by the MAC-hs in the UEs to get the processes that reside in the
// MAC-hs in the BS.

harq_process *HsdpaMac::getHARQProcess(int process)
{
   return process_.at(process);
}

/////////////////////////////////////////////////////
//                                                 //
//  MAC-hs PDU Disassemble                         //
//                                                 //
/////////////////////////////////////////////////////

void HsdpaMac::hspdu_disassemble(vector < Packet * >*pdu_vect)
{

   // Send the individual PDUs up to the AM RLC in the UE
   for (unsigned int i = 0; i < pdu_vect->size(); i++) {

      // Because these small MAC-d PDU are transported inside a MAC-hs PDU the
      // new direction hasn't been set yet.
      (hdr_cmn::access(pdu_vect->at(i)))->direction() = hdr_cmn::UP;
      uptarget_->recv(pdu_vect->at(i));
   }
}



/////////////////////////////////////////////////////
//                                                 //
//  SendDown - Packet Enquing at MAC-hs            //
//                                                 //
/////////////////////////////////////////////////////

void HsdpaMac::sendDown(Packet * p)
{

   // Enque PDUs from RLC in their appropriate buffer

   hdr_ip     *piph = hdr_ip::access(p);
   int flow_ = piph->flowid();
   int priority_ = piph->prio();


   // Check if the appropriate is full (over buffer MAX level
   // Drop packet if TRUE
   // Else, enque in the buffer:

   // Nail wants to keep this here for posterity:
   //      int index = ((flow_max_ * priority_)- (flow_max_ - flow_) - 1);
   // index is determined by having flow_max_ flows for each priority
   int index = priority_ * flow_max_ + flow_;

   credits_allocated_.at(index)--;

   if ((q_.at(index)->length()) >= max_mac_hs_buffer_level_) {
      // This shouldn't happen. When this does happen it probably indicated an
      // error in the credit allocation algorithm.
      drop(p);
   } else {
      assert(hdr_cmn::access(p)->direction() == hdr_cmn::DOWN);
      q_.at(index)->enque(p);
   }

   //Note: Buffers indexed: f1p1,f2p1,f3p1,f1p2,f2p2,f3p2,f1p3...

}


/////////////////////////////////////////////////////
//                                                 //
//  Credit Allocation                              //
//                                                 //
/////////////////////////////////////////////////////

void HsdpaMac::cred_alloc(vector < int >*pdu_pend_)
{
   // Determine which Node B Flow Control is to be used
   // 1: per flow (default) 2: per node


   if (flow_control_mode_ == 1) {

      //1. determine credits pending from credit allocation vector
      //   (use Interval and flow control RTT values)
      //2. determine current buffersizes at MAC-hs
      //3. new credit allocation is minimum of pdu_pend_ and
      //   (Max buffer level - buffersize - credits pending)
      //4. update credits alocated vector
      //5. schedule update of credit allocation in AM RLC (delayed by Iub Delay)



      for (int index = 0; index < (flow_max_ * priority_max_); index++) {



         new_rlc_credits_.at(index) =
               min(pdu_pend_->at(index),
                   (max_mac_hs_buffer_level_ - q_.at(index)->length() -
                    credits_allocated_.at(index)));
         credits_allocated_.at(index) += new_rlc_credits_.at(index);


      }

      // Schedule credit_update in RLC, with Iub delay
      cred_alloc_timer_.sched(flow_control_rtt_ / 2);

   } else if (flow_control_mode_ == 2) {

      // CQI averaged over window, for each buffer
      // averaging window size set
      //

   } else {
      //Error message here as no correct flow control mode has been set
   }
}

// Used to deallocate credits whenever the RLC Window blocks packets from
// // being sent to the Node B. Called from AM-HS::Credit_Update.
void HsdpaMac::remove_credit_allocation(int q_index)
{
  if (credits_allocated_.at(q_index) > 0) {
     credits_allocated_.at(q_index) = credits_allocated_.at(q_index) - 1;
  }
}


/////////////////////////////////////////////////////
//                                                 //
//  Scheduling                                     //
//                                                 //
/////////////////////////////////////////////////////

int HsdpaMac::ScheduleRoundRobin()
{
  int index = -1;              // Queue-id, flow-priority combination
  int flowID;
  int max_num_pdus = 0;
  int pdu_size = 0;

   // Round Robin Scheduling
   //set index to equal the buffer index with the longest waiting head packet
   double min_arrival_time = Scheduler::instance().clock();

   for (int j = 0; j < flow_max_ * priority_max_; j++) {
      flowID = j % flow_max_; // this should be correct ;-)


      if (q_.at(j)->lastServedTime_ < min_arrival_time
         && q_.at(j)->length() > 0) {

         // CHECK: this should normally be 40 (set in ns-default)
         pdu_size = q_.at(j)->getPacketSize();
        

         // only schedule for queues that have a high enough CQI to actually send packets
         max_num_pdus = calc_num_pdus(flowID, pdu_size);


         if ( max_num_pdus > 0 ) {
            min_arrival_time = q_.at(j)->lastServedTime_;
            index = j;
         }
      }
   }
   // If all queues are empty, index will stay -1 and the rest of the
   // method will be skipped.

   return index;

}

int HsdpaMac::ScheduleCoverI()
{
   // C/I Scheduling
   int index = -1;
   int max_num_pdus = 0;
   int pdu_size = 0;

   static vector < int > temp_cqi_vector_;
   static vector < int > temp_fid_vector_;

   temp_fid_vector_.clear();
   temp_cqi_vector_.clear();

   // Check for activated flows

   for (int i = 0; i < flow_max_; i++) {

      if (activated_.at(i) == TRUE) {

         // get CQI for this flow
         line       *tempLine = getLine(i);

         // insert CQI into a temp vector of CQI values per flow (0 value means inactive)
         temp_cqi_vector_.push_back(tempLine->cqi);
         temp_fid_vector_.push_back(i);
      }
   }



   // We now have 2 vectors (of the same size) with all the cqi and fids

   // Straight Insertion sorting of vectors

   int temp_cqi_val, temp_fid_val;

   for (unsigned int i = 1; i < temp_cqi_vector_.size(); i++) {

      temp_cqi_val = temp_cqi_vector_.at(i);
      temp_fid_val = temp_fid_vector_.at(i);

      int j = i - 1;

      while (j >= 0 && temp_cqi_vector_.at(j) < temp_cqi_val) {
         temp_cqi_vector_.at(j + 1) = temp_cqi_vector_.at(j);
         temp_fid_vector_.at(j + 1) = temp_fid_vector_.at(j);
         j--;
      }

      temp_cqi_vector_.at(j + 1) = temp_cqi_val;
      temp_fid_vector_.at(j + 1) = temp_fid_val;
   }


   // Loop through the priorities of each flow, in order

   for (unsigned int i = 0; i < temp_fid_vector_.size(); i++) {

      

      for (int j = 0; j < priority_max_; j++) {
        
        pdu_size = q_.at((j* flow_max_)+ temp_fid_vector_.at(i))->getPacketSize();
        max_num_pdus = calc_num_pdus(temp_fid_vector_.at(i), pdu_size);
        
         // Set index to the first priority/flow that has packets waiting
         if ( (q_.at((j * flow_max_) + temp_fid_vector_.at(i))->length() >
            0) && (max_num_pdus > 0) ) {
            index = ((j * flow_max_) + temp_fid_vector_.at(i));
         }
         // Break out of priority loop if index has been set
         if (index >= 0) {
            break;
         }

      }
      // Break out of flow loop if index has been set
      if (index >= 0) {
         break;
      }
   }

   return index;
}

int HsdpaMac::ScheduleFairChannel()
{

    int index = -1;
    int max_num_pdus = 0;
    int pdu_size = 0;
    static vector < float > temp_power_vector_;
    static vector < int > temp_fid_vector_;
    static vector < float > temp_rel_power_vector_;

    temp_power_vector_.clear();
    temp_fid_vector_.clear();
    temp_rel_power_vector_.clear();

    // Check for activated flows
    for (int i = 0; i < flow_max_; i++) {

        if ( activated_.at(i) ) {

            // get TTI input trace line for this flow
            line *tempLine = getLine(i);

            // insert first received power/flow id into temp vectors
            temp_power_vector_.push_back(tempLine->rx1);
            temp_fid_vector_.push_back(i);
        }
    }

    int temp_fid_val;
    float temp_pow_val;

  // Calculate relative power for each active flow
    for (unsigned int i = 0; i < temp_fid_vector_.size(); i++) {

        //    temp_rel_power_vector_.push_back( temp_power_vector_.at(i) - p_filtered_.at(temp_fid_vector_.at(i)) );

        if ( var_filtered_.at(i) > 0.0 ) {
            float tmp;
            tmp = temp_power_vector_.at(i) - p_filtered_.at(temp_fid_vector_.at(i) );
            tmp = tmp / sqrt( var_filtered_.at( temp_fid_vector_.at(i) ) );
            temp_rel_power_vector_.push_back( tmp );
        } else {
            temp_rel_power_vector_.push_back(0.0);
        }
    }




    // Sort by Maximum relative CQI
    for (unsigned int i = 1; i < temp_rel_power_vector_.size(); i++) {

        temp_pow_val = temp_rel_power_vector_.at(i);
        temp_fid_val = temp_fid_vector_.at(i);

        int j = i - 1;

        while (j >= 0 && temp_rel_power_vector_.at(j) < temp_pow_val) {
            temp_rel_power_vector_.at(j + 1) = temp_rel_power_vector_.at(j);
            temp_fid_vector_.at(j + 1) = temp_fid_vector_.at(j);
            j--;
        }
        temp_rel_power_vector_.at(j + 1) = temp_pow_val;
        temp_fid_vector_.at(j + 1) = temp_fid_val;
    }



    // Loop through the priorities of each flow, in order

    for (unsigned int i = 0; i < temp_fid_vector_.size(); i++) {
      

      for (int j = 0; j < priority_max_; j++) {
        
        pdu_size = q_.at((j * flow_max_) + temp_fid_vector_.at(i))->getPacketSize();
        max_num_pdus = calc_num_pdus(temp_fid_vector_.at(i), pdu_size);
            // Set index to the first priority/flow that has packets waiting
            if ( (q_.at((j * flow_max_) + temp_fid_vector_.at(i))->length() > 0) && (max_num_pdus > 0)) {
                index = ((j * flow_max_) + temp_fid_vector_.at(i));
            }
            // Break out of priority loop if index has been set
            if (index >= 0) {
                break;
            }

        }
        // Break out of flow loop if index has been set
        if (index >= 0) {
            break;
        }
    }

    return index;

}

void HsdpaMac::HS_schedule()
{

   int index = -1;              // Queue-id, flow-priority combination
   int flowID;
   int max_num_pdus = 0;
   int pdu_size = 0;

   //Update p_filtered values for use in FCDS

    for (int i = 0; i < flow_max_; i++) {

        if (activated_.at(i) == TRUE) {

            line       *tempLine = getLine(i);

            if (p_filtered_.at(i) < -998.0) {

                p_filtered_.at(i) = tempLine->rx1;
                var_filtered_.at(i) = 0.0;
            } else {
                p_filtered_.at(i) = p_filtered_.at(i) * alpha_ +  (1 - alpha_) * tempLine->rx1;
                var_filtered_.at(i) = var_filtered_.at(i) * alpha_ +
                                        (1 - alpha_) *  pow( tempLine->rx1 - p_filtered_.at(i) ,2);
            }

        }

    }

   // If next process is free, schedule
   if (process_.at(current_proc_num_)->isFree_) {
      // Determine which scheduling algorithm is to be used
      // 1: Round Robin 2: Maximum C/I 3: FCDS

      if (scheduler_type_ == 1) {
        index = ScheduleRoundRobin();
      } else if (scheduler_type_ == 2) {
        index = ScheduleCoverI();
      } else if (scheduler_type_ == 3) {
        index = ScheduleFairChannel();
      } else {
         //error: undefined scheduler
      }


      if (index >= 0) {
        pdu_size = q_.at(index)->getPacketSize();
      
         // set flowID to the one we're going to use
         flowID = index % flow_max_; // this should be correct ;-)

         // Calculate the max number of MAC-d PDUs that will fit in a MAC-hs PDU
         max_num_pdus = calc_num_pdus(flowID, pdu_size);


         if (max_num_pdus > 0) {
            //setup HARQ process for this user
            setupHARQprocess(flowID);

            // Generate MAC-hs PDU and assign to process struct
            mac_hs_pdu_gen(index, max_num_pdus);
            // Update TSN according to the size of reordering buffer (default size = 64)
            q_.at(index)->tx_seq_nr_ = (q_.at(index)->tx_seq_nr_ + 1);
         } else {
         }
      }
   }

}


line  *HsdpaMac::getLine(int user_id){
   line  *tempLine =
         errorModel_.at(user_id)->getElementAt(Scheduler::instance().clock());

   if (tempLine != NULL) {
      // When the CQI value is out of range, we should set it to something that
      // is in the range.
      if (tempLine->cqi < 1) {
         tempLine->cqi = 1;
      }
      if (tempLine->cqi > 30) {
         tempLine->cqi = 30;
      }
   }
   return tempLine;
}


/////////////////////////////////////////////////////
//                                                 //
// Calculate the maximum # of PDUs in MAC-hs PDU   //
// Modifies global tb_size_                        //
/////////////////////////////////////////////////////

// pdu_size  relates to a MAC-d PDU size.
int HsdpaMac::calc_num_pdus(int user_id, int pdu_size)
{

   // Read TBS from input trace file
   // Calculate, using pdu_size, the # that will fit in TBS
   // Use mac-hs header size (should be a default value)

   int num_pdus = 0;

   // if pdu_size is not usable, return 0
   if (pdu_size <= 0) { return num_pdus; }

   tb_size_ = 0; //global class variable

   line  *tempLine = getLine(user_id);

   if (tempLine != NULL) {
      tb_size_ = cqiMapping[tempLine->cqi];
   }

   if (tb_size_ > mac_hs_headersize_) {
      num_pdus = int ((tb_size_ - mac_hs_headersize_) / (pdu_size * 8));
   }

   return num_pdus;
}


/////////////////////////////////////////////////////
//                                                 //
// Setup HARQ process for this TTI
//                                                 //
/////////////////////////////////////////////////////

void HsdpaMac::setupHARQprocess(int user_id)
{

   line  *tempLine = getLine(user_id); // read the input trace for clock.now


   // Add first Rx power to process struct (from input trace file)
   process_.at(current_proc_num_)->rx_powers_.clear();
   process_.at(current_proc_num_)->rx_powers_.push_back(tempLine->rx1);
   process_.at(current_proc_num_)->rx_powers_.push_back(tempLine->rx2);
   process_.at(current_proc_num_)->rx_powers_.push_back(tempLine->rx3);

   int temp_random = Random::integer(snrBlerMatrix_.size());

   process_.at(current_proc_num_)->target_power =
         snrBlerMatrix_.at(temp_random)->at(tempLine->cqi - 1);


   // Add indication of 1st transmission to process struct
   process_.at(current_proc_num_)->tx_number_ = 1;

   // Add indication of process number to process struct
   process_.at(current_proc_num_)->proc_number_ = current_proc_num_;

   // Add indication of process status to process struct
   process_.at(current_proc_num_)->isFree_ = false;

}



/////////////////////////////////////////////////////
//                                                 //
//  MAC-hs PDU Generation                          //
//                                                 //
/////////////////////////////////////////////////////


void HsdpaMac::mac_hs_pdu_gen(int index, int max_pdus)
{


   // Create NS packet for MAC-hs PDU and store in process struct

   Packet     *c = q_.at(index)->dequeCopy();

   hdr_mac_hs *hsh;
   hdr_cmn    *ch;
   hdr_ip     *iph;
   unsigned int deqLen = 0;

   hsh = hdr_mac_hs::access(c);
   ch = HDR_CMN(c);
   iph = hdr_ip::access(c);

   // Rename packet type for HARQ tracking
   ch->ptype() = PT_AMDA_H1;

   hsh->proc_num() = current_proc_num_;
   hsh->priority_id() = iph->prio();
   hsh->size_index_id() = pdu_size_;
   hsh->num_pdus() = max_pdus;
   hsh->ts_num() = q_.at(index)->tx_seq_nr_;

   ch->size() = (int) floor(tb_size_ / 8.0);

   process_.at(current_proc_num_)->proc_pdu_ = c;

   process_.at(current_proc_num_)->pdu_vector_.clear();

   deqLen = min(max_pdus, q_.at(index)->length());

   // Deque buffer and form a vector of the pdu packets for a HARQ process
   for (unsigned int i = 0; i < deqLen; i++) {
      process_.at(current_proc_num_)->pdu_vector_.push_back(q_.at(index)->
                                                            deque());
   }
}

// This method will read a file that consists of a large number of values that
// represent the minimum receive power for a successfull transmission. It
// populates the snrBlerMatrix_ vector.

void HsdpaMac::loadSnrBlerMatrix(char *filename)
{

   FILE       *fp;              // filepointer
   char temp_line[LONG_LINE];
   float temp_val[30];

   if ((fp = fopen(filename, "r")) == NULL) {
      fprintf(stderr, "can't open file %s\n", filename);
      exit(1);
   }

   while (fgets(temp_line, LONG_LINE, fp)) {
      // lines starting with % are comments and are ignored
      if (*temp_line == '%') {
         continue;
      }
      int res = sscanf(temp_line,
                       "%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
                       &(temp_val[0]), &(temp_val[1]), &(temp_val[2]),
                       &(temp_val[3]), &(temp_val[4]),
                       &(temp_val[5]), &(temp_val[6]), &(temp_val[7]),
                       &(temp_val[8]), &(temp_val[9]),
                       &(temp_val[10]), &(temp_val[11]), &(temp_val[12]),
                       &(temp_val[13]), &(temp_val[14]),
                       &(temp_val[15]), &(temp_val[16]), &(temp_val[17]),
                       &(temp_val[18]), &(temp_val[19]),
                       &(temp_val[20]), &(temp_val[21]), &(temp_val[22]),
                       &(temp_val[23]), &(temp_val[24]),
                       &(temp_val[25]), &(temp_val[26]), &(temp_val[27]),
                       &(temp_val[28]), &(temp_val[29])
            );

      if (res == 30) {
         vector < double >*temp_vector = new vector < double >;

         for (int i = 0; i < 30; i++) {
            temp_vector->push_back(temp_val[i]);
         }
         snrBlerMatrix_.push_back(temp_vector);
      } else {
         // when not all 30 cqi values are read in correctly, stop the reading
         // and processing of the file.
         break;
      }
   }

}


static class HsdpaPhyClass:public TclClass {
public:
   HsdpaPhyClass():TclClass("Phy/Hsdpa") {
   } TclObject *create(int, const char *const *) {
      return (new HsdpaPhy);
   }
}

class_HsdpaPhy;


HsdpaPhy::HsdpaPhy():Phy()
{
}

void HsdpaPhy::recv(Packet * p, Handler * h)
{

   struct hdr_cmn *hdr = HDR_CMN(p);

   switch (hdr->direction()) {
     case hdr_cmn::DOWN:
        sendDown(p);
        return;
     case hdr_cmn::UP:
        if (sendUp(p) == 0) {
           Packet::free(p);
           return;
        } else {
           uptarget_->recv(p, (Handler *) 0);
        }
        break;
     default:
        if (sendUp(p) == 0) {
           Packet::free(p);
           return;
        } else {
           uptarget_->recv(p, (Handler *) 0);
        }
   }
}

void HsdpaPhy::sendDown(Packet * p)
{
   hdr_cmn    *ch;

   ch = hdr_cmn::access(p);

   if (channel_ != NULL) {
      channel_->recv(p, this);
   } else {
      Packet::free(p);
   }
}

int HsdpaPhy::sendUp(Packet * p)
{

   hdr_mac_hs *hsh = hdr_mac_hs::access(p);
   hdr_cmn    *ch = hdr_cmn::access(p);

   int harq_process_id = hsh->proc_num();
   harq_process *process = mac_address->getHARQProcess(harq_process_id);


   if (process->rx_powers_[process->tx_number_ - 1] < process->target_power) {
      ch->error() = 1;
   } else {
      ch->error() = 0;
   }

   return 1;
}

int HsdpaPhy::command(int argc, const char *const *argv)
{
   return Phy::command(argc, argv);
}


