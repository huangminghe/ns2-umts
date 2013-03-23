/*
 * Copyright (c) 2004,2005, University of Cincinnati, Ohio.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the OBR Center for 
 *      Distributed and Mobile Computing lab at the University of Cincinnati.
 * 4. Neither the name of the University nor of the lab may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "bt-lossmod.h"
#include "baseband.h"
#include "random.h"

bool BTLossMod::lost(Baseband * bb, hdr_bt * bh)
{
    // Lost if far away
    return (bb->distance(bh->X_, bh->Y_) > Baseband::radioRange);
}

bool LMBlueHoc::lost(Baseband * bb, hdr_bt * bh)
{
    int dist = (int) bb->distance(bh->X_, bh->Y_);

    if (dist > Baseband::radioRange) {	// too far away
	return 1;
    }

    if (bh->type == hdr_bt::Id) {	// Id packet is supposed to be robust.
	return 0;
    }

    dist -= 2;
    if (dist < 0) {
	dist = 0;
    } else if (dist > 22) {
	return 1;
    }

    hdr_bt::packet_type type = bh->type;
    switch (type) {
    case hdr_bt::EV4:	// need work
    case hdr_bt::EV5:
    case hdr_bt::EV3:
    case hdr_bt::EV3_2:
    case hdr_bt::EV3_3:
    case hdr_bt::EV5_2:
    case hdr_bt::EV5_3:
	type = hdr_bt::HV1;
	break;

    case hdr_bt::DH1_2:
    case hdr_bt::DH1_3:
    case hdr_bt::HLO:
	type = hdr_bt::DH1;
	break;

    case hdr_bt::DH3_2:
    case hdr_bt::DH3_3:
	type = hdr_bt::DH3;
	break;

    case hdr_bt::DH5_2:
    case hdr_bt::DH5_3:
	type = hdr_bt::DH5;
	break;

    default:
	break;
    }

    double ran = Random::uniform();

    return (ran < FERvsDist[type][dist]);
}
