/* This software was developed at the National Institute of Standards and
 * Technology by employees of the Federal Government in the course of
 * their official duties. Pursuant to title 17 Section 105 of the United
 * States Code this software is not subject to copyright protection and
 * is in the public domain.
 * NIST assumes no responsibility whatsoever for its use by other parties,
 * and makes no guarantees, expressed or implied, about its quality,
 * reliability, or any other characteristic.
 * <BR>
 * We would appreciate acknowledgement if the software is used.
 * <BR>
 * NIST ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION AND
 * DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING
 * FROM THE USE OF THIS SOFTWARE.
 * </PRE></P>
 * @author  rouil
 */

#include "ofdmphy.h"
#include "mac802_16pkt.h"

//#define DEBUG_WIMAX 1

/**
 * Tcl hook for creating the physical layer
 */
static class OFDMPhyClass: public TclClass {
public:
        OFDMPhyClass() : TclClass("Phy/WirelessPhy/OFDM") {}
        TclObject* create(int, const char*const*) {
                return (new OFDMPhy);
        }
} class_OfdmPhy;

OFDMPhy::OFDMPhy() : WirelessPhy()
{
  //bind attributes
  bind ("g_", &g_);
  //bind ("rtg_", &rtg_);
  //bind ("ttg_", &ttg_);
  //bind ("fbandwidth_", &fbandwidth_);

  //default modulation is BPSK
  modulation_ = OFDM_BPSK_1_2;
  Tcl& tcl = Tcl::instance();
  tcl.evalf("Mac/802_16 set fbandwidth_");
  fbandwidth_ = atof (tcl.result()); 
  state_ = OFDM_IDLE;
  activated_ = true;

  updateFs ();
}

/*
 * Activate node
 */
void OFDMPhy::node_on ()
{
  activated_ = true;
}

/*
 * Deactivate node
 */
void OFDMPhy::node_off ()
{
  activated_ = false;
}


/**
 * Change the frequency at which the phy is operating
 * @param freq The new frequency
 */
void OFDMPhy::setFrequency (double freq)
{
  freq_ = freq;
  lambda_ = SPEED_OF_LIGHT / freq_;
}

/**
 * Set the new modulation for the physical layer
 */
void OFDMPhy::setModulation (Ofdm_mod_rate modulation) {
  modulation_ = modulation;
}
/**
 * Return the current modulation
 */
Ofdm_mod_rate OFDMPhy::getModulation () {
  return modulation_;
}
/**
 * Set the new transmitting power
 */
void OFDMPhy::setTxPower (double power) {
  Pt_ = power;
}
/**
 * Return the current transmitting power
 */
double OFDMPhy::getTxPower () {
  return getPt();
}

/** 
 * Update the PS information
 */
void OFDMPhy::updateFs () {
  /* The PS=4*Fs with Fs=floor (n.BW/8000)*8000
   * and n=8/7 is channel bandwidth multiple of 1.75Mhz
   * n=86/75 is channel bandwidth multiple of 1.5Mhz
   * n=144/125 is channel bandwidth multiple of 1.25Mhz
   * n=316/275 is channel bandwidth multiple of 2.75Mhz
   * n=57/50 is channel bandwidth multiple of 2.0Mhz
   * n=8/7 for all other cases
   */
  double n; 

  if (((int) (fbandwidth_ / 1.75)) * 1.75 == fbandwidth_) {
    n = 8.0/7;
  } else if (((int) (fbandwidth_ / 1.5)) * 1.5 == fbandwidth_) {
    n = 86.0/75;
  } else if (((int) (fbandwidth_ / 1.25)) * 1.25 == fbandwidth_) {
    n = 144.0/125;
  } else if (((int) (fbandwidth_ / 2.75)) * 2.75 == fbandwidth_) {
    n = 316.0/275;
  } else if (((int) (fbandwidth_ / 2.0)) * 2.0 == fbandwidth_) {
    n = 57.0/50;
  } else {
    n = 8.0/7;
  }

  fs_ = floor (n*fbandwidth_/8000) * 8000;
#ifdef DEBUG_WIMAX
  printf ("Fs updated. Bw=%f, n=%f, new value is %e\n", fbandwidth_, n, fs_);
#endif
}

/*
 * Compute the transmission time for a packet of size sdusize and
 * using the given modulation
 * @param sdusize Size in bytes of the data to send
 * @param mod The modulation to use
 */
double OFDMPhy::getTrxTime (int sdusize, Ofdm_mod_rate mod) {
  //we compute the number of symbols required
  int nb_symbols, bpsymb;

  switch (mod) {
  case OFDM_BPSK_1_2:
    bpsymb = OFDM_BPSK_1_2_bpsymb;
    break;
  case OFDM_QPSK_1_2:
    bpsymb = OFDM_QPSK_1_2_bpsymb;
    break;
  case OFDM_QPSK_3_4:
    bpsymb = OFDM_QPSK_3_4_bpsymb;
    break;
  case OFDM_16QAM_1_2:
    bpsymb = OFDM_16QAM_1_2_bpsymb;
    break;
  case OFDM_16QAM_3_4:
    bpsymb = OFDM_16QAM_3_4_bpsymb;
    break;
  case OFDM_64QAM_2_3:
    bpsymb = OFDM_64QAM_2_3_bpsymb;
    break;
  case OFDM_64QAM_3_4:
    bpsymb = OFDM_64QAM_3_4_bpsymb;
    break;
  default:
    printf ("Error: unknown modulation: method getTrxTime in file ofdmphy.cc\n");
    exit (1);
  }

#ifdef DEBUG_WIMAX
  printf ("Nb symbols=%d\n", (int) (ceil(((double)sdusize*8)/bpsymb)));
#endif

  nb_symbols = (int) (ceil(((double)sdusize*8)/bpsymb));
  return (nb_symbols*getSymbolTime ());
}

/*
 * Return the maximum size in bytes that can be sent for the given 
 * nb symbols and modulation
 */
int OFDMPhy::getMaxPktSize (double nbsymbols, Ofdm_mod_rate mod)
{
  int bpsymb;

  switch (mod) {
  case OFDM_BPSK_1_2:
    bpsymb = OFDM_BPSK_1_2_bpsymb;
    break;
  case OFDM_QPSK_1_2:
    bpsymb = OFDM_QPSK_1_2_bpsymb;
    break;
  case OFDM_QPSK_3_4:
    bpsymb = OFDM_QPSK_3_4_bpsymb;
    break;
  case OFDM_16QAM_1_2:
    bpsymb = OFDM_16QAM_1_2_bpsymb;
    break;
  case OFDM_16QAM_3_4:
    bpsymb = OFDM_16QAM_3_4_bpsymb;
    break;
  case OFDM_64QAM_2_3:
    bpsymb = OFDM_64QAM_2_3_bpsymb;
    break;
  case OFDM_64QAM_3_4:
    bpsymb = OFDM_64QAM_3_4_bpsymb;
    break;
  default:
    printf ("Error: unknown modulation: method getTrxTime in file ofdmphy.cc\n");
    exit (1);
  }

  return (int)(nbsymbols*bpsymb)/8;
}

/**
 * Return the OFDM symbol duration time
 */
double OFDMPhy::getSymbolTime () 
{ 
  //printf ("fs=%e, Subcarrier spacing=%e\n", fs_, fs_/((double)NFFT));
  return (1+g_)*((double)NFFT)/fs_; 
}


/*
 * Set the mode for physical layer
 * The Mac layer is in charge of know when to change the state by 
 * request the delay for the Rx2Tx and Tx2Rx
 */
void OFDMPhy::setMode (Ofdm_phy_state mode)
{
  state_ = mode;
}

/* Redefine the method for sending a packet
 * Add physical layer information
 * @param p The packet to be sent
 */
void OFDMPhy::sendDown(Packet *p)
{
  hdr_mac802_16* wph = HDR_MAC802_16(p);

  /* Check phy status */
  if (state_ != OFDM_SEND) {
    printf ("Warning: OFDM not in sending state. Drop packet.\n");
    Packet::free (p);
    return;
  }

#ifdef DEBUG_WIMAX
  printf ("OFDM phy sending packet. Modulation is %d, cyclic prefix is %f\n", 
	  modulation_, g_);
#endif 

  wph->phy_info.freq_ = freq_;
  wph->phy_info.modulation_ = modulation_;
  wph->phy_info.g_ = g_;

  //the packet can be sent
  WirelessPhy::sendDown (p);
}

/* Redefine the method for receiving a packet
 * Add physical layer information
 * @param p The packet to be sent
 */
int OFDMPhy::sendUp(Packet *p)
{
  hdr_mac802_16* wph = HDR_MAC802_16(p);

  if (!activated_)
    return 0;

  if (freq_ != wph->phy_info.freq_) {
#ifdef DEBUG_WIMAX
    printf ("drop packet because frequency is different (%f, %f)\n", freq_,wph->phy_info.freq_);
#endif 
    return 0;
  }

  /* Check phy status */
  if (state_ != OFDM_RECV) {
#ifdef DEBUG_WIMAX
    printf ("Warning: OFDM phy not in receiving state. Drop packet.\n");
#endif 
    return 0;
  }

#ifdef DEBUG_WIMAX
  printf ("OFDM phy receiving packet with mod=%d and cp=%f\n", wph->phy_info.modulation_,wph->phy_info.g_);
#endif 
  
  //the packet can be received
  return WirelessPhy::sendUp (p);
}

