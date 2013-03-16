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
 * $Id: demuxerRtModule.cc,v 1.1.1.1 2006/03/08 13:53:01 rouil Exp $
 */

#include "node.h"
#include "demuxerRtModule.h"

static class DemuxerRoutingModuleClass:public TclClass {
public:
   DemuxerRoutingModuleClass():TclClass("RtModule/Demuxer") {
   } TclObject *create(int, const char *const *) {
      return (new DemuxerRoutingModule);
   }
}

class_demuxer_routing_module;

int DemuxerRoutingModule::command(int argc, const char *const *argv)
{
   Tcl & tcl = Tcl::instance();
   if (argc == 3) {
      if (strcmp(argv[1], "route-notify") == 0) {
         Node       *node = (Node *) (TclObject::lookup(argv[2]));

         if (node == NULL) {
            tcl.add_errorf("Invalid node object %s", argv[2]);
            return TCL_ERROR;
         }
         if (node != n_) {
            tcl.add_errorf("Node object %s different from n_", argv[2]);
            return TCL_ERROR;
         }
         n_->route_notify(this);
         return TCL_OK;
      }
      if (strcmp(argv[1], "unreg-route-notify") == 0) {
         Node       *node = (Node *) (TclObject::lookup(argv[2]));

         if (node == NULL) {
            tcl.add_errorf("Invalid node object %s", argv[2]);
            return TCL_ERROR;
         }
         if (node != n_) {
            tcl.add_errorf("Node object %s different from n_", argv[2]);
            return TCL_ERROR;
         }
         n_->unreg_route_notify(this);
         return TCL_OK;
      }
   }
   return (RoutingModule::command(argc, argv));
}
