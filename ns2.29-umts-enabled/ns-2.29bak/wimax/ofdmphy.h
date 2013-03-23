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

#ifndef OFDMPHY_H
#define OFDMPHY_H

#include <packet.h>
#include "wireless-phy.h"

/* Define subcarrier information */
#define NFFT 256
#define NUSED 200 //number of subcarrier used

/** Status of physical layer */
enum Ofdm_phy_state {
  OFDM_IDLE,  /* Module is not doing anything */
  OFDM_SEND,  /* Module is ready to send or sending */
  OFDM_RECV,  /* Module is can receive or is receiving */
  OFDM_RX2TX, /* Module is transitioning from receiving mode to sending mode */
  OFDM_TX2RX  /* Module is transitioning from sending mode to receiving mode */
};

/** Definition of supported rate */
enum Ofdm_mod_rate {
  OFDM_BPSK_1_2,   /* Efficiency is 1 bps/Hz */
  OFDM_QPSK_1_2,   /* Efficiency is 2 bps/Hz */
  OFDM_QPSK_3_4,   /* Efficiency is 2 bps/Hz */
  OFDM_16QAM_1_2,  /* Efficiency is 4 bps/Hz */
  OFDM_16QAM_3_4,  /* Efficiency is 4 bps/Hz */
  OFDM_64QAM_2_3,  /* Efficiency is 6 bps/Hz */
  OFDM_64QAM_3_4,  /* Efficiency is 6 bps/Hz */
};

/**
 * How to compute the number of information bit per symbol:
 * - Each symbol has 192 data subcarrier (200-8 for pilots)
 * - A modulation has a coding rate (1/2, 2/3, or 3/4)
 * - A modulation has an efficiency (1, 2, 4, or 6)
 * - There is a 0x00 tail byte at the end of each OFDM symbol
 * So for BPSK, 192*1*1/2-8=88
 */
enum Ofdm_bit_per_symbol {
  OFDM_BPSK_1_2_bpsymb = 88,   
  OFDM_QPSK_1_2_bpsymb = 184,   
  OFDM_QPSK_3_4_bpsymb = 280,   
  OFDM_16QAM_1_2_bpsymb = 376,  
  OFDM_16QAM_3_4_bpsymb = 578,  
  OFDM_64QAM_2_3_bpsymb = 760,  
  OFDM_64QAM_3_4_bpsymb = 856,  
};

/** 
 * Class OFDMPhy
 * Physical layer implementing OFDM
 */ 
class OFDMPhy : public WirelessPhy {

public:
  OFDMPhy();

  /**
   * Change the frequency at which the phy is operating
   * @param freq The new frequency
   */
  void setFrequency (double freq);

  /**
   * Set the new modulation for the physical layer
   * @param modulation The new physical modulation
   */
  void  setModulation (Ofdm_mod_rate modulation);    
  
  /**
   * Return the current modulation
   */
  Ofdm_mod_rate  getModulation ();
    
  /**
   * Set the new transmitting power
   * @param power The new transmitting power
   */
  void  setTxPower (double power);
  
  /**
   * Return the current transmitting power
   */
  double  getTxPower ();
  
  /**
   * Return the duration of a PS (physical slot), unit for allocation time.
   * Use Frame duration / PS to find the number of available slot per frame
   */
  inline double  getPS () { return (4/fs_); }
    
  /**
   * Return the OFDM symbol duration time
   */
  double getSymbolTime ();

  /**
   * Compute the transmission time for a packet of size sdusize and
   * using the given modulation
   */
  double getTrxTime (int, Ofdm_mod_rate);

  /**
   * Return the maximum size in bytes that can be sent for the given 
   * nb of symbols and modulation
   */
  int getMaxPktSize (double nbsymbols, Ofdm_mod_rate);

  /**
   * Return the number of PS used by an OFDM symbol
   */
  inline int getSymbolPS () { return (int) (ceil (getSymbolTime() / getPS())); }

  /** 
   * Set the mode for physical layer
   */
  void setMode (Ofdm_phy_state mode);

  /**
   * Activate node
   */
  void node_on ();
  
  /**
   * Deactivate node
   */
  void node_off ();

protected:

  /**
   * Update the sampling frequency. Called after changing frequency BW
   */
  void updateFs ();

  /* 
   * Return the delay required for switching for Rx to Tx mode
   */
  //inline double getRx2TxDelay () { return getPS () * rtg_; }

  /* 
   * Return the delay required for switching for Tx to Rx mode
   */
  //inline double getTx2RxDelay () { return getPS () * ttg_; }


  /* Overwritten methods for handling packets */
  void sendDown(Packet *p);
  int sendUp(Packet *p);

private:

  /**
   * The current modulation
   */
   Ofdm_mod_rate modulation_;
  /**
   * The current transmitting power
   */
   int tx_power_;
  /**
   * Ratio of CP time over useful time
   */
   double g_;
  /**
   * The number of PS required to switch from Receiver to Transmitter
   */
   //int rtg_;
  /**
   * The number of PS required to switch from Transmitter to Receiver
   */
   //int ttg_;

   /**
    * The sampling frequency 
    */
   double fs_;
   
   /**
    * The frequency bandwidth (Hz)
    */
   double fbandwidth_;
   
   /**
    * The state of the OFDM
    */
   Ofdm_phy_state state_;
   
   /**
    * Indicates if the node is activated
    */
   bool activated_;
      
};
#endif //OFDMPHY_H

