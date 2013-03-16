/*
 * Copyright (c) 2004, University of Cincinnati, Ohio.
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

/*
 * bt-stat.cc
 */

#include "bt-stat.h"
#include <string.h>


FILE *BtStat::logstat = NULL;
FILE *BtStat::logtoscreen = NULL;
FILE *BtStat::logstat2 = NULL;
FILE *BtStat::logstat3 = NULL;
FILE *BtStat::log = NULL;

int BtStat::trace_all_stat2_ = 0;
int BtStat::trace_all_stat3_ = 0;

void BtStat::init_logfiles()
{
    logstat = stdout;
    logtoscreen = stderr;
    logstat2 = stdout;
    logstat3 = stdout;
    log = stdout;
}

void BtStat::add(int nbytes, double now)
{
//    double now = Scheduler::instance().clock();
    if (now < _begin || _index >= _size) {
	return;
    }

    // Check if long period has passed without data
    while (now >= _nextstop) {
	if (++_index == _size) {
	    return;
	}
	_nextstop += _step;
    }

    _arr[_index] += nbytes;
    if (adjustL2caphdr_) {
	 _arr[_index] += 4;	// offset the 4 bytes l2cap hdr.
    }
}

void BtStat::recv(int nbytes, int dst, int dport, double now)
{
    if (now < _begin || _index >= _size) {
	return;
    }

    int i;
    for (i = 0; i < _numFlow; i++) {
	if (_recvBytes[i * _size + 1] == dport && 
		(_recvBytes[i * _size] == dst || dport == 255)) {
	    break;
	}
    }
    int ind = i;
    if (ind >= _numFlow) {
	int new_numFlow = ind + 1;
	int * newRecv = new int[new_numFlow * _size];
	memset(newRecv, 0, new_numFlow * _size * sizeof(int));
	memcpy(newRecv, _recvBytes, _numFlow * _size * sizeof(int));
	// int * newind = new int[new_numFlow];
	// memset(newind, 0, new_numFlow * sizeof(int));
	// memcpy(newind, _ind, _numFlow * sizeof(int));
	_numFlow = new_numFlow;
	delete [] _recvBytes;
	_recvBytes = newRecv;
	_recvBytes[ind * _size] = (dport == 255 ? -1 : dst);
	_recvBytes[ind * _size + 1] = dport;
    }
    
    // Check if long period has passed without data
    while (now >= _nextstop) {
	if (++_index == _size) {
	    return;
	}
	_nextstop += _step;
    }
    _recvBytes[_size * ind + _index] 
	+= (adjustL2caphdr_ ? nbytes + 4 : nbytes);
}

void BtStat::dump_recv(FILE *out, char ifs)
{
    if (_numFlow == 0) {
	return;
    }
    int i;
    for (i = 0; i < 2; i++) {
	if (i > 0) {
	    fprintf(out, "%c", ifs);
	}
	fprintf(out, "%d", _recvBytes[i] );
	for (int j = 1; j < _numFlow; j++) {
	    fprintf(out, " %d", _recvBytes[j * _size + i] );
	}
    }
    for (i = 2; i < _size - 2; i++) {
	fprintf(out, "%c", ifs);
	fprintf(out, "%d", _recvBytes[i]);
	for (int j = 1; j < _numFlow; j++) {
	    // fprintf(out, "%.2f ", _recvBytes[j * _size + i] / _step);
	    // fprintf(out, "%.2f ", _recvBytes[j * _size + i] * 8.0);
	    fprintf(out, " %d", _recvBytes[j * _size + i]);
	}
    }
}

void BtStat::record_delay(double ts, int hops, double now)
{
    if (now < _begin ) {
	return;
    }
    _flow_delay += now - ts;
    _flow_delay_cnt++;
    _flow_delay_hopcnt += hops;
}

void BtStat::add_page_time(double pt)
{
}

void BtStat::add_inq_time(double it, int)
{
}

void BtStat::dump()
{
    int i;
    // arr[size - 1] = 0;
    // arr[size - 2] = 0;
    double sum = 0;
    double nstep = 0;

    for (i = _size - 2; i >= 2; i--) {
	sum += _arr[i];
	nstep += _step;
	_ave[i] = _arr[i] / _step;
	_accu[i] = sum / nstep;
    }

    fprintf(logtoscreen, "%d thr: %f (%.2f/%.2f) eff: %f ( %.2f /723200)\n",
		_addr,
		_accu[2], _accu[2] * nstep, nstep, 
		_accu[2] * 8 / 723200, _accu[2] * 8);

    if (trace_all_stat2_ || trace_me_stat2_) {
	fprintf(logstat2, "%.2f ", _accu[2] * 8);
    }

    for (i = 2; i <= _size - 2; i++) {
	fprintf(logstat, "%f\t%f\t%f\t%f\n", _arr[i], _ave[i], _accu[i], _accu[i] * 8);
    }

    if (trace_me_stat_pernode_) {
/*
	for (i = 2; i <= _size - 2; i++) {
	    fprintf(mylogfile_, "%.2f ", _ave[i] * 8);
	}
*/
	// dump_recv(mylogfile_, '#');
	dump_recv(mylogfile_, '\t');
	fprintf(mylogfile_, " | %f %d %d\n",
		_flow_delay, _flow_delay_cnt, _flow_delay_hopcnt);
    }
    // dump_recv(stderr);
}
