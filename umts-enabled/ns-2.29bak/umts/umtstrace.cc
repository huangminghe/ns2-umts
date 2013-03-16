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
 * $Id: umtstrace.cc,v 1.1.1.1 2006/03/08 13:53:01 rouil Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include "packet.h"
#include "ip.h"
#include "tcp.h"
#include "rtp.h"
#include "srm.h"
#include "flags.h"
#include "address.h"
#include "trace.h"
#include "tfrc.h"
#include "rap/rap.h"
#include "um.h"
#include "umtstrace.h"


class UMTSTraceClass:public TclClass {
public:
   UMTSTraceClass():TclClass("Trace/UMTS") {
   } TclObject *create(int argc, const char *const *argv) {
      if (argc >= 5) {
         return (new UMTSTrace(*argv[4]));
      }
      return 0;
   }
}

umts_trace_class;

char       *srm_names__[] = {
   SRM_NAMES
};

void UMTSTrace::format(int tt, int s, int d, Packet * p)
{
   if (s == -1) {
      s = hdr_rlc::access(p)->src();
   }
   if (d == -1) {
      d = hdr_rlc::access(p)->dst();
   }
   hdr_cmn    *th = hdr_cmn::access(p);
   hdr_ip     *iph = hdr_ip::access(p);
   hdr_tcp    *tcph = hdr_tcp::access(p);
   hdr_srm    *sh = hdr_srm::access(p);
   hdr_rlc    *rlch = hdr_rlc::access(p);

   const char *sname = "null";

   packet_t t = th->ptype();
   const char *name = packet_info.name(t);

   /* SRM-specific */
   if (strcmp(name, "SRM") == 0 || strcmp(name, "cbr") == 0
       || strcmp(name, "udp") == 0) {
      if (sh->type() < 5 && sh->type() > 0) {
         sname = srm_names__[sh->type()];
      }
   }

   if (name == 0) {
      abort();
   }

   int seqno = get_seqno(p);

   /* 
    * When new flags are added, make sure to change NUMFLAGS
    * in trace.h
    */
   char flags[NUMFLAGS + 1];

   for (int i = 0; i < NUMFLAGS; i++) {
      flags[i] = '-';
   }
   flags[NUMFLAGS] = 0;

   hdr_flags  *hf = hdr_flags::access(p);

   flags[0] = hf->ecn_ ? 'C' : '-'; // Ecn Echo
   flags[1] = hf->pri_ ? 'P' : '-';
   flags[2] = '-';
   flags[3] = hf->cong_action_ ? 'A' : '-';  // Congestion Action
   flags[4] = hf->ecn_to_echo_ ? 'E' : '-';  // Congestion Experienced
   flags[5] = hf->fs_ ? 'F' : '-';  // Fast start: see tcp-fs and tcp-int
   flags[6] = hf->ecn_capable_ ? 'N' : '-';

#ifdef notdef
   flags[1] = (iph->flags() & PF_PRI) ? 'P' : '-';
   flags[2] = (iph->flags() & PF_USR1) ? '1' : '-';
   flags[3] = (iph->flags() & PF_USR2) ? '2' : '-';
   flags[5] = 0;
#endif
   char       *src_nodeaddr = Address::instance().print_nodeaddr(iph->saddr());
   char       *src_portaddr = Address::instance().print_portaddr(iph->sport());
   char       *dst_nodeaddr = Address::instance().print_nodeaddr(iph->daddr());
   char       *dst_portaddr = Address::instance().print_portaddr(iph->dport());

   if (t == PT_UM || t == PT_AMDA || t == PT_AMPA || t == PT_AMBA
       || t == PT_AMPBPA || t == PT_AMPBBA || t == PT_AMDA_H1 || t == PT_AMDA_H2 || t == PT_AMDA_H3) {
      sprintf(pt_->buffer(),
              "%c " TIME_FORMAT " %d %d %s %d %s %d %s.%s %s.%s %d %d %d", tt,
              pt_->round(Scheduler::instance().clock()), s, d, name, th->size(),
              flags, iph->flowid() /* was p->class_ */ ,
              // iph->src() >> (Address::instance().NodeShift_[1]), 
              // iph->src() & (Address::instance().PortMask_), 
              // iph->dst() >> (Address::instance().NodeShift_[1]), 
              // iph->dst() & (Address::instance().PortMask_),
              src_nodeaddr, src_portaddr, dst_nodeaddr, dst_portaddr, seqno, th->uid(),   /* was p->uid_ */
              rlch->seqno());

   } else {

      if (!show_tcphdr_) {
         sprintf(pt_->buffer(),
                 "%c " TIME_FORMAT " %d %d %s %d %s %d %s.%s %s.%s %d %d", tt,
                 pt_->round(Scheduler::instance().clock()), s, d, name,
                 th->size(), flags, iph->flowid() /* was p->class_ */ ,
                 // iph->src() >> (Address::instance().NodeShift_[1]), 
                 // iph->src() & (Address::instance().PortMask_), 
                 // iph->dst() >> (Address::instance().NodeShift_[1]), 
                 // iph->dst() & (Address::instance().PortMask_),
                 src_nodeaddr, src_portaddr, dst_nodeaddr, dst_portaddr, seqno, th->uid());  /* was p->uid_ */
      } else {
         sprintf(pt_->buffer(), "%c " TIME_FORMAT " %d %d %s %d %s %d %s.%s %s.%s %d %d %d 0x%x %d %d", tt, pt_->round(Scheduler::instance().clock()), s, d, name, th->size(), flags, iph->flowid(),  /* was p->class_ */
                 // iph->src() >> (Address::instance().NodeShift_[1]), 
                 // iph->src() & (Address::instance().PortMask_), 
                 // iph->dst() >> (Address::instance().NodeShift_[1]), 
                 // iph->dst() & (Address::instance().PortMask_),
                 src_nodeaddr, src_portaddr, dst_nodeaddr, dst_portaddr, seqno, th->uid(),   /* was p->uid_ */
                 tcph->ackno(), tcph->flags(), tcph->hlen(), tcph->sa_length());
      }
      if (pt_->namchannel() != 0)
         sprintf(pt_->nbuffer(),
                 "%c -t " TIME_FORMAT
                 " -s %d -d %d -p %s -e %d -c %d -i %d -a %d -x {%s.%s %s.%s %d %s %s}",
                 tt, Scheduler::instance().clock(), s, d, name, th->size(),
                 iph->flowid(), th->uid(), iph->flowid(), src_nodeaddr,
                 src_portaddr, dst_nodeaddr, dst_portaddr, seqno, flags, sname);
   }
   //NIST: moved this code outside of else
   delete[]src_nodeaddr;
   delete[]src_portaddr;
   delete[]dst_nodeaddr;
   delete[]dst_portaddr;
   //end NIST
}


int
            UMTSTrace::get_seqno(Packet * p)
{
   hdr_cmn    *th = hdr_cmn::access(p);
   hdr_tcp    *tcph = hdr_tcp::access(p);
   hdr_rtp    *rh = hdr_rtp::access(p);
   hdr_rap    *raph = hdr_rap::access(p);
   hdr_tfrc   *tfrch = hdr_tfrc::access(p);

   packet_t t = th->ptype();
   int seqno;

   /* UDP's now have seqno's too */
   if (t == PT_RTP || t == PT_CBR || t == PT_UDP || t == PT_EXP
       || t == PT_PARETO || t == PT_COOT) {
      seqno = rh->seqno();
   } else if (t == PT_RAP_DATA || t == PT_RAP_ACK) {
      seqno = raph->seqno();
   } else if (t == PT_TCP || t == PT_ACK || t == PT_HTTP || t == PT_FTP
              || t == PT_TELNET) {
      seqno = tcph->seqno();
   } else if (t == PT_TFRC) {
      seqno = tfrch->seqno;
   } else if (t == PT_UM || t == PT_AMDA || t == PT_AMPA || t == PT_AMBA
              || t == PT_AMPBPA || t == PT_AMPBBA || t == PT_AMDA_H1 || t == PT_AMDA_H2 || t == PT_AMDA_H3) {
      seqno = tcph->seqno();
   } else {
      seqno = -1;
   }
   return seqno;
}
