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
 *	bt-node.cc
 */

#include "bt-node.h"
#include "random.h"
#include "scat-form-law.h"
#include "scat-form-pl.h"
#include "lmp-piconet.h"
#include "lmp-link.h"

static class BTNodeClass:public TclClass {
  public:
    BTNodeClass():TclClass("Node/BTNode") { } 
    TclObject *create(int, const char *const *) {
	return (new BTNode());
    }
} class_btnode;

void BTNodeTimer::handle(Event * e)
{
    if (e == &node_->on_ev_) {
	node_->on();
    }
}

BTNode::BTNode()
:  _agent(0), wifi_(0), scatFormator_(0), stat_(0), _timer(this), _on(0)
{
    bind("randomizeSlotOffset_", &randomizeSlotOffset_);
    bind("enable_clkdrfit_in_rp_", &enable_clkdrfit_in_rp_);

    randomizeSlotOffset_ = 1;
    enable_clkdrfit_in_rp_ = 1;
    logfile_ = stdout;
}

int BTNode::_numFileOpened = 0;
const char *BTNode::_tracefilename[MAXTRACEFILES];
FILE *BTNode::_tracefile[MAXTRACEFILES];
char BTNode::filenameBuff[4096];
// char* filenameBuffAvl = filenameBuff;
// char* filenameBuffAvl = NULL;
int BTNode::filenameBuffInd = 0;

// Trace in UCBT is handled differently from the main ns part.
// There are 2 sets of commands in the forms of
//
//      trace-all-XXX [on|off|<filename>]
//      trace-me-XXX  [on|off|<filename>]
// the 'all' commands affect all bluetooth nodes, while 'me' commands 
// affect the current node only.
int BTNode::setTrace(const char *cmdname, const char *arg, int appnd)
{
    FILE **tfile = NULL;
    int *flag = NULL;
    int all = 0;
    const char *name;

    if (!strncmp(cmdname, "trace-all-", 10)) {
	all = 1;
	name = cmdname + 10;
    } else if (strncmp(cmdname, "trace-me-", 9)) {
	fprintf(stderr, "Unknown command %s %s. \n", cmdname, arg);
	return 0;
    } else {
	name = cmdname + 9;
    }

    if (!strcmp(name, "tx")) {
	if (all) {
	    flag = &Baseband::trace_all_tx_;
	} else {
	    flag = &bb_->trace_me_tx_;
	}
	tfile = &BtStat::log;
    } else if (!strcmp(name, "rx")) {
	if (all) {
	    flag = &Baseband::trace_all_rx_;
	} else {
	    flag = &bb_->trace_me_rx_;
	}
	tfile = &BtStat::log;
    } else if (!strcmp(name, "in-air")) {
	if (all) {
	    flag = &Baseband::trace_all_in_air_;
	} else {
	    flag = &bb_->trace_me_in_air_;
	}
	tfile = &BtStat::log;
    } else if (!strcmp(name, "POLL")) {
	if (all) {
	    flag = &Baseband::trace_all_poll_;
	} else {
	    flag = &bb_->trace_me_poll_;
	}
	tfile = &BtStat::log;
    } else if (!strcmp(name, "NULL")) {
	if (all) {
	    flag = &Baseband::trace_all_null_;
	} else {
	    flag = &bb_->trace_me_null_;
	}
	tfile = &BtStat::log;
    } else if (!strcmp(name, "link")) {
	if (all) {
	    flag = &Baseband::trace_all_link_;
	} else {
	    flag = &bb_->trace_me_link_;
	}
	tfile = &BtStat::logstat;
    } else if (!strcmp(name, "awmmf")) {
	if (all) {
	    flag = &Baseband::trace_all_awmmf_;
	} else {
	    flag = &bb_->trace_me_awmmf_;
	}
	tfile = &BtStat::logstat;
    } else if (!strcmp(name, "stat")) {
	if (all) {
	    flag = &Baseband::trace_all_stat_;
	} else {
	    flag = &bb_->trace_me_stat_;
	}
	tfile = &BtStat::logstat;
    } else if (!strcmp(name, "stat2")) {
	if (!stat_) {
	    fprintf(stderr, "stat_ is not set.");
	    abort();
	}
	if (all) {
	    flag = &BtStat::trace_all_stat2_;
	} else {
	    flag = &stat_->trace_me_stat2_;
	}
	tfile = &BtStat::logstat2;
    } else if (!strcmp(name, "stat3")) {
	if (!stat_) {
	    fprintf(stderr, "stat_ is not set.");
	    abort();
	}
	if (all) {
	    // fprintf(stderr, "trace_all can't be used with stat3.\n");
	    // abort();
	    flag = &BtStat::trace_all_stat3_;
	} else {
	    flag = &stat_->trace_me_stat3_;
	}
	tfile = &BtStat::logstat3;
    } else if (!strcmp(name, "stat-pernode")) {
	if (!stat_) {
	    fprintf(stderr, "stat_ is not set.");
	    abort();
	}
	if (all) {
	    fprintf(stderr,
		    "trace_all can't be used with stat-pernode.\n");
	    abort();
	} else {
	    flag = &stat_->trace_me_stat_pernode_;
	    tfile = &stat_->mylogfile_;
	}
    } else if (!strcmp(name, "l2cmd")) {
	if (all) {
	    flag = &L2CAP::trace_all_l2cap_cmd_;
	} else {
	    flag = &l2cap_->trace_me_l2cap_cmd_;
	}
	tfile = &BtStat::log;
    } else if (!strcmp(name, "bnep")) {
	if (all) {
	    flag = &BNEP::trace_all_bnep_;
	} else {
	    flag = &bnep_->trace_me_bnep_;
	}
	tfile = &BtStat::log;
    } else if (!strcasecmp(name, "scoAgent")) {
	if (all) {
	    flag = &ScoAgent::trace_all_scoagent_;
	} else {
	    fprintf(stderr, "trace_me can't be used with scoAgent.\n");
	    abort();
	}
	tfile = &BtStat::log;
    } else {
	fprintf(stderr, "Unknown command %s %s. \n", cmdname, arg);
	return 0;
    }

    if (!strcmp(arg, "off")) {
	*flag = 0;
	return 1;
    }

    *flag = 1;
    if (!strcmp(arg, "on")) {	// Use defined tracefile default stdout
	return 1;
    } else if (!strcmp(arg, "stdout")) {
	*tfile = stdout;
	return 1;
    } else if (!strcmp(arg, "stderr")) {
	*tfile = stderr;
	return 1;
    } else {
	return setTraceStream(tfile, arg, cmdname, appnd);
    }
}

int BTNode::setTraceStream(FILE ** tfileref, const char *fname,
			   const char *cmd, int appnd)
{
    int i;

    for (i = 0; i < _numFileOpened; i++) {
	if (!strcmp(fname, _tracefilename[i])) {
	    *tfileref = _tracefile[i];
	    if (appnd) {
		fprintf(stderr, "file %s is open. Can't use 'a' flag.",
			fname);
		return 0;
	    }
	    return 1;
	}
    }

    if (_numFileOpened >= MAXTRACEFILES) {
	fprintf(stderr,
		"Too many trace files opened.  Change MAXTRACEFILES in bt.h and recompile.\n");
	return 0;
    }

    const char *flag = (appnd ? "a" : "w");
    if ((_tracefile[_numFileOpened] = fopen(fname, flag))) {
	strcpy(filenameBuff + filenameBuffInd, fname);
	_tracefilename[_numFileOpened] = filenameBuff + filenameBuffInd;
	filenameBuffInd += (strlen(fname) + 1);
	*tfileref = _tracefile[_numFileOpened];
	_numFileOpened++;
	return 1;
    } else {
	fprintf(stderr, "Can't open trace file: %s for cmd: %s\n", fname,
		cmd);
	return 0;
    }
}

void BTNode::on()
{
    Tcl & tcl = Tcl::instance();
    if (scatFormator_) {
	lmp_->scanWhenOn_ = 0;
	lmp_->on();
	scatFormator_->start();
    } else {
	lmp_->on();
    }
    tcl.evalf("%s start ", (dynamic_cast < Agent * >(ragent_))->name());
}

int BTNode::command(int argc, const char *const *argv)
{
    Tcl & tcl = Tcl::instance();

    if (argc > 2 && !strcmp(argv[1], "make-pico")) {
	hdr_bt::packet_type pt = hdr_bt::str_to_packet_type(argv[2]);
	hdr_bt::packet_type rpt = hdr_bt::str_to_packet_type(argv[3]);
	for (int i = 4; i < argc; i++) {
	    BTNode *slave = lookupNode(atoi(argv[i]));
	    // join(slave, pt, rpt);
	    bnep_join(slave, pt, rpt);
	}
	return (TCL_OK);

    } else if (argc > 2 && !strcmp(argv[1], "make-pico-fast")) {
	hdr_bt::packet_type pt = hdr_bt::str_to_packet_type(argv[2]);
	hdr_bt::packet_type rpt = hdr_bt::str_to_packet_type(argv[3]);
	for (int i = 4; i < argc; i++) {
	    BTNode *slave = lookupNode(atoi(argv[i]));
	    if (!slave) {
		slave = (BTNode *) TclObject::lookup(argv[i]);
	    }
	    if (!slave) {
		fprintf(stderr, "make-pico-fast: node lookup fails: %s\n",
			argv[i]);
		return TCL_ERROR;
	    }
	    bnep_join(slave, pt, rpt);
	}
	return (TCL_OK);

    } else if (argc > 2 && !strcmp(argv[1], "pico-range")) {
	int numNode = argc - 5;
	BTNode **nd = new BTNode *[argc];
	double x1 = atof(argv[2]);
	double y1 = atof(argv[3]);
	double x2 = atof(argv[4]);
	double y2 = atof(argv[5]);
	int i;

	bb_->x1_range = x1;
	bb_->y1_range = y1;
	bb_->x2_range = x2;
	bb_->y2_range = y2;

	bb_->X_ = Random::uniform() * (x2 - x1) + x1;
	bb_->Y_ = Random::uniform() * (y2 - y1) + y1;
	// fprintf(stderr, "(%d, %f, %f)\n", bb_->bd_addr_, bb_->X_, bb_->Y_);

	for (i = 1; i < numNode; i++) {
	    if (argv[i + 5][0] >= '\0' && argv[i + 5][0] <= '9') {
		nd[i] = lookupNode(atoi(argv[i + 5]));
	    } else {
		nd[i] = (BTNode *) TclObject::lookup(argv[i + 5]);
	    }
	    if (!nd[i]) {
		fprintf(stderr, "manu-rt-path: node lookup fails: %s\n",
			argv[i + 1]);
		return TCL_ERROR;
	    }
	    nd[i]->bb_->x1_range = x1;
	    nd[i]->bb_->y1_range = y1;
	    nd[i]->bb_->x2_range = x2;
	    nd[i]->bb_->y2_range = y2;

	    nd[i]->bb_->X_ = Random::uniform() * (x2 - x1) + x1;
	    nd[i]->bb_->Y_ = Random::uniform() * (y2 - y1) + y1;
	    // fprintf(stderr, "(%d, %f, %f)\n", nd[i]->bb_->bd_addr_,
	    //      nd[i]->bb_->X_, nd[i]->bb_->Y_);
	}

	return (TCL_OK);

    } else if (argc > 2 && !strcmp(argv[1], "dstNodes")) {
	for (int i = 2; i < argc; i++) {
	    BTNode *nd = lookupNode(atoi(argv[i]));
	    if (!nd) {
		nd = (BTNode *) TclObject::lookup(argv[i]);
	    }
	    if (!nd) {
		fprintf(stderr, "dstNodes: node lookup fails: %s\n",
			argv[i]);
		return (TCL_ERROR);
	    }
	    nd->bnep_->_isDst = 1;
	}
	return (TCL_OK);

    } else if (argc > 2 && !strncmp(argv[1], "manu-rt-", 8)) {
	int numNode = argc - 1;
	BTNode **nd = new BTNode *[argc];
	int i;
	nd[0] = this;
	for (i = 1; i < numNode; i++) {
	    if (argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') {
		nd[i] = lookupNode(atoi(argv[i + 1]));
	    } else {
		nd[i] = (BTNode *) TclObject::lookup(argv[i + 1]);
	    }
	    if (!nd[i]) {
		fprintf(stderr, "manu-rt-path: node lookup fails: %s\n",
			argv[i + 1]);
		return TCL_ERROR;
	    }
	}

	if (!strcmp(argv[1], "manu-rt-path")) {
	    for (i = 0; i < numNode - 1; i++) {
		nd[i]->ragent_->addRtEntry(nd[numNode - 1]->bb_->bd_addr_,
					   nd[i + 1]->bb_->bd_addr_, 0);
	    }

	    for (i = numNode - 1; i > 0; i--) {
		nd[i]->ragent_->addRtEntry(nd[0]->bb_->bd_addr_,
					   nd[i - 1]->bb_->bd_addr_, 0);
	    }
	} else if (!strcmp(argv[1], "manu-rt-linear")) {
	    for (int src = 0; src < numNode - 1; src++) {
		for (int dst = src + 1; dst < numNode; dst++) {
		    for (i = src; i < dst; i++) {
			nd[i]->ragent_->addRtEntry(nd[dst]->bb_->bd_addr_,
						   nd[i +
						      1]->bb_->bd_addr_,
						   0);
			nd[i +
			   1]->ragent_->addRtEntry(nd[src]->bb_->bd_addr_,
						   nd[i]->bb_->bd_addr_,
						   0);
		    }
		}
	    }
	} else if (!strcmp(argv[1], "manu-rt-star")) {
	    for (i = 1; i < numNode - 1; i++) {
		nd[0]->ragent_->addRtEntry(nd[i]->bb_->bd_addr_,
					   nd[i]->bb_->bd_addr_, 0);
		nd[i]->ragent_->addRtEntry(nd[0]->bb_->bd_addr_,
					   nd[0]->bb_->bd_addr_, 0);
	    }
	} else {
	    fprintf(stderr, "%s: command is not recognized.\n", argv[1]);
	    return TCL_ERROR;
	}

	return TCL_OK;

    } else if (!strcmp(argv[1], "set-statist")) {
	if (argc < 5) {
	    return TCL_ERROR;
	}
	double begin = atof(argv[2]);
	double end = atof(argv[3]);
	double step = atof(argv[4]);
	if (stat_) {
	    delete stat_;
	}
	stat_ = new BtStat(begin, end, step, bb_->bd_addr_);

	if (argc >= 6) {
	    if (!strcmp(argv[5], "adjust-l2cap-hdr")) {
		stat_->adjustL2caphdr_ = 1;
	    }
	}
	return (TCL_OK);

    } else if (argc == 2) {

	if (!strcmp(argv[1], "cancel-inquiry-scan")) {
	    bb_->inquiryScan_cancel();
	    return TCL_OK;

	} else if (!strcmp(argv[1], "cancel-inquiry")) {
	    lmp_->HCI_Inquiry_Cancel();
	    return TCL_OK;

	    // don't use this cmd directly. use on in tcl space instead.
	} else if (!strcmp(argv[1], "turn-it-on")) {
	    if (_on) {
		return TCL_ERROR;
	    }
	    _on = 1;
	    if (randomizeSlotOffset_) {
		Scheduler::instance().schedule(&_timer, &on_ev_,
					       Random::integer(1250) *
					       1E-6);
	    } else {
		on();
	    }

	    return TCL_OK;

	} else if (!strcmp(argv[1], "version")) {
	    fprintf(stderr, "%s\n\n", BTVERSION);
	    return TCL_OK;

	} else if (!strcmp(argv[1], "print-stat")) {
	    printStat();
	    return TCL_OK;

	} else if (!strcmp(argv[1], "print-all-stat")) {
	    printAllStat();
	    return TCL_OK;

	} else if (!strcmp(argv[1], "reset-energy")) {
	    bb_->energyReset();
	    return TCL_OK;

	} else if (!strcmp(argv[1], "reset-energy-allnodes")) {
	    energyResetAllNodes();
	    return TCL_OK;

	} else if (!strcmp(argv[1], "print-lmp-cmd-len")) {
	    fprintf(stderr, "sizeof(LMPLink::ParkReq) : %ld\n",
		    (long)sizeof(LMPLink::ParkReq)); //NIST: cast to long for 64bits
	    fprintf(stderr, "sizeof(LMPLink::ModBeacon) : %ld\n",
		    (long)sizeof(LMPLink::ModBeacon)); //NIST: cast to long for 64bits
	    fprintf(stderr, "sizeof(LMPLink::UnparkBDADDRreq) : %ld\n",
		    (long)sizeof(LMPLink::UnparkBDADDRreq)); //NIST: cast to long for 64bits
	    fprintf(stderr, "sizeof(LMPLink::UnparkPMADDRreq) : %ld\n",
		    (long)sizeof(LMPLink::UnparkPMADDRreq)); //NIST: cast to long for 64bits
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "checkWiFi")) {
	    Baseband::check_wifi_ = 1;
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "notCheckWiFi")) {
	    Baseband::check_wifi_ = 0;
	    return TCL_OK;
	}

    } else if (argc == 3) {

	if (!strcasecmp(argv[1], "LossMod")) {
	    if (!strcasecmp(argv[2], "BlueHoc")) {
		if (Baseband::lossmod) {
		    delete Baseband::lossmod;
		}
		Baseband::lossmod = new LMBlueHoc();
	    } else if (!strcasecmp(argv[2], "off")) {
		if (Baseband::lossmod) {
		    delete Baseband::lossmod;
		}
		Baseband::lossmod = new BTLossMod();
	    } else {
		fprintf(stderr,
			"unknown parameter for LossMod [BlueHoc|off]: %s\n",
			argv[2]);
		return TCL_ERROR;
	    }
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "scatForm")) {
	    if (!strcasecmp(argv[2], "law")) {
		scatFormator_ = new ScatFormLaw(this);
	    } else {
		fprintf(stderr,
			"unknown parameter for scatForm [law|]: %s\n",
			argv[2]);
		return TCL_ERROR;
	    }
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "setall_scanWhenOn")) {
	    setall_scanwhenon(atoi(argv[2]));
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "TPollDefault")) {
	    Baseband::T_poll_default = atoi(argv[2]);
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "TPollMax")) {
	    Baseband::T_poll_max = atoi(argv[2]);
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "radioRange")) {
	    Baseband::radioRange = atof(argv[2]);
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "CollisionDist")) {
	    Baseband::collisionDist = atof(argv[2]);
	    return TCL_OK;

	    // use it before the node is turned on.
	} else if (!strcasecmp(argv[1], "set-clock")) {
	    lmp_->setClock(atoi(argv[2]));
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "ifqtype")) {
	    l2cap_->ifq_ = new char[strlen(argv[2]) + 1];
	    strcpy(l2cap_->ifq_, argv[2]);
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "log-target")) {
	    log_target_ = (Trace *) TclObject::lookup(argv[2]);
	    if (log_target_ == 0) {
		return TCL_ERROR;
	    }
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "SchedAlgo")) {
	    if (!strcasecmp(argv[2], "RR")) {
		if (bb_->linkSched->type() != BTLinkScheduler::RR) {
		    delete bb_->linkSched;
		    bb_->linkSched = new BTRR(bb_);
		}
	    } else if (!strcasecmp(argv[2], "DRR")) {
		if (bb_->linkSched->type() != BTLinkScheduler::DRR) {
		    delete bb_->linkSched;
		    bb_->linkSched = new BTDRR(bb_);
		}
	    } else if (!strcasecmp(argv[2], "ERR")) {
		if (bb_->linkSched->type() != BTLinkScheduler::ERR) {
		    delete bb_->linkSched;
		    bb_->linkSched = new BTERR(bb_);
		}
	    } else if (!strcasecmp(argv[2], "PRR")) {
		if (bb_->linkSched->type() != BTLinkScheduler::PRR) {
		    delete bb_->linkSched;
		    bb_->linkSched = new BTPRR(bb_);
		}
	    } else if (!strcasecmp(argv[2], "FCFS")) {
		if (bb_->linkSched->type() != BTLinkScheduler::FCFS) {
		    delete bb_->linkSched;
		    bb_->linkSched = new BTFCFS(bb_);
		}
	    } else {
		fprintf
		    (stderr,
		     "unknown parameter for SchedAlgo [DRR|ERR|PRR|FCFS]: %s\n",
		     argv[2]);
		return TCL_ERROR;
	    }
	    return TCL_OK;

	} else if (scatFormator_ &&
		   scatFormator_->type() == ScatFormator::SFLaw &&
		   !strcasecmp(argv[1], "sf-law-delta")) {
	    ((ScatFormLaw *) scatFormator_)->_delta = atof(argv[2]);
	    return TCL_OK;

	} else if (scatFormator_ &&
		   scatFormator_->type() == ScatFormator::SFLaw &&
		   !strcasecmp(argv[1], "sf-law-p")) {
	    ((ScatFormLaw *) scatFormator_)->_P = atof(argv[2]);
	    return TCL_OK;

	} else if (scatFormator_ &&
		   scatFormator_->type() == ScatFormator::SFLaw &&
		   !strcasecmp(argv[1], "sf-law-k")) {
	    ((ScatFormLaw *) scatFormator_)->_K = atoi(argv[2]);
	    return TCL_OK;

	} else if (scatFormator_ &&
		   scatFormator_->type() == ScatFormator::SFLaw &&
		   !strcasecmp(argv[1], "sf-law-term-schred")) {
	    ((ScatFormLaw *) scatFormator_)->_term_schred = atoi(argv[2]);
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "BrAlgo")) {
	    if (!strcasecmp(argv[2], "DRP")) {
		if (lmp_->rpScheduler) {
		    delete lmp_->rpScheduler;
		}
		lmp_->rpScheduler = new DichRP(lmp_);
	    } else if (!strcasecmp(argv[2], "DRPB")) {
		if (lmp_->rpScheduler) {
		    delete lmp_->rpScheduler;
		}
		lmp_->rpScheduler = new DRPBcast(lmp_);
	    } else if (!strcasecmp(argv[2], "MRDRP")) {
		if (lmp_->rpScheduler) {
		    delete lmp_->rpScheduler;
		}
		lmp_->rpScheduler = new MultiRoleDRP(lmp_);
	    } else if (!strcasecmp(argv[2], "TDRP")) {
		if (lmp_->rpScheduler) {
		    delete lmp_->rpScheduler;
		}
		lmp_->rpScheduler = new TreeDRP(lmp_);
	    } else if (!strcasecmp(argv[2], "MDRP")) {
		if (lmp_->rpScheduler) {
		    delete lmp_->rpScheduler;
		}
		lmp_->rpScheduler = new MaxDistRP(lmp_);
	    } else if (!strcasecmp(argv[2], "RPHSI")) {
		if (lmp_->rpScheduler) {
		    delete lmp_->rpScheduler;
		}
		lmp_->rpScheduler = new RPHoldSI(lmp_);
	    } else if (!strcasecmp(argv[2], "RPHMI")) {
		if (lmp_->rpScheduler) {
		    delete lmp_->rpScheduler;
		}
		lmp_->rpScheduler = new RPHoldMI(lmp_);
	    } else {
		fprintf(stderr,
			"unknown parameter for BrAlgo [DRP|RPHSI|DRPB|TDRP|MDRP]: %s\n",
			argv[2]);
		return TCL_ERROR;
	    }
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "T_poll_max")) {
	    Baseband::T_poll_max = atoi(argv[2]);
	    if (Baseband::T_poll_max <= 2) {
		fprintf(stderr,
			"Invalid parameter for T_poll_max (>=2): %s\n",
			argv[2]);
		return TCL_ERROR;
	    }
	    return TCL_OK;

	} else if (!strcmp(argv[1], "bnep-connect")) {
	    BTNode *dest = (BTNode *) TclObject::lookup(argv[2]);
	    BNEP::Connection * conn = bnep_->connect(dest->bb_->bd_addr_);
	    tcl.result(conn->cid->_queue->name());
	    return (TCL_OK);

	} else if (!strcmp(argv[1], "get-ifq")) {
	    BTNode *dest = (BTNode *) TclObject::lookup(argv[2]);
	    L2CAPChannel *ch =
		l2cap_->lookupChannel(PSM_BNEP, dest->bb_->bd_addr_);
	    tcl.result(ch->_queue->name());
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "useChannelSyn")) {
	    if (atoi(argv[2]) || !strcmp(argv[2], "yes")) {
		LMP::useReSyn_ = 1;
	    } else {
		LMP::useReSyn_ = 0;
	    }
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "useSynByGod")) {
	    if (atoi(argv[2]) || !strcmp(argv[2], "yes")) {
		Baseband::useSynByGod_ = 1;
	    } else {
		Baseband::useSynByGod_ = 0;
	    }
	    return TCL_OK;

	} else if (!strcmp(argv[1], "set-rate")) {
	    LMP::setRate(atoi(argv[2]));
	    return TCL_OK;

	} else if (!strcmp(argv[1], "test-fh")) {
	    bd_addr_t addr = strtol(argv[2], NULL, 0);
	    bb_->test_fh(addr);
	    return TCL_OK;

	} else if (!strcmp(argv[1], "bnep-disconnect")) {
	    bnep_->disconnect(atoi(argv[2]), 0);	// bd_addr, reason
	    return (TCL_OK);

	} else if (!strcmp(argv[1], "sco-disconnect")) {
	    ScoAgent *ag1 = (ScoAgent *) TclObject::lookup(argv[2]);
	    uchar reason = 0;
	    if (!ag1) {
		fprintf(stderr, "sco-disconnect: unkown Agent:%s\n",
			argv[2]);
		return TCL_ERROR;
	    }
	    lmp_->HCI_Disconnect(ag1->connh, reason);
	    return (TCL_OK);

	} else if (!strncmp(argv[1], "trace-", 6)) {
	    return (setTrace(argv[1], argv[2], 0) ? TCL_OK : TCL_ERROR);
	}

    } else if (argc == 4) {

	if (!strcmp(argv[1], "pagescan")) {
	    lmp_->HCI_Write_Page_Scan_Activity(atoi(argv[2]),
					       atoi(argv[3]));
	    lmp_->reqOutstanding = 1;
	    lmp_->HCI_Write_Scan_Enable(1);
	    return TCL_OK;

	} else if (!strcmp(argv[1], "bnep-connect")) {
	    BTNode *dest = (BTNode *) TclObject::lookup(argv[2]);
	    hdr_bt::packet_type pt = hdr_bt::NotSpecified;
	    hdr_bt::packet_type rpt = hdr_bt::NotSpecified;
	    Queue *ifq = (Queue *) TclObject::lookup(argv[3]);
	    BNEP::Connection * conn =
		bnep_->connect(dest->bb_->bd_addr_, pt, rpt, ifq);
	    tcl.result(conn->cid->_queue->name());

	} else if (!strcmp(argv[1], "bnep-disconnect")) {
	    bnep_->disconnect(atoi(argv[2]), atoi(argv[3]));	// bd_addr, reason
	    return (TCL_OK);

	} else if (!strcmp(argv[1], "sco-disconnect")) {
	    ScoAgent *ag1 = (ScoAgent *) TclObject::lookup(argv[2]);
	    uchar reason = atoi(argv[3]);
	    if (!ag1) {
		fprintf(stderr, "sco-disconnect: unkown Agent:%s\n",
			argv[2]);
		return TCL_ERROR;
	    }
	    // sco_disconnect(ag1, atoi(argv[3]));      // SCOAgent, reason
	    lmp_->HCI_Disconnect(ag1->connh, reason);
	    return (TCL_OK);

	// cmd toroidal-distance x-max y-max
	// if x-max < 0, it is turned off, that is Euclidian distance is used.
	// Please make sure Baseband::toroidal_x_ is the width of the region
	// (x_max) and toroidal_y_ is the height (y_max).
	} else if (!strcasecmp(argv[1], "toroidal-distance")) {
	    Baseband::toroidal_x_ = atof(argv[2]);
	    Baseband::toroidal_y_ = atof(argv[3]);
	    return TCL_OK;

	} else if (!strncmp(argv[1], "trace-", 6)) {
	    int append = 0;
	    if (argv[3][0] == 'a' || argv[3][0] == 'A') {
		append = 1;
	    }
	    return (setTrace(argv[1], argv[2], append) ? TCL_OK :
		    TCL_ERROR);

	} else if (!strcmp(argv[1], "inquiryscan") ||
		   !strcmp(argv[1], "inqscan")) {
	    lmp_->HCI_Write_Inquiry_Scan_Activity(atoi(argv[2]),
						  atoi(argv[3]));
	    lmp_->reqOutstanding = 1;
	    lmp_->HCI_Write_Scan_Enable(2);
	    return (TCL_OK);

	} else if (!strcmp(argv[1], "pktType")) {
	    lmp_->defaultPktType_ = hdr_bt::str_to_packet_type(argv[2]);
	    lmp_->defaultRecvPktType_ =
		hdr_bt::str_to_packet_type(argv[3]);
	    if (lmp_->defaultPktType_ == hdr_bt::Invalid
		|| lmp_->defaultRecvPktType_ == hdr_bt::Invalid) {
		fprintf(stderr, "Invalid packet type.\n");
		return (TCL_ERROR);
	    }
	    return (TCL_OK);

	} else if (!strcmp(argv[1], "inquiry")) {
	    lmp_->reqOutstanding = 1;
	    lmp_->HCI_Inquiry(lmp_->giac_, atoi(argv[2]), atoi(argv[3]));
	    return (TCL_OK);

	} else if (!strcmp(argv[1], "cmd-after-bnep-connect")) {
	    BTNode *dest = (BTNode *) TclObject::lookup(argv[2]);
	    BNEP::Connection * conn =
		bnep_->lookupConnection(dest->bb_->bd_addr_);
	    char *cmd = new char[strlen(argv[3]) + 1];
	    int trmL = 0;
	    if (*argv[3] == '{') {
		trmL++;
		if (argv[3][1] == '{') {
		    trmL++;
		}
	    }
	    strcpy(cmd, argv[3] + trmL);
	    cmd[strlen(argv[3]) - 2 * trmL] = '\0';

	    conn->setCmd(cmd);
	    fprintf(stderr, "==== nsCmd:%s\n", cmd);
	    // dump_str(argv[3]);
	    // ch->setCmd("$cbr0 start\n");
	    return TCL_OK;

	} else if (!strcmp(argv[1], "set-prio-level")) {
	    BTNode *dest = (BTNode *) TclObject::lookup(argv[2]);
	    bb_->set_prio(dest->bb_->bd_addr_, atoi(argv[3]));
	    return TCL_OK;

	} else if (!strcmp(argv[1], "set-ifq")) {
	    BTNode *dest = (BTNode *) TclObject::lookup(argv[2]);
	    L2CAPChannel *ch =
		l2cap_->lookupChannel(PSM_BNEP, dest->bb_->bd_addr_);
	    // TODO: need to clean up the old queue??
	    ch->_queue = (Queue *) TclObject::lookup(argv[3]);
	    return TCL_OK;

	} else if (!strcmp(argv[1], "pos")) {
	    double x = atof(argv[2]);
	    double y = atof(argv[3]);
	    bb_->setPos(x, y);
	    fprintf(stderr, "Node %d new pos (%f, %f)\n", address(), x, y);
	    return TCL_OK;

	} else if (!strcmp(argv[1], "add-rtenty")) {
	    ragent_->addRtEntry(atoi(argv[2]), atoi(argv[3]), 0);
	    return (TCL_OK);

	} else if (!strcmp(argv[1], "switch-role")) {
	    BTNode *peer = (BTNode *) TclObject::lookup(argv[2]);
	    if (!peer) {
		fprintf(stderr,
			"cmd switch-role, can't get peer node: %s",
			argv[2]);
		return TCL_ERROR;
	    }
	    int tobemaster;
	    if (!strcmp(argv[3], "master")) {
		tobemaster = 0;
	    } else if (!strcmp(argv[3], "slave")) {
		tobemaster = 1;
	    } else {
		fprintf(stderr,
			"unknown parameter: %s. switch-role node [master|slave]\n",
			argv[3]);
		return TCL_ERROR;
	    }

	    lmp_->HCI_Switch_Role(peer->bb_->bd_addr_, tobemaster);
	    return TCL_OK;
	}

    } else if (argc == 5) {

	if (!strcmp(argv[1], "setdest")) {
	    double x = atof(argv[2]);
	    double y = atof(argv[3]);
	    double s = atof(argv[4]);
	    bb_->setdest(x, y, s);
	    return (TCL_OK);

	} else if (!strcmp(argv[1], "pos")) {
	    double x = atof(argv[2]);
	    double y = atof(argv[3]);
	    double z = atof(argv[4]);
	    bb_->setPos(x, y, z);
	    return (TCL_OK);

	} else if (!strcmp(argv[1], "bnep-connect")) {
	    BTNode *dest = (BTNode *) TclObject::lookup(argv[2]);
	    hdr_bt::packet_type pt = hdr_bt::str_to_packet_type(argv[3]);
	    hdr_bt::packet_type rpt = hdr_bt::str_to_packet_type(argv[4]);
	    BNEP::Connection * conn =
		bnep_->connect(dest->bb_->bd_addr_, pt, rpt);
	    tcl.result(conn->cid->_queue->name());
	    return (TCL_OK);

	} else if (!strcmp(argv[1], "sco-connect")) {
	    BTNode *dest = (BTNode *) TclObject::lookup(argv[2]);
	    ScoAgent *ag1 = (ScoAgent *) TclObject::lookup(argv[3]);
	    ScoAgent *ag2 = (ScoAgent *) TclObject::lookup(argv[4]);
	    sco_connect(dest, ag1, ag2);
	    return (TCL_OK);

	    // set up piconet by bypast the connection set up process
	    // master join slave toslavePkt toMasterPkt
	} else if (!strcmp(argv[1], "join")) {
	    BTNode *slave = (BTNode *) TclObject::lookup(argv[2]);
	    hdr_bt::packet_type pt = hdr_bt::str_to_packet_type(argv[3]);
	    hdr_bt::packet_type rpt = hdr_bt::str_to_packet_type(argv[4]);
	    // join(slave, pt, rpt);
	    bnep_join(slave, pt, rpt);
	    return (TCL_OK);
	}

    } else if (argc == 6) {
	if (!strcmp(argv[1], "range")) {
	    double x1 = atof(argv[2]);
	    double y1 = atof(argv[3]);
	    double x2 = atof(argv[4]);
	    double y2 = atof(argv[5]);
	    if (x1 == 0 && y1 == 0 && x2 == 0 && y2 == 0) {
		// fprintf(stderr, "range cmd is not in effect: all 0's.\n");
		return (TCL_OK);
	    }
	    bb_->x1_range = x1;
	    bb_->y1_range = y1;
	    bb_->x2_range = x2;
	    bb_->y2_range = y2;

	    bb_->X_ = Random::uniform() * (x2 - x1) + x1;
	    bb_->Y_ = Random::uniform() * (y2 - y1) + y1;

#ifdef BTDEBUG
	    fprintf(stderr,
		    "Set range[(%f,%f), (%f,%f)]. Reposition to (%f, %f)\n",
		    x1, y1, x2, y2, bb_->X_, bb_->Y_);
#endif

	    return (TCL_OK);

	} else if (!strcmp(argv[1], "bnep-connect")) {
	    BTNode *dest = (BTNode *) TclObject::lookup(argv[2]);
	    hdr_bt::packet_type pt = hdr_bt::str_to_packet_type(argv[3]);
	    hdr_bt::packet_type rpt = hdr_bt::str_to_packet_type(argv[4]);
	    Queue *ifq = (Queue *) TclObject::lookup(argv[5]);
	    BNEP::Connection * conn =
		bnep_->connect(dest->bb_->bd_addr_, pt, rpt, ifq);
	    tcl.result(conn->cid->_queue->name());
	    return (TCL_OK);
	}

    } else if (argc == 8) {

	if (!strcmp(argv[1], "setup")) {
	    uint32_t addr = atoi(argv[2]);

	    /* all devices are initiated as slaves. Anyway, the role changes
	     * dynamically, since the one who does inquiry, paging, 
	     * becomes master.  The one who does inquiry scan and page scan
	     * becomes slave.
	     */

	    Baseband *bb = (Baseband *) TclObject::lookup(argv[3]);
	    LMP *lmp = (LMP *) TclObject::lookup(argv[4]);
	    L2CAP *l2cap = (L2CAP *) TclObject::lookup(argv[5]);
	    BNEP *bnep = (BNEP *) TclObject::lookup(argv[6]);
	    SDP *sdp = (SDP *) TclObject::lookup(argv[7]);

	    setup(addr, bb, lmp, l2cap, bnep, sdp);

	    return (TCL_OK);
	}

    } else if (argc == 9) {

	if (!strcmp(argv[1], "bnep-qos-setup")) {
	    BTNode *dest = (BTNode *) TclObject::lookup(argv[2]);
	    L2CAPChannel *ch = bnep_->lookupChannel(dest->bb_->bd_addr_);
	    uint8_t Flags = atoi(argv[3]);
	    uint8_t Service_Type = atoi(argv[4]);
	    int Token_Rate = atoi(argv[5]);
	    int Peak_Bandwidth = atoi(argv[6]);
	    int Latency = atoi(argv[7]);
	    int Delay_Variation = atoi(argv[8]);

	    ch->setQosParam(new QosParam(Flags, Service_Type, Token_Rate,
					 Peak_Bandwidth, Latency,
					 Delay_Variation));
	    return (TCL_OK);
	}
    }
    return Node::command(argc, argv);
}

void BTNode::setup(uint32_t addr, Baseband * bb, LMP * lmp,
		   L2CAP * l2cap, BNEP * bnep, SDP * sdp)
{
    bb_ = bb;
    lmp_ = lmp;
    l2cap_ = l2cap;
    bnep_ = bnep;
    sdp_ = sdp;

    lmp_->setup(addr, bb, l2cap, this);
    l2cap_->setup(addr, lmp, bnep, sdp, this);
    bnep_->setup(addr, lmp, l2cap, sdp, this);
    sdp_->setup(addr, lmp, l2cap, this);
}

void BTNode::addScoAgent(ScoAgent * a)
{
    a->next = _agent;
    _agent = a;
}

void BTNode::sco_connect(BTNode * dest, ScoAgent * ag_here,
			 ScoAgent * ag_dest)
{
    addScoAgent(ag_here);
    dest->addScoAgent(ag_dest);
    uint32_t addr = dest->bb_->bd_addr_;
    ag_here->daddr = addr;
    ag_dest->daddr = bb_->bd_addr_;
    ag_here->lmp_ = lmp_;
    ag_dest->lmp_ = dest->lmp_;

    if (!lmp_->addReqAgent(ag_here)) {
	fprintf(stderr, "BTNode::sco_connect(): sco req pending.\n");
	exit(-1);
    }
    if (!dest->lmp_->addReqAgent(ag_dest)) {
	fprintf(stderr, "BTNode::sco_connect(): sco req pending.\n");
	exit(-1);
    }

    ConnectionHandle *connh = l2cap_->lookupConnectionHandle(addr);

    if (!connh) {
	Bd_info *bd;
	if ((bd = lmp_->lookupBdinfo(addr)) == NULL) {
	    bd = new Bd_info(addr, 0);
	}
	connh =
	    lmp_->HCI_Create_Connection(addr, hdr_bt::DH1, bd->sr,
					bd->page_scan_mode, bd->offset,
					lmp_->allowRS_);
    }
    ag_here->connh =
	lmp_->HCI_Add_SCO_Connection(connh,
				     (hdr_bt::packet_type) ag_here->
				     packetType_);
}

int BTNode::masterLink(bd_addr_t rmt)
{
    ConnectionHandle *connh = l2cap_->lookupConnectionHandle(rmt);
    return (connh->isMaster());
}

void BTNode::linkDetached(bd_addr_t addr, uchar reason)
{
    if (scatFormator_) {
	scatFormator_->linkDetached(addr, reason);
    }
}

void BTNode::set_mobilenode(MobileNode * m)
{
    wifi_ = m;
}

void BTNode::printStat()
{
    fprintf(BtStat::logstat, "\n*** Stat for node: %d\n", bb_->bd_addr_);
    bb_->dumpEnergy();
    bb_->dumpTtlSlots();
    if (stat_) {
	stat_->dump();
    }
}

void BTNode::printAllStat()
{
    Scheduler & s = Scheduler::instance();
    // double now = s.clock();

    Baseband *wk = bb_;

#if 1
    do {
	wk->node_->printStat();
    } while ((wk = wk->_next) != bb_);
    fprintf(BtStat::logstat, "\n");
#endif

    if (stat_ && (stat_->trace_all_stat2_ || stat_->trace_me_stat2_)) {
	fprintf(BtStat::logstat2, "\n");
    }

    double at_sl_ld = 0;
    int nturn_sl_ld = 0;
    double at_sl = 0;
    int nturn_sl = 0;

    double at_br_ld = 0;
    int nturn_br_ld = 0;
    double at_br = 0;
    int nturn_br = 0;

    double at_ma_ld = 0;
    int nturn_ma_ld = 0;
    double at_ma = 0;
    int nturn_ma = 0;


    double duty_sl_load = 0;
    int duty_sl_num_load = 0;
    double duty_br_load = 0;
    int duty_br_num_load = 0;
    double duty_ma_load = 0;
    int duty_ma_num_load = 0;

    double duty_sl = 0;
    int duty_sl_num = 0;
    double duty_br = 0;
    int duty_br_num = 0;
    double duty_ma = 0;
    int duty_ma_num = 0;

    int pktmiscnt = 0;
    int pktcnt = 0;
    double tt;			// total time

    wk = bb_;
    do {

	wk->node_->bnep_->dump_deliveryRate(BtStat::logstat);

	if (wk->node_->bnep_->_isDst) {
	    for (int f = 0; f < wk->node_->bnep_->_max_dst_port; f++) {
		pktmiscnt += wk->node_->bnep_->_flow_pkt_miscnt[f];
		pktcnt += wk->node_->bnep_->_dst_flow_seq[f];
		if (stat_
		    && (stat_->trace_all_stat3_
			|| stat_->trace_me_stat3_)) {
		    fprintf(BtStat::logstat3, "%d %d ",
			    wk->node_->bnep_->_flow_pkt_miscnt[f],
			    wk->node_->bnep_->_dst_flow_seq[f]);
		}
	    }
	}

	tt = s.clock() - wk->startTime_;
	if (tt == 0) {
	    continue;
	}
	if (wk->lmp_->masterPico) {
	    if (wk->node_->bnep_->_hasTraffic) {
		duty_ma_load +=
		    (wk->activeTime_ +
		     wk->numTurnOn_ * wk->warmUpTime_) / tt;
		at_ma_ld += wk->activeTime_ / tt;
		nturn_ma_ld += int (wk->numTurnOn_ / tt + 0.5);
		duty_ma_num_load++;
	    } else {
		duty_ma +=
		    (wk->activeTime_ +
		     wk->numTurnOn_ * wk->warmUpTime_) / tt;
		at_ma += wk->activeTime_ / tt;
		nturn_ma += int (wk->numTurnOn_ / tt + 0.5);
		duty_ma_num++;
	    }
	} else if (wk->lmp_->num_piconet > 1) {
	    if (wk->node_->bnep_->_hasTraffic) {
		duty_br_load +=
		    (wk->activeTime_ +
		     wk->numTurnOn_ * wk->warmUpTime_) / tt;
		at_br_ld += wk->activeTime_ / tt;
		nturn_br_ld += int (wk->numTurnOn_ / tt + 0.5);
		duty_br_num_load++;
	    } else {
		duty_br +=
		    (wk->activeTime_ +
		     wk->numTurnOn_ * wk->warmUpTime_) / tt;
		at_br += wk->activeTime_ / tt;
		nturn_br += int (wk->numTurnOn_ / tt + 0.5);
		duty_br_num++;
	    }
	} else {
	    if (wk->node_->bnep_->_hasTraffic) {
		duty_sl_load +=
		    (wk->activeTime_ +
		     wk->numTurnOn_ * wk->warmUpTime_) / tt;
		at_sl_ld += wk->activeTime_ / tt;
		nturn_sl_ld += int (wk->numTurnOn_ / tt + 0.5);
		duty_sl_num_load++;
	    } else {
		duty_sl +=
		    (wk->activeTime_ +
		     wk->numTurnOn_ * wk->warmUpTime_) / tt;
		at_sl += wk->activeTime_ / tt;
		nturn_sl += int (wk->numTurnOn_ / tt + 0.5);
		duty_sl_num++;
	    }
	}
    } while ((wk = wk->_next) != bb_);

    double pktDelvrRatio =
	(pktcnt ? (1 - double (pktmiscnt) / pktcnt) : 0);
    fprintf(BtStat::logstat, "Packet Delivery Rate: %f %d %d\n\n",
	    pktDelvrRatio, pktmiscnt, pktcnt);

    fprintf(BtStat::logstat,
	    "ec MA: %f %f %f  BR: %f %f %f  SL: %f %f %f\n\n",
	    (duty_ma_num_load ? duty_ma_load / duty_ma_num_load : 0),
	    (duty_ma_num ? duty_ma / duty_ma_num : 0),
	    (duty_ma_num_load + duty_ma_num ?
	     (duty_ma_load + duty_ma) / (duty_ma_num_load +
					 duty_ma_num) : 0),
	    (duty_br_num_load ? duty_br_load / duty_br_num_load : 0),
	    (duty_br_num ? duty_br / duty_br_num : 0),
	    (duty_br_num_load +
	     duty_br_num ? (duty_br_load + duty_br) / (duty_br_num_load +
						       duty_br_num) : 0),
	    (duty_sl_num_load ? duty_sl_load / duty_sl_num_load : 0),
	    (duty_sl_num ? duty_sl / duty_sl_num : 0),
	    (duty_sl_num_load +
	     duty_sl_num ? (duty_sl_load + duty_sl) / (duty_sl_num_load +
						       duty_sl_num) : 0)
	);

    bnep_->dump_delay(BtStat::logstat);

    // suppose tt is the same for everyone.
    if (stat_ && (stat_->trace_all_stat3_ || stat_->trace_me_stat3_)) {
	fprintf(BtStat::logstat3,
		"| %f %d %d %f %d %d %f %d %d %f %d %d %f %d %d %f %d %d | ",
		at_ma_ld, nturn_ma_ld, duty_ma_num_load, at_ma,
		nturn_ma, duty_ma_num, at_br_ld, nturn_br_ld,
		duty_br_num_load, at_br, nturn_br, duty_br_num, at_sl_ld,
		nturn_sl_ld, duty_sl_num_load, at_sl, nturn_sl,
		duty_sl_num);
/*
    if (stat_ && (stat_->trace_all_stat3_ || stat_->trace_me_stat3_)) {
	fprintf(BtStat::logstat3, "| %f %d %f %d %f %d %f %d %f %d %f %d | ",
		duty_ma_load, duty_ma_num_load,
		duty_ma, duty_ma_num,
		duty_br_load, duty_br_num_load,
		duty_br, duty_br_num,
		duty_sl_load, duty_sl_num_load,
		duty_sl, duty_sl_num);
*/
	bnep_->dump_delay_raw(BtStat::logstat3);
	fprintf(BtStat::logstat3, "\n");
    }

    if (scatFormator_) {
	scatFormator_->dumpTopo();
    }
}

void BTNode::energyResetAllNodes()
{
    Baseband *wk = bb_;
    do {
	wk->energyReset();
    } while ((wk = wk->_next) != bb_);
}

void BTNode::setall_scanwhenon(int v)
{
    Baseband *wk = bb_;
    do {
	wk->node_->lmp_->scanWhenOn_ = v;
    } while ((wk = wk->_next) != bb_);
}

BTNode *BTNode::lookupNode(bd_addr_t n)
{
    Baseband *wk = bb_;

    do {
	if (wk->bd_addr_ == n) {
	    return wk->node_;
	}
    } while ((wk = wk->_next) != bb_);

    return NULL;
}

void BTNode::force_on()
{
    Scheduler & s = Scheduler::instance();
    if (!lmp_->_on) {
	lmp_->_on = 1;
	// checkLink();
	bb_->on();
	// bb_->clkn_ = (bb_->clkn_ & 0xFFFFFFFC) + 3;
	int ntick = (bb_->clkn_ & 0x03);
	double nextfr = bb_->clkn_ev.time_;
	bb_->t_clkn_00 = nextfr - bb_->Tick * (ntick + 1);
	while (bb_->t_clkn_00 > s.clock()) {
	    bb_->t_clkn_00 -= bb_->SlotTime * 2;
	}
/*
	bb_->clkn_ = (bb_->clkn_ & 0xFFFFFFFC) + 3;
	bb_->t_clkn_00 = nextfr - bb_->SlotTime * 2;
*/
    }
}

void BTNode::forceSuspendCurPico()
{
    Scheduler & s = Scheduler::instance();
    LMPLink *wk = lmp_->curPico->activeLink;
    if (wk) {
	int numLink = lmp_->curPico->numActiveLink;
	for (int i = 0; i < numLink; i++) {
	    if (wk->_in_sniff) {
		if (wk->_sniff_ev_to->uid_ > 0) {
		    s.cancel(wk->_sniff_ev_to);
		}
		wk->handle_sniff_suspend();
	    } else {
		wk->force_oneway_hold(200);
	    }
	    wk = wk->next;
	}
    } else {
	lmp_->suspendCurPiconet();
    }
}

// Bypass any message tx/rx, page/pagescan, to have bnep Conn ready.
void BTNode::bnep_join(BTNode * slave, hdr_bt::packet_type pt,
		       hdr_bt::packet_type rpt)
{
    if (pt < hdr_bt::NotSpecified) {
	lmp_->defaultPktType_ = pt;
	slave->lmp_->defaultRecvPktType_ = pt;
    }
    if (rpt < hdr_bt::NotSpecified) {
	lmp_->defaultRecvPktType_ = rpt;
	slave->lmp_->defaultPktType_ = rpt;
    }

    Scheduler & s = Scheduler::instance();

    // Make sure the 2 nodes are on and active.
    force_on();
    slave->force_on();
    if (slave->bb_->_insleep) {
	slave->bb_->wakeupClkn();
    }
    if (bb_->_insleep) {
	bb_->wakeupClkn();
    }
    // suspend curPico for the slave, if necessary.
    if (slave->lmp_->curPico) {
	slave->forceSuspendCurPico();
    }
    // suspend curPico for the master, if necessary.
    if (lmp_->curPico && lmp_->curPico != lmp_->masterPico) {
	forceSuspendCurPico();
    }
    // compute slotoffset and clkoffset
    double t;
    int sclkn = slave->bb_->clkn_;
    double sclkn_00 = slave->bb_->t_clkn_00;
/*
    if ((t =
	 (s.clock() - slave->bb_->t_clkn_00)) >=
	slave->bb_->SlotTime * 2) {
	int nc = int (t / (bb_->SlotTime * 2));
	sclkn_00 += slave->bb_->SlotTime * 2 * nc;
	sclkn += (nc * 4);
    }
*/
    int slotoffset =
	(int) ((bb_->t_clkn_00 + BT_CLKN_CLK_DIFF - sclkn_00) * 1E6);
    while (slotoffset < 0) {
	slotoffset += 1250;
    }
    while (slotoffset >= 1250) {
	slotoffset -= 1250;
    }

/*
    if (slotoffset <= -1250 || slotoffset >= 1250) {
	fprintf(stderr, "slotoffset is %d %f %f %f %f\n",
		slotoffset, s.clock(),
		bb_->t_clkn_00, sclkn_00, slave->bb_->t_clkn_00);
	abort();
    }
*/
    int clkoffset = ((int) (bb_->clkn_ & 0xFFFFFFFC)
		     - (int) (sclkn & 0xFFFFFFFC));

    Bd_info *slaveinfo =
	lmp_->_add_bd_info(new Bd_info(slave->bb_->bd_addr_, clkoffset));
    Bd_info *masterinfo =
	slave->lmp_->_add_bd_info(new Bd_info(bb_->bd_addr_, clkoffset));
    ConnectionHandle *connh = new ConnectionHandle(pt, rpt);
    ConnectionHandle *sconnh = new ConnectionHandle(rpt, pt);

    masterinfo->lt_addr = slaveinfo->lt_addr =
	lmp_->get_lt_addr(slave->bb_->bd_addr_);
    slave->lmp_->_my_info->lt_addr = slaveinfo->lt_addr;

    Piconet *pico = lmp_->masterPico;
    LMPLink *mlink;
    if (pico) {
	mlink = pico->add_slave(slaveinfo, lmp_, connh);
    } else {
	mlink = lmp_->add_piconet(slaveinfo, 1, connh);
    }
    mlink->txBuffer->reset_seqn();
    mlink->piconet->slot_offset = 0;
    mlink->piconet->clk_offset = 0;

    LMPLink *slink = slave->lmp_->add_piconet(masterinfo, 0, sconnh);
    // slink->lt_addr = mlink->lt_addr = slaveinfo->lt_addr;
    slink->txBuffer->reset_seqn();
    slink->piconet->slot_offset = slotoffset;
    slink->piconet->clk_offset = clkoffset;

    mlink->pt = pt;
    mlink->rpt = rpt;
    slink->rpt = pt;
    slink->pt = rpt;

    // start master's clk
    lmp_->curPico = lmp_->masterPico;
    bb_->setPiconetParam(lmp_->masterPico);
    if (bb_->clk_ev.uid_ > 0) {
	s.cancel(&bb_->clk_ev);
    }
    t = s.clock() - bb_->t_clkn_00;
    if (t < BT_CLKN_CLK_DIFF) {
	bb_->clk_ = bb_->clkn_ - 2;
	t = BT_CLKN_CLK_DIFF - t;
    } else {
	t = bb_->SlotTime * 2 + BT_CLKN_CLK_DIFF - t;
	bb_->clk_ = (bb_->clkn_ & 0xfffffffc) + 2;
    }
    if (bb_->clk_ & 0x01) {
	fprintf(stderr, "clkn:%d clk:%d t:%f\n", bb_->clkn_, bb_->clk_, t);
	abort();
    }
    s.schedule(&bb_->clk_handler, &bb_->clk_ev, t);

    // start slave's clk
    slave->lmp_->curPico = slink->piconet;
    slave->bb_->setPiconetParam(slink->piconet);
    if (slave->bb_->clk_ev.uid_ > 0) {
	s.cancel(&slave->bb_->clk_ev);
    }
    slave->bb_->clk_ = bb_->clk_;
    s.schedule(&slave->bb_->clk_handler, &slave->bb_->clk_ev, t);

    // lmp_->link_setup(connh);
    connh->_ready = 1;
    sconnh->_ready = 1;
    slink->connected = 1;
    mlink->connected = 1;

    // set up L2CAPChannel
    L2CAPChannel *ch = setupL2CAPChannel(connh, slave->bb_->bd_addr_, 0);
    L2CAPChannel *sch =
	slave->setupL2CAPChannel(sconnh, bb_->bd_addr_, ch);
    ch->_rcid = sch;

    // set up BNEP Connection    
    BNEP::Connection * bc = bnep_->addConnection(ch);
    BNEP::Connection * sbc = slave->bnep_->addConnection(sch);
    bnep_->_br_table.add(bc->daddr, bc->port);
    slave->bnep_->_br_table.add(sbc->daddr, sbc->port);
    bc->_ready = 1;
    sbc->_ready = 1;

    // start SNIFF negotiation.
    if (slave->lmp_->rpScheduler && (slave->lmp_->lowDutyCycle_ ||
				     slave->lmp_->suspendPico)) {
	slave->lmp_->rpScheduler->start(slink);
#if 0
    } else if (lmp_->rpScheduler && (lmp_->lowDutyCycle_ ||
				     lmp_->suspendPico)) {
#endif
    } else if (lmp_->rpScheduler && lmp_->suspendPico) {
	lmp_->rpScheduler->start(mlink);
    }
}

L2CAPChannel *BTNode::setupL2CAPChannel(ConnectionHandle * connh,
					bd_addr_t rmt, L2CAPChannel * rcid)
{
    L2CAPChannel *ch = new L2CAPChannel(l2cap_, PSM_BNEP, connh, rcid);
    ch->_bd_addr = rmt;
    connh->recv_packet_type = lmp_->defaultRecvPktType_;
    l2cap_->addConnectionHandle(connh);
    // ch->_connhand->add_channel(ch);
    ch->_connhand->reqCid = ch;
    l2cap_->registerChannel(ch);
    // connh->_ready = 1;
    ch->_ready = 1;
    ch->_rcid = rcid;
    return ch;
}

void BTNode::flushPkt(bd_addr_t addr)
{
    L2CAPChannel *ch = l2cap_->lookupChannel(PSM_BNEP, addr);
    if (ch) {
	ch->flush();
    }
}
