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
 * $Id: error_model.cc,v 1.1.1.1 2006/03/08 13:53:01 rouil Exp $
 */

#include "error_model.h"

UmtsErrorModel::UmtsErrorModel(double tti)
{
   tti_ = tti;
   init_ = 0;
   nrec_ = 0;
}

void UmtsErrorModel::attachTraceFile(char *filename)
{
   line       *t;
   FILE       *fp;              // filepointer
   char temp_line[LINE_LENGTH + 1]; // Last char for ending 0

   if ((fp = fopen(filename, "r")) == NULL) {
      printf("can't open file %s\n", filename);
      exit(1);
   }
   // count number of lines
   while (fgets(temp_line, LINE_LENGTH - 1, fp)) {
      if (*temp_line != '%') {
         nrec_++;
      }
   }
   rewind(fp);

   trace_ = new struct line[nrec_];

   t = trace_;
   for (int i = 0; i < nrec_; i++) {
      fgets(temp_line, LINE_LENGTH - 1, fp);
      int res =
            sscanf(temp_line, "%f %f %f %d", &(t->rx1), &(t->rx2), &(t->rx3),
                   &(t->cqi));
      if (res == 4) {
         // only increase the counter when the complete line has been correctly
         // read. In other cases (i.e. (hopefully) comments) skip this line.
         t++;
      } else if (res == EOF) {
         break;
      }
   }

   fclose(fp);
   init_ = 1;
}

line       *UmtsErrorModel::getElementAt(double time)
{
   if (init_ == 1) {
      int rec = (int) (time / tti_);

      if (rec >= nrec_) {
         // After the end of the tracefile
         printf("CQI + received powers tracefile is not large enough.\n");
         exit(1);
      }
      return &(trace_[rec]);
   } else {
      return NULL;
   }
}
