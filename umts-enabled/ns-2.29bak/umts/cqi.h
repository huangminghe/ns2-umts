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
 * $Id: cqi.h,v 1.1.1.1 2006/03/08 13:53:01 rouil Exp $
 */

// CQI to Transport Block Size mappings
// for UE categories 1-6
// 3GPP TS 25.214 V5.5.0, page 39

#ifndef ns_cqi
#define ns_cqi

int         cqiMapping[31] = {

   0,                           // 0 (dummy)
   137,                         // 1
   173,                         // 2
   233,                         // 3
   317,                         // 4
   377,                         // 5
   461,                         // 6
   650,                         // 7
   792,                         // 8
   931,                         // 9
   1262,                        // 10
   1483,                        // 11
   1742,                        // 12
   2279,                        // 13
   2583,                        // 14
   3319,                        // 15
   3565,                        // 16
   4189,                        // 17
   4664,                        // 18
   5287,                        // 19
   5887,                        // 20
   6554,                        // 21
   7168,                        // 22
   7168,                        // 23
   7168,                        // 24
   7168,                        // 25
   7168,                        // 26
   7168,                        // 27
   7168,                        // 28
   7168,                        // 29
   7168                         // 30
};


// for UE categories 7-8 (not used)

/*
9719,  // 23
11418, // 24
14411, // 25
14411, // 26
14411, // 27
14411, // 28
14411, // 29
14411  // 30
*/


// for UE category 9 (not used)

/*
17300, // 26
17300, // 27
17300, // 28
17300, // 29
17300  // 30
*/


// for UE category 10 (not used)

/*
21754, // 27
23370, // 28
24222, // 29
25558  // 30
*/

#endif
