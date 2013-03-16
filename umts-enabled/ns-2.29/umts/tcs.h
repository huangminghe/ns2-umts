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
 * $Id: tcs.h,v 1.1.1.1 2006/03/08 13:53:01 rouil Exp $
 */

/*
 * Code for Transport Channel Switching
 */

#ifndef ns_tcs_h
#define ns_tcs_h

#include "bi-connector.h"
#include "timer-handler.h"
#include "am.h"
#include "um.h"

#define	dedicated	0
#define	common		1


class CS;
class TCM;

class DownSwitch_Timer:public TimerHandler {
public:
   DownSwitch_Timer(CS * a):TimerHandler() {
      a_ = a;
} protected:
   virtual void expire(Event * e);
   CS         *a_;
};


class Thp_Report_Timer:public TimerHandler {
public:
   Thp_Report_Timer(TCM * a):TimerHandler() {
      a_ = a;
} protected:
   virtual void expire(Event * e);
   TCM        *a_;
};

class Report_Processing_Timer:public TimerHandler {
public:
   Report_Processing_Timer(TCM * a):TimerHandler() {
      a_ = a;
} protected:
   virtual void expire(Event * e);
   TCM        *a_;
};

class Buff_Check_Timer:public TimerHandler {
public:
   Buff_Check_Timer(TCM * a):TimerHandler() {
      a_ = a;
} protected:
   virtual void expire(Event * e);
   TCM        *a_;
};

class Pending_Timer:public TimerHandler {
public:
   Pending_Timer(TCM * a):TimerHandler() {
      a_ = a;
} protected:
   virtual void expire(Event * e);
   TCM        *a_;
};


class CS:   public NsObject {
public:
   CS();
   virtual void timeout(int type);
protected:
   int         command(int argc, const char *const *argv);
   void        recv(Packet *, Handler * callback = 0) {
   } int       noSwitching_;
   TCM        *uplink_;
   TCM        *downlink_;
   AM         *uplink_am_;
   AM         *downlink_am_;
   UM         *downlink_um_;
   UM         *uplink_um_;
   double      us_node_tti_;
   double      us_node_bw_;
   double      ds_node_tti_;
   double      ds_node_bw_;
   double      us_bs_tti_;
   double      us_bs_bw_;
   double      ds_bs_tti_;
   double      ds_bs_bw_;
   double      DOWNSWITCH_TIMER_sRAB_;
   DownSwitch_Timer DS_T_sRab;
};

class TCM:  public BiConnector {
public:
   TCM();
   virtual void timeout(int tno);
   int         status() {
      return status_;
   } int      &direction() {
      return direction_;
   }
   int        &totalBits() {
      return total_bits_;
   }
 protected:
   int         command(int argc, const char *const *argv);
   void        recv(Packet *, Handler * callback = 0);

   int         bs_;
   int         direction_;
   int         total_bits_;
   int         throughput_;
   int         status_;
   CS         *cs_;
   AM         *am_;
   UM         *um_;

   double      THP_REPORT_INTERVAL_;
   double      DOWNSWITCH_THRESHOLD_;

   double      DOWNSWITCH_TIMER_THRESHOLD_;

   int         UL_RLC_BUF_UPSWITCH_THRESH_sRAB_;
   int         DL_RLC_BUF_UPSWITCH_THRESH_sRAB_;

   double      PENDING_TIME_AFTER_TRIGGER_;
   double      BUFF_CHECK_INTERVAL_;

   Thp_Report_Timer THP_RT;
   Report_Processing_Timer RPT;
   Buff_Check_Timer BRT;
   Pending_Timer PTAT;

};

#endif
