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
 * $Id: umtslink.cc,v 1.3 2006/11/16 16:54:10 rouil Exp $
 */

#include "umtslink.h"
#include "random.h"



/*
 * UmtsMac
 */

static class UmtsMacClass:public TclClass {
public:
   UmtsMacClass():TclClass("Mac/Umts") {
   } TclObject *create(int, const char *const *) {
      return (new UmtsMac);
   }
}

umts_class_mac;


void PhyFrame_Timer::expire(Event *)
{
   a_->timeout(0);
}


UmtsMac::UmtsMac():VirtualUmtsMac(), avg_delay_(0), frame_timer_(this)/*NIST*/, status_(LINK_STATUS_UP)/*end NIST*/
{
   bind_time("TTI_", &TTI_);
   bind_time("shared_delay_", &shared_delay_);
   fflush(stdout);
   //NIST: add supported events and commands
   linkType_ = LINK_UMTS;
   eventList_ = 0x3;
   commandList_ = 0x3;
   //end NIST
}

int UmtsMac::command(int argc, const char *const *argv)
{
   if (argc == 2) {
      Tcl & tcl = Tcl::instance();
      if (strcmp(argv[1], "set_access_delay") == 0) {
         avg_delay_ = shared_delay_;
         return TCL_OK;
      } else if (strcmp(argv[1], "TTI") == 0) {
         tcl.resultf("%f", TTI_);
         return TCL_OK;
      } else if (strcmp(argv[1], "BW") == 0) {
         tcl.resultf("%f", bandwidth_);
         return TCL_OK;
      } else if (strcmp(argv[1], "start_TTI") == 0) {
         frame_timer_.sched(TTI_);
         return TCL_OK;
      } //NIST: add connect/disconnect commands
      else if (strcmp(argv[1], "connect-link") == 0) {
        if (status_ == LINK_STATUS_DOWN) {
	  Mac::send_link_up (addr(), -1, -1); //TDB add macPoA
          status_ = LINK_STATUS_UP;
        }
        return TCL_OK; //for other status we are already connected
      } else if (strcmp(argv[1], "disconnect-link") == 0) {
        if (status_ != LINK_STATUS_DOWN) {
	  Mac::send_link_down (addr(), -1, LD_RC_EXPLICIT_DISCONNECT);
          status_ = LINK_STATUS_DOWN;
        }
        return TCL_OK;
      }
      //end NIST
   } else if (argc == 3) {
      if (strcmp(argv[1], "SA") == 0) {
         index_ = atoi(argv[2]);
         return TCL_OK;
      }
   }
   return Mac::command(argc, argv);
}

void UmtsMac::timeout(int tno)
{
   if (tno == 0) {
      if (q_.length() == 0) {
         frame_timer_.resched(TTI_);
         return;
      }

      double delay = Random::uniform(avg_delay_);
      double txt = TTI_;

      if (avg_delay_ != 0) {
         txt = txt + delay;
      }

      double data = 0;
      double bits_to_send = bandwidth_ * TTI_;
      Packet     *p = q_.dequeCopy();

      while ((data + (hdr_cmn::access(p)->size() * 8))
             <= bits_to_send) {
         Packet::free(p);
         p = q_.deque();
         data = data + (hdr_cmn::access(p)->size() * 8);
         // For convenience, we encode the transmit time
         // in the Mac header
         HDR_MAC(p)->txtime() = txt;
         downtarget_->recv(p, this);
         if (q_.length()) {
            p = q_.dequeCopy();
         } else {
            frame_timer_.resched(TTI_);
            return;
         }
      }
      Packet::free(p);
      frame_timer_.resched(TTI_);
      return;
   }
}

void UmtsMac::sendUp(Packet * p)
{
   hdr_mac    *mh = HDR_MAC(p);
   int dst = this->hdr_dst((char *) mh);  // mac destination address

//   hdr_cmn* ch = hdr_cmn::access(p);
//   hdr_rlc* llh = hdr_rlc::access(p);
//   hdr_ip* iph = hdr_ip::access(p);

   //NIST: add link status check
   if (status_ != LINK_STATUS_UP) {
     Packet::free(p);
     return;
   }
   //end NIST

   if (dst == index_) {
      // First bit of packet has arrived-- wait for
      // (txtime + delay_) before sending up
      Scheduler::instance().schedule(uptarget_, p, delay_ + mh->txtime());
   } else {
      Packet::free(p);
   }
}



void UmtsMac::sendDown(Packet * p)
{
   double bPtti_ = bandwidth_ * TTI_;

   //NIST: add link status check
   if (status_ != LINK_STATUS_UP) {
     drop(p);
     return;
   }
   //end NIST

   if ((q_.size() + hdr_cmn::access(p)->size() * 8) > (bPtti_ * 2)) {
      drop(p);
   } else {
      q_.enque(p);
   }

}

//NIST: add MIH method
void UmtsMac::link_connect (int macPoA)
{
  if (status_ == LINK_STATUS_DOWN) {
    Mac::send_link_up (addr(), -1, -1); //TDB add macPoA
    status_ = LINK_STATUS_UP;
  }
}
                                                                                                                                                        
void UmtsMac::link_disconnect ()
{
  if (status_ != LINK_STATUS_DOWN) {
    Mac::send_link_down (addr(), -1, LD_RC_EXPLICIT_DISCONNECT);
    status_ = LINK_STATUS_DOWN;
  }
}
                                                                                                                                                        
void UmtsMac::link_configure (link_parameter_config_t* config)
{
  assert (config);
  //we only support datarate for now
  if (config->bandwidth != PARAMETER_UNKNOWN_VALUE) {
    if (bandwidth_ != config->bandwidth) {
      //Mac::send_link_parameter_change (LINK_BANDWIDTH, dataRate_,config->bandwidth);
    }
    bandwidth_ = config->bandwidth;
  }
  config->bandwidth = bandwidth_;
  config->type = LINK_UMTS;
  //we set the rest to PARAMETER_UNKNOWN_VALUE
  config->ber = PARAMETER_UNKNOWN_VALUE;
  config->delay = PARAMETER_UNKNOWN_VALUE;
  config->macPoA = PARAMETER_UNKNOWN_VALUE;
}
//end NIST


/*
 *  UmtsPhy
 */

static class UmtsPhyClass:public TclClass {
public:
   UmtsPhyClass():TclClass("Phy/Umts") {
   } TclObject *create(int, const char *const *) {
      return (new UmtsPhy);
   }
}

class_UmtsPhy;

void UmtsPhy::sendDown(Packet * p)
{
   if (channel_) {
      channel_->recv(p, this);
   } else {
      Packet::free(p);
   }
}

// Note that this doesn't do that much right now.  If you want to incorporate
// an error model, you could insert a "propagation" object like in the
// wireless case.
int UmtsPhy::sendUp(Packet * /* pkt */ )
{
   return TRUE;
}

int UmtsPhy::command(int argc, const char *const *argv)
{
   return Phy::command(argc, argv);
}
