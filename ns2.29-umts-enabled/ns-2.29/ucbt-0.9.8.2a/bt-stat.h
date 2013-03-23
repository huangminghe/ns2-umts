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


#include <stdlib.h>
#include <stdio.h>

#ifndef __bt_stat_h__
#define __bt_stat_h__

class BtStat {
  public:

	//int recvBytes;

    BtStat(double b, double e, double s, int ad)
    : _begin(b), _end(e), _step(s) {
	_size = (int)((_end - _begin) / _step) + 2;
	_arr = new double[_size];
	_ave = new double[_size];
	_accu = new double[_size];
	for (int i = 0; i < _size; i++) { _arr[i] = 0; }
	_nextstop = _begin + _step;
	// _index = 0;
	_index = 2;
	_addr = ad;
	trace_me_stat2_ = 0;
	trace_me_stat3_ = 0;
	trace_me_stat_pernode_ = 0;
	adjustL2caphdr_ = 0;
	mylogfile_ = stdout;

	_flow_delay = 0;
	_flow_delay_cnt = 0;
	_flow_delay_hopcnt = 0;

	_recvBytes = NULL;
	_numFlow = 0;
    }
    ~BtStat() { delete [] _arr; delete [] _ave; delete [] _accu;}

    static void init_logfiles();

    static FILE * logstat;
    static FILE * logtoscreen;
    static FILE * logstat2;
    static FILE * logstat3;
    static FILE * log;

    static int trace_all_stat2_;
    int trace_me_stat2_;
    static int trace_all_stat3_;
    int trace_me_stat3_;
    int trace_me_stat_pernode_;

    FILE * mylogfile_;	// for indivial nodes
    int adjustL2caphdr_;

    int * _recvBytes;
    int _numFlow;

    double *_arr;
    double *_ave;
    double *_accu;
    int _size;
    double _begin;
    double _end;
    double _step;

    double _nextstop;
    int _index;
    int _addr;

    double _flow_delay;
    int _flow_delay_cnt;
    int _flow_delay_hopcnt;

    void dump();
    void add(int nbytes, double now);
    void recv(int nbytes, int src, int port, double now);
    void record_delay(double ts, int hops, double now);
    void dump_recv(FILE *out, char ifs = '\n');

    static void add_page_time(double pt);
    static void add_inq_time(double it, int);
};

#endif
