/* -*-	Mode:C++; c-basic-offset:8; tab-width:8 -*- */
/*
 * Copyright (c) 1997 Regents of the University of California.
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
 * 	This product includes software developed by the MASH Research
 * 	Group at the University of California Berkeley.
 * 4. Neither the name of the University nor of the Research Group may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#) $Header: /var/lib/cvs/ns-2.29/trace/trace.h,v 1.1.1.1 2006/03/08 13:52:49 rouil Exp $
 */

#ifndef ns_trace_h
#define ns_trace_h

#define NUMFLAGS 8

#include <math.h> // floor
#include <map.h>
#include "packet.h"
#include "basetrace.h"
#include <fstream>


/* Tracing has evolved into two types, packet tracing and event tracing.
Class Trace essentially supports packet tracing. 
However in addition to the basic tracing properties (that it derives from a BaseTrace class), pkt-tracing also requires to inherit some of the Connector class properties as well.

Hence Trace should be renamed as ConnectorTrace in the future.
And it shall have a BaseTrace * variable, where BaseTrace class supporting pure tracing functionalities and should be the parent class for all subsequent trace-related classes.
*/

class Trace : public Connector {
protected:
	nsaddr_t src_;
    nsaddr_t dst_;
    int callback_;

    virtual void format(int tt, int s, int d, Packet* p);
    void annotate(const char* s);
	int show_tcphdr_;  // bool flags; backward compat
	int show_sctphdr_; // bool flags; backward compat
	void callback();
public:
	Trace(int type);
        ~Trace();

	BaseTrace *pt_;    // support for pkt tracing

	int type_;	
    int command(int argc, const char*const* argv);
	static int get_seqno(Packet* p);
    void recv(Packet* p, Handler*);
	void recvOnly(Packet *p);

	//Default rounding is to 6 digits after decimal
	//#define PRECISION 1.0E+6
	//According to freeBSD /usr/include/float.h 15 is the number of digits 
	// in a double.  We can specify all of them, because we're rounding to
	// 6 digits after the decimal and and %g removes trailing zeros.
	//#define TIME_FORMAT "%.15g"
	// annoying way of tackling sprintf rounding platform 
	// differences :
	// use round(Scheduler::instance().clock()) instead of 
	// Scheduler::instance().clock().
	//static double round (double x, double precision=PRECISION) {
	//return (double)floor(x*precision + 0.5)/precision;
	//}

	virtual void write_nam_trace(const char *s);
	void trace(TracedVar* var);
	//void namdump();
};

class DequeTrace : public Trace {
public:
	DequeTrace(int type) : Trace(type) {}
	~DequeTrace();
	void recv(Packet* p, Handler*);

};

struct PacketRecord {
private:
	char *src_nodeaddr;
	char *dst_nodeaddr;
	char action;
	int uid;	
public:
	PacketRecord() {
		src_nodeaddr = new char;
		*src_nodeaddr = '\0';
		dst_nodeaddr = new char;
		*dst_nodeaddr = '\0';
		action = '\0';
		uid = 0;
	}

	PacketRecord(const PacketRecord& key) {
		int fromLen = strlen(key.src_nodeaddr)+1;
		src_nodeaddr = new char[fromLen];
		strcpy(src_nodeaddr, key.src_nodeaddr);

		int toLen = strlen(key.dst_nodeaddr)+1;
		dst_nodeaddr = new char[toLen];
		strcpy(dst_nodeaddr, key.dst_nodeaddr);

		action = key.action;
		uid = key.uid; 
	}

	~PacketRecord(){
		delete src_nodeaddr;
		delete dst_nodeaddr;
	}

	PacketRecord& operator=(const PacketRecord& key) {
		if (*this == key)
		{
			return *this;
		}

		delete src_nodeaddr;
		delete dst_nodeaddr;

		int fromLen = strlen(key.src_nodeaddr)+1;
		src_nodeaddr = new char[fromLen];
		strcpy(src_nodeaddr, key.src_nodeaddr);

		int toLen = strlen(key.dst_nodeaddr)+1;
		dst_nodeaddr = new char[toLen];
		strcpy(dst_nodeaddr, key.dst_nodeaddr);

		action = key.action;
		uid = key.uid;

		return *this;
	}

	friend bool operator==(const PacketRecord& a, const PacketRecord& b) {
		if( a.uid == b.uid 
			&& a.action == b.action 
			&& strcmp(a.src_nodeaddr, b.dst_nodeaddr) == 0 
			&& strcmp(a.dst_nodeaddr, b.dst_nodeaddr) == 0)
			return true;
		return false;
	}
	
	friend bool operator<(const PacketRecord& a, const PacketRecord& b) {
		int fromCmp = strcmp(a.src_nodeaddr, b.src_nodeaddr);
		int toCmp = strcmp(a.dst_nodeaddr, b.dst_nodeaddr);

		if (a.uid != b.uid) {
			return a.uid < b.uid ? true:false;
		}
		else if (a.action != b.action) {
			return a.action < b.action ? true:false;
		}
		else if (fromCmp != 0) {
			return fromCmp < 0 ? true:false;
		}
		else {
			return toCmp < 0 ? true:false;
		}
	}
	
	friend bool operator>(const PacketRecord& a, const PacketRecord& b) {
		int fromCmp = strcmp(a.src_nodeaddr, b.src_nodeaddr);
		int toCmp = strcmp(a.dst_nodeaddr, b.dst_nodeaddr);

		if (a.uid != b.uid) {
			return a.uid > b.uid ? true:false;
		}
		else if (a.action != b.action) {
			return a.action > b.action ? true:false;
		}
		else if (fromCmp != 0) {
			return fromCmp > 0 ? true:false;
		}
		else  {
			return toCmp > 0 ? true:false;
		}
	}

	void src_nodeaddrSet(const char* str) {
		int	srcLen = strlen(str) + 1;
		src_nodeaddr = new char[srcLen];
		strncpy(src_nodeaddr, str, srcLen);
	}

	void dst_nodeaddrSet(const char* str) {
		int dstLen = strlen(str) + 1;
		dst_nodeaddr = new char[dstLen];
		strncpy(dst_nodeaddr, str, dstLen);
	}

	void actionSet(const char flag) {
		action = flag;
	}

	void uidSet(const int val) {
		uid = val;
	}

	const char *src_nodeaddrGet() const {
		return src_nodeaddr;
	}
	char *dst_nodeaddrGet() const {
		return dst_nodeaddr;
	}
	char actionGet() const {
		return action;
	}
	int uidGet() const {
		return uid;
	}
};

#endif
