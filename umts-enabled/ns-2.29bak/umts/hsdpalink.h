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
 * $Id: hsdpalink.h,v 1.1.1.1 2006/03/08 13:53:01 rouil Exp $
 */

#ifndef ns_hsdpa_link_h
#define ns_hsdpa_link_h

#include "error_model.h"
#include "phy.h"
#include "virtual_umtsmac.h"
#include <vector>
#include "math.h"
#include "umts-timers.h"
#include "umts-queue.h"
#include <stdio.h>


#define LONG_LINE 500

////////////////////////////HSDPA MAC//////////////////////////

// Defines a new data structure for the MAC-hs PDU Header
struct hdr_mac_hs {

   int         proc_num_;       // Associated HARQ process number
   int         priority_id_;    // Global Reordering Queue ID
   int         ts_num_;         // Transmission Sequence Number (TSN)
   int         size_index_id_;  // Size of the MAC-d PDUs
   int         num_pdus_;       // Number of MAC-d PDUs (RLC PDUs)

   static int  offset_;

   inline int &offset() {
      return offset_;
   } static hdr_mac_hs *access(const Packet * p) {
      return (hdr_mac_hs *) p->access(offset_);
   }

   int        &proc_num() {
      return proc_num_;
   }
   int        &priority_id() {
      return priority_id_;
   }
   int        &ts_num() {
      return ts_num_;
   }
   int        &size_index_id() {
      return size_index_id_;
   }
   int        &num_pdus() {
      return num_pdus_;
   }

};


class HsdpaMac;


class harq_process {
public:
   harq_process(HsdpaMac * machs):clear_process_timer((VirtualUmtsMac *) machs,
                                                      MACHS_TIMER_PROCESS_CLEAR)
   {
   } int       proc_number_;    // HARQ process number
   int         tx_number_;      // Number of transmissions. From 1 to total number of retransmissions
   vector<double> rx_powers_;   // Vector of received powers for all transmissions. Read in from external file.
   double      target_power;    // The target power of the harq_process. When the received power is higher than this,

   // the packet is received correctly.
   bool        isFree_;         // Status of HARQ process. TRUE - Process is free, FALSE - Process is being used.

   vector<Packet*> pdu_vector_;  // Vector of MAC-d PDUs which form the MAC-hs PDU ( RLC information retained)
   Packet     *proc_pdu_;       // MAC-hs PDU, used for NS purposes.

   HsdpaMacTimer clear_process_timer;  // Timer to simulate delay in processing ACK

};

// Defines a new data structure for the status of each reordering priority queue

class reord_mac_hs {
public:
   reord_mac_hs(HsdpaMac * machs):stall_timer_((VirtualUmtsMac *) machs,
                                               MACHS_TIMER_REORDER_STALL) {
   } int       next_expected_TSN_;
   int         T1_TSN_;
   int         rcvWindow_UpperEdge_;

   vector< vector<Packet*>* >  reord_buffer_;

   HsdpaMacTimer stall_timer_;  // Timer used to help ensure reordering queue does not stall


};

class HsdpaMac:public VirtualUmtsMac {
public:
   HsdpaMac();
   virtual void timeout(int tno);
   void        sendUp(Packet * p);
   void        sendDown(Packet * p);
   void        cred_alloc(vector<int>* pdu_pend_);
   void        recv(Packet * p, Handler * h);
   void        remove_credit_allocation(int queue_index);

   harq_process *getHARQProcess(int process);

   //  hsdpaPhy getDowntarget();     // Returns the connected physical channel

protected:
   int         command(int argc, const char *const *argv);

   int         current_proc_num_;   // Current HARQ stop-and-wait process
   int         pdu_size_;       // MAC-d PDU size
   int         tb_size_;        // Transport-block size in bits (dependent on CQI value)

   // time between credit_allocs, is used to determine how large cred_pend_ should be
   double      creditAllocationTimeoutInterval_;

// variables initialised from TCL:
   double      TTI_;
   double      flow_control_rtt_;   // Round trip time of Flow Control (RTT of Iub + some
   // CPU time)
   double      ack_process_delay_;  // Delay of ACk being sent over control channel
   double      stall_timer_delay_;  // Reordering Queue stall avoidance timer (see 3GPP
   // TS25.321)
   int         priority_max_;   // Maximum number of service priorities
   int         flow_max_;       // Maximum number of flows
   int         scheduler_type_; // 1. Round Robin 2. Maximum C/I
   int         max_mac_hs_buffer_level_;
   int         flow_control_mode_;  // 1. Per-flow 2. Per-node
   int         nr_harq_rtx_;    // Max number of HARQ retransmissions
   int         nr_harq_processes_;
   int         reord_buf_size_; // Size in elements
   int         mac_hs_headersize_;
   int         creditAllocationInterval_; // The number of TTIs between a new
                                          // credit allocation.
   double       alpha_;                // Fair Channel Dependent Scheduling (FCDS) parameters

// end variables from TCL

    vector <harq_process*> process_;
    vector <umtsQueue*>    q_;

   int         rcwWindow_Size_;

    vector <int> new_rlc_credits_;     // Credit allocation per queue
    vector <int> credits_allocated_;   // Number of packets that are allocated,
   // but not received yet

    vector <reord_mac_hs*>  reord_; // Per priority queue

    vector < vector<double>* >    snrBlerMatrix_;
    vector <bool>  activated_;
    vector <float> p_filtered_; // Filtered powers for FCDS
    vector <float> var_filtered_; // variance for Filtered powers for FCDS

    void        HS_schedule();
    int         ScheduleRoundRobin();
    int         ScheduleCoverI();
    int         ScheduleFairChannel();

    int         calc_num_pdus(int user_id, int pdu_size);
    line        *getLine(int user_id);
    void        setupHARQprocess(int user_id);
    void        hspdu_disassemble(vector < Packet * >*pdu_vect);
    void        mac_hs_pdu_gen(int buffer_index, int max_pdus);
    void        loadSnrBlerMatrix(char *filename);

    void        t1_expire(int priority_id);
    void        t1_stop(int priority_id);
    void        t1_cancel(int priority_id);
    void        t1_schedule(int priority_id, int tsn);
    bool        ro_isInsideWindow(int priority_id, int tsn);
    void        ro_placeInBuffer(int priority_id, int tsn,
                                    harq_process * process);
    void        ro_passUp(int priority_id, int tsn);
    void        ro_stopTransmission(harq_process * process);

    HsdpaMacTimer frame_timer_;
    HsdpaMacTimer cred_alloc_timer_;
    HsdpaMacTimer scheduler_timer_;

    vector <UmtsErrorModel*> errorModel_;

};

 ////////////////////////////PHY////////////////////////////

class HsdpaPhy:public Phy {
public:
   HsdpaPhy();
   void        recv(Packet * p, Handler * h);
   void        sendDown(Packet * p);
   int         sendUp(Packet * p);
protected:
   int         command(int argc, const char *const *argv);

};

#endif
