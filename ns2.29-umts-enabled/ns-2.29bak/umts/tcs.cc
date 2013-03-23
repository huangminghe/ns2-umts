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
 * $Id: tcs.cc,v 1.1.1.1 2006/03/08 13:53:01 rouil Exp $
 */

#include "packet.h"
#include "tcs.h"
#include "mac.h"
#include <iostream>

static class TCMClass:public TclClass {
public:
   TCMClass():TclClass("TrChMeasurer") {
   } TclObject *create(int, const char *const *) {
      return (new TCM);
   }
}

class_tcm;

static class CSClass:public TclClass {
public:
   CSClass():TclClass("ChannelSwitcher") {
   } TclObject *create(int, const char *const *) {
      return (new CS);
   }
}

class_cs;


void DownSwitch_Timer::expire(Event *)
{
   a_->timeout(1);
}

void Thp_Report_Timer::expire(Event *)
{
   a_->timeout(0);
}

void Report_Processing_Timer::expire(Event *)
{
   a_->timeout(1);
}

void Buff_Check_Timer::expire(Event *)
{
   a_->timeout(2);
}

void Pending_Timer::expire(Event *)
{
   a_->timeout(3);
}

TCM::TCM():BiConnector(), bs_(0), direction_(0), total_bits_(0), status_(0), cs_(0), am_(NULL), um_(NULL),
THP_RT(this), RPT(this), BRT(this), PTAT(this)
{

   bind_time("THP_REPORT_INTERVAL_", &THP_REPORT_INTERVAL_);
   bind("DOWNSWITCH_THRESHOLD_", &DOWNSWITCH_THRESHOLD_);
   bind("DOWNSWITCH_TIMER_THRESHOLD_", &DOWNSWITCH_TIMER_THRESHOLD_);
   bind("UL_RLC_BUF_UPSWITCH_THRESH_sRAB_", &UL_RLC_BUF_UPSWITCH_THRESH_sRAB_);
   bind("DL_RLC_BUF_UPSWITCH_THRESH_sRAB_", &DL_RLC_BUF_UPSWITCH_THRESH_sRAB_);
   bind_time("PENDING_TIME_AFTER_TRIGGER_", &PENDING_TIME_AFTER_TRIGGER_);
   bind_time("BUFF_CHECK_INTERVAL_", &BUFF_CHECK_INTERVAL_);

   BRT.resched(BUFF_CHECK_INTERVAL_);
   THP_RT.resched(THP_REPORT_INTERVAL_);
}

int TCM::command(int argc, const char *const *argv)
{
   if (argc == 3) {
      if (strcmp(argv[1], "CS") == 0) {
         cs_ = (CS *) TclObject::lookup(argv[2]);
         if (cs_ == NULL) {
            return (TCL_ERROR);
         }
         return (TCL_OK);
      } else if (strcmp(argv[1], "BS") == 0) {
         bs_ = atoi(argv[2]);
         return (TCL_OK);
      } else if (strcmp(argv[1], "RLC-AM") == 0) {
         am_ = (AM *) TclObject::lookup(argv[2]);
         if (am_ == NULL) {
            return (TCL_ERROR);
         }
         return (TCL_OK);
      } else if (strcmp(argv[1], "RLC-UM") == 0) {
	um_ = (UM *) TclObject::lookup(argv[2]);
	if (um_ == NULL) {
	   return (TCL_ERROR);
	}
        return (TCL_OK);
      }	
   }
   return (BiConnector::command(argc, argv));
}

void TCM::recv(Packet * p, Handler * callback)
{

   hdr_cmn    *ch = HDR_CMN(p);

   total_bits_ = total_bits_ + (ch->size() * 8);
   if (direction_ == dedicated) {
      sendUp(p, 0);
   } else if (direction_ == common) {
      sendDown(p, 0);
   } else {
      drop(p);
   }
}

void TCM::timeout(int tno)
{
   if (tno == 0) {
      double throughput_bps = total_bits_ / THP_REPORT_INTERVAL_;

      throughput_ = (int) throughput_bps / 1024;
      if (((int) throughput_bps % 1024) > 512) {
         throughput_++;
      }
      total_bits_ = 0;

      if (throughput_ < DOWNSWITCH_THRESHOLD_) {
         status_ = 1;
      } else if (throughput_ > DOWNSWITCH_TIMER_THRESHOLD_) {
         status_ = 2;
      } else {
         status_ = 0;
      }

      if (bs_) {
         RPT.resched(0.000005);
      }

      THP_RT.resched(THP_REPORT_INTERVAL_);

   } else if (tno == 1) {
      cs_->timeout(0);
   } else if (tno == 2) {
     
       int size = 0;
     
      // Added check for UM RLC support also
      if (am_ != NULL) {
        size =am_->buff_size() / 100;

        if ((am_->buff_size() % 100) > 50) {
          size++;
        }
	
      } else if (um_ != NULL) {
         size =um_->buff_size() / 100;

	 if ((um_->buff_size() % 100) > 50) {
		 size++;
	 }
      }
	      
      size = size * 100;
      if (bs_) {
         if (size > DL_RLC_BUF_UPSWITCH_THRESH_sRAB_) {
            cs_->timeout(2);
            PTAT.resched(PENDING_TIME_AFTER_TRIGGER_);
         } else {
            BRT.resched(BUFF_CHECK_INTERVAL_);
         }
      } else if (size > UL_RLC_BUF_UPSWITCH_THRESH_sRAB_) {
         cs_->timeout(2);
         PTAT.resched(PENDING_TIME_AFTER_TRIGGER_);
      } else {
         BRT.resched(BUFF_CHECK_INTERVAL_);
      }

   } else if (tno == 3) {
      BRT.resched(BUFF_CHECK_INTERVAL_);
   }
}


CS::CS():NsObject(), uplink_(0), downlink_(0), uplink_am_(NULL), downlink_am_(NULL), downlink_um_(NULL), uplink_um_(NULL),DS_T_sRab(this)
{
   bind_time("DOWNSWITCH_TIMER_sRAB_", &DOWNSWITCH_TIMER_sRAB_);
   bind_bool("noSwitching_", &noSwitching_);
}


int CS::command(int argc, const char *const *argv)
{
   if (argc == 3) {
      if (strcmp(argv[1], "uplink-tcm") == 0) {
         uplink_ = (TCM *) TclObject::lookup(argv[2]);
         if (uplink_ == NULL) {
            return (TCL_ERROR);
         }
         return (TCL_OK);
      } else if (strcmp(argv[1], "downlink-tcm") == 0) {
         downlink_ = (TCM *) TclObject::lookup(argv[2]);
         if (downlink_ == NULL)
            return (TCL_ERROR);
         return (TCL_OK);
      } else if (strcmp(argv[1], "uplink-AM") == 0) {
         uplink_am_ = (AM *) TclObject::lookup(argv[2]);
         if (uplink_am_ == NULL) {
            return (TCL_ERROR);
         }
         return (TCL_OK);
      } else if (strcmp(argv[1], "downlink-AM") == 0) {
         downlink_am_ = (AM *) TclObject::lookup(argv[2]);
         if (downlink_am_ == NULL) {
            return (TCL_ERROR);
         }
         return (TCL_OK);
      } else if (strcmp(argv[1], "downlink-UM") == 0) {
         downlink_um_ = (UM *) TclObject::lookup(argv[2]);
         if (downlink_um_ == NULL) {
            return (TCL_ERROR);
         }
         return (TCL_OK);
      } else if (strcmp(argv[1], "uplink-UM") == 0) {
         uplink_um_ = (UM *) TclObject::lookup(argv[2]);
         if (uplink_um_ == NULL) {
            return (TCL_ERROR);
         }
         return (TCL_OK);
      }
   } else if (argc == 6) {
      if (strcmp(argv[1], "rlc_DS_info") == 0) {
         ds_node_tti_ = atof(argv[2]);
         ds_node_bw_ = atof(argv[3]);
         ds_bs_tti_ = atof(argv[4]);
         ds_bs_bw_ = atof(argv[5]);
         return (TCL_OK);
      } else if (strcmp(argv[1], "rlc_US_info") == 0) {
         us_node_tti_ = atof(argv[2]);
         us_node_bw_ = atof(argv[3]);
         us_bs_tti_ = atof(argv[4]);
         us_bs_bw_ = atof(argv[5]);
         return (TCL_OK);
      }
   }
   return (NsObject::command(argc, argv));
}

void CS::timeout(int type)
{
   if (type == 0) {
      assert(uplink_ && downlink_);
      if (uplink_->status() == 1 && downlink_->status() == 1
          && DS_T_sRab.status() == TIMER_IDLE) {
         DS_T_sRab.resched(DOWNSWITCH_TIMER_sRAB_ + 0.00001);
      } else if ((uplink_->status() == 2 || downlink_->status() == 2)
                 && DS_T_sRab.status() == TIMER_PENDING) {
         DS_T_sRab.cancel();
      }
   } else if (type == 1) {
      /* do you want to introduce a probability here?
       * To simulate downswitch failures? */
      if (noSwitching_) {
         DS_T_sRab.resched(DOWNSWITCH_TIMER_sRAB_ + 0.00001);
         return;
      }
      uplink_->direction() = common;
      downlink_->direction() = common;

      if ( uplink_am_ != NULL && downlink_am_ !=NULL ) {
	uplink_am_->CSwitch(ds_node_bw_, ds_node_tti_);
	downlink_am_->CSwitch(ds_bs_bw_, ds_bs_tti_);
      } else if (uplink_um_ != NULL && downlink_um_ != NULL) {
	uplink_um_->CSwitch(ds_node_bw_, ds_node_tti_);
	downlink_um_->CSwitch(ds_bs_bw_, ds_bs_tti_);
      }
      
/*		cout<<"downswitch at "<<Scheduler::instance().clock()<<endl; */
      DS_T_sRab.resched(DOWNSWITCH_TIMER_sRAB_ + 0.00001);

   } else if (type == 2) {
      /* do you want to introduce a probability here?
       * To simulate upswitch failures? */
      if (noSwitching_) {
         return;
      }
      uplink_->direction() = dedicated;
      downlink_->direction() = dedicated;
      if ( uplink_am_ != NULL && downlink_am_ !=NULL ) {
	uplink_am_->CSwitch(us_node_bw_, us_node_tti_);
	downlink_am_->CSwitch(us_bs_bw_, us_bs_tti_);
      } else if (uplink_um_ != NULL && downlink_um_ != NULL) {
	uplink_um_->CSwitch(us_node_bw_, us_node_tti_);
	downlink_um_->CSwitch(us_bs_bw_, us_bs_tti_);
      }
/*		cout<<"upswitch at "<<Scheduler::instance().clock()<<endl; */
   }
}
