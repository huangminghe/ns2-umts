/* -*- c++ -*-
   packet-stamp.h
   $Id: packet-stamp.h,v 1.2 2006/03/17 22:08:56 rouil Exp $

   Information carried by a packet to allow a receive to decide if it
   will recieve the packet or not.

*/

#ifndef _cmu_packetstamp_h_
#define _cmu_packetstamp_h_

class MobileNode;
/* to avoid a pretty wild circular dependence among header files
   (between packet.h and queue.h), I can't do the #include here:
   #include <cmu/node.h>
   Since PacketStamp is just a container class, it doesn't really matter .
   -dam 8/8/98
   */
#include <antenna.h>

//NIST:add packet type in stamp
//#include "interference.h"
/* define the packet type */
typedef enum {
  WLAN_PKT_TYPE,
  WPAN_PKT_TYPE,
  BT_PKT_TYPE,
  WIMAX_PKT_TYPE,
  UMTS_PKT_TYPE,

  UNKNOWN_PKT_TYPE
} pkt_Type;


class PacketStamp {
public:

  PacketStamp() : ant(0), node(0), Pr(-1), lambda(-1)/*NIST*/,pkt_type(UNKNOWN_PKT_TYPE)/*end NIST*/ { }

  void init(const PacketStamp *s) {
	  Antenna* ant;
	  if (s->ant != NULL)
		  ant = s->ant->copy();
	  else
		  ant = 0;
	  
	  //Antenna *ant = (s->ant) ? s->ant->copy(): 0;
	  //NIST: add packet type
	  //stamp(s->node, ant, s->Pr, s->lambda); //old
	  stamp(s->node, ant, s->Pr, s->lambda, s->pkt_type);
	  //end NIST
  }

  void stamp(MobileNode *n, Antenna *a, double xmitPr, double lam/*NIST*/, pkt_Type pkt_t/*end NIST*/ ) {
    ant = a;
    node = n;
    Pr = xmitPr;
    lambda = lam;
    //NIST: add packet type
    pkt_type = pkt_t;
    //end NIST
  }

  inline Antenna * getAntenna() {return ant;}
  inline MobileNode * getNode() {return node;}
  inline double getTxPr() {return Pr;}
  inline double getLambda() {return lambda;}
  //NIST: add packet type
  inline pkt_Type getPktType() {return pkt_type;}
  //end NIST

  /* WILD HACK: The following two variables are a wild hack.
     They will go away in the next release...
     They're used by the mac-802_11 object to determine
     capture.  This will be moved into the net-if family of 
     objects in the future. */
  double RxPr;			// power with which pkt is received
  double CPThresh;		// capture threshold for recving interface

protected:
  Antenna       *ant;
  MobileNode	*node;
  double        Pr;		// power pkt sent with
  double        lambda;         // wavelength of signal
  //NIST: add packet type
  pkt_Type pkt_type;
  //end NIST
};

#endif /* !_cmu_packetstamp_h_ */
