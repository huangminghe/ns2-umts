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
 * $Id: umts-headers.h,v 1.1.1.1 2006/03/08 13:53:01 rouil Exp $
 */

#ifndef ns_umts_headers_h
#define ns_umts_headers_h


#define ETHERTYPE_RLC 0x0900 // Type of Payload in MAC frame

enum RLCFrameType {
   RLC_DATA = 0x0000,           /* Data RLC PDU */
   RLC_ACK = 0x0001,            /* Positive ACK PDU (like Tahoe) */
   RLC_BACK = 0x0010,           /* Bitmap ACK PDU */
   RLC_PB_ACK = 0x0100,
   RLC_PB_BACK = 0x1000,
};


struct hdr_rlc {
   packet_t    lptype_;         // SDU type
   int         lerror_;         // error flag of SDU
   double      lts_;            // ts value of SDU
   int         lsize_;          // payload SDU size

   RLCFrameType lltype_;        // RLC frame type
   int         seqno_;          // PDU sequence number
   int         a_seqno_;        // PDU sequence number
   int         eopno_;          // end of SDU seqno
   int         segment_;        // the segment number of the PDU, starts with 0 for the first
   // PDU of a SDU.
   bool        poll_;           // poll flag

   int         payload_[3];
   int         lengthInd_;
   int         padding_;

   int         FSN_;
   int         length_;
   int        *bitmap_;

   nsaddr_t    src_;
   nsaddr_t    dst_;

   static int  offset_;
   inline int &offset() {
      return offset_;
   } static hdr_rlc *access(const Packet * p) {
      return (hdr_rlc *) p->access(offset_);
   }

   packet_t & lptype() {
      return (lptype_);
   }
   int        &lerror() {
      return lerror_;
   }
   double     &lts() {
      return (lts_);
   }
   int        &lsize() {
      return lsize_;
   }

   RLCFrameType & lltype() {
      return lltype_;
   }
   int        &seqno() {
      return seqno_;
   }
   int        &a_seqno() {
      return a_seqno_;
   }
   int        &eopno() {
      return eopno_;
   }
   int        &segment() {
      return segment_;
   }
   bool       &poll() {
      return poll_;
   }

   int        &FSN() {
      return FSN_;
   }
   int        &length() {
      return length_;
   }
   int        &bitmap(int i) {
      return bitmap_[i];
   }

   int        &lengthInd() {
      return lengthInd_;
   }
   int        &padding() {
      return padding_;
   }
   int        &payload(int i) {
      return payload_[i];
   }

   nsaddr_t & src() {
      return (src_);
   }
   nsaddr_t & dst() {
      return (dst_);
   }
};

#endif
