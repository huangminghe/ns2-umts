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
 * $Id: umtslink.h,v 1.1.1.1 2006/03/08 13:53:01 rouil Exp $
 */

#ifndef ns_umts_link_h
#define ns_umts_link_h

#include "phy.h"
#include "virtual_umtsmac.h"
#include "rlc.h"
#include "um.h"
#include "umts-queue.h"

////////////////////////////MAC////////////////////////////

class UmtsMac;

class PhyFrame_Timer:public TimerHandler {
public:
   PhyFrame_Timer(UmtsMac * a):TimerHandler() {
      a_ = a;
} protected:
   virtual void expire(Event * e);
   UmtsMac    *a_;
};


class UmtsMac:public VirtualUmtsMac {
public:
   UmtsMac();
   virtual void timeout(int tno);
   void        sendUp(Packet * p);
   void        sendDown(Packet * p);

   //NIST: add command method
   void link_connect (int);
   void link_disconnect ();
   void link_configure (link_parameter_config_t*);
   //end NIST

protected:
   int         command(int argc, const char *const *argv);
   double      avg_delay_;
   double      shared_delay_;
   double      TTI_;
   umtsQueue   q_;
   PhyFrame_Timer frame_timer_;
   //NIST:add status
   int status_;          //current link status.
   //end NIST
};

////////////////////////////PHY////////////////////////////

class UmtsPhy:public Phy {
public:
   UmtsPhy() {
   } void      sendDown(Packet * p);
   int         sendUp(Packet * p);

 protected:
   int         command(int argc, const char *const *argv);
};


#endif
