/*
 * Copyright (c) 2005 Twente Institute for Wireless and Mobile
 *		Communications B.V.
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
 * 3. Neither the name of Ericsson Telecommunicatie B.V. or WMC may be used
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
 * $Id $
 */


#include "random.h"
#include "trafgen.h"
#include "ranvar.h"
#include <stdlib.h>

/* implement an on/off source with average on and off times taken
 * from a pareto distribution.  (enough of these sources multiplexed
 * produces aggregate traffic that is LRD).  It is parameterized
 * by the average burst time, average idle time, burst rate, and
 * pareto shape parameter and packet size.
 */

class COOT_Traffic : public TrafficGenerator {
public:
  COOT_Traffic();
  virtual double next_interval(int&);
  int on()  { return on_ ; }
  int command(int argc, const char*const* argv);
  
protected:
  void init();
  double ontime_;  /* average length of burst (sec) */
  double offtime_; /* average idle period (sec) */
  double rate_;    /* send rate during burst (bps) */
  double interval_; /* inter-packet time at burst rate */
  double burstlen_; /* average # packets/burst */
  double shape_;    /* pareto shape parameter */
  unsigned int rem_; /* number of packets remaining in current burst */
  double p1_;       /* parameter for pareto distribution to compute
		     * number of packets in burst.
		     */
  int on_;          /* denotes whether in the on or off state */
  int coot_ID_; 
  RNG * rng_; /* If the user wants to specify his own RNG object */

  ExponentialRandomVariable Offtime_;

};


static class COOTTrafficClass : public TclClass {
public:
  COOTTrafficClass() : TclClass("Application/Traffic/Coot") {}
  TclObject* create(int, const char*const*) {
    return (new COOT_Traffic());
  }
} class_coot_traffic;


int COOT_Traffic::command(int argc, const char*const* argv){
  
  Tcl& tcl = Tcl::instance();
  if(argc==3){
    if (strcmp(argv[1], "use-rng") == 0) {
      rng_ = (RNG*)TclObject::lookup(argv[2]);
      if (rng_ == 0) {
	tcl.resultf("no such RNG %s", argv[2]);
	return(TCL_ERROR);
      }                        
      return (TCL_OK);
    } else if (strcmp(argv[1], "coot-ID") == 0) {
      coot_ID_ = atoi(argv[2]);
      return (TCL_OK);
    }
  }
  return Application::command(argc,argv);
}


COOT_Traffic::COOT_Traffic() : rng_(NULL)
{
  bind_time("burst_time_", &ontime_);
  bind_time("idle_time_", &offtime_);
  bind_bw("rate_", &rate_);
  bind("shape_", &shape_);
  bind("packetSize_", &size_);
}

void COOT_Traffic::init()
{
  interval_ = (double)(size_ << 3)/(double)rate_;
  burstlen_ = ontime_/interval_;
  rem_ = 0;
  on_ = 0;
  p1_ = burstlen_ * (shape_ - 1.0)/shape_;

// Uncomment if you want TCP packets marked as "coot"
// if (agent_)
//    agent_->set_pkttype(PT_COOT);
}

double COOT_Traffic::next_interval(int& size)
{
  
  double t = interval_;
  
  on_ = 1;
  if (rem_ == 0) {
    /* compute number of packets in next burst */
    if(rng_ == 0){
      rem_ = int(Random::pareto(p1_, shape_) + .5);
    }
    else{
      rem_ = int(rng_->pareto(p1_, shape_) + .5);
    }

    /* make sure we got at least 1 */
    if (rem_ == 0)
      rem_ = 1;	
    
    /* start of an idle period, compute idle time */
    if(rng_ == 0) {
      t += Offtime_.value();
    }
    else {
      t += double(rng_->exponential(offtime_));
    }
		  

    //printf(" AVG IDLE VALUE: %f\n", offtime_);
    //printf("%d\t %f\t %f\t %f\t %d\t %%COOT%d%%\n", coot_ID_, Scheduler::instance().clock(),t, ((rem_-1)*interval_), rem_, coot_ID_); 

    on_ = 0;
    
  }
  rem_--;
  
  size = size_;
  return(t);
   
}





