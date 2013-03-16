/*
** Physical_Layer.h :
**
** Low Rate WPAN model in Opnet
** National Institute of Standards and Technology
**
** This model was developed at the National Institute of Standards
** and Technology by employees of the Federal Government in the course
** of their official duties. Pursuant to title 17 Section 105 of the
** United States Code this software is not subject to copyright
** protection and is in the public domain. This is an experimental
** system.  NIST assumes no responsibility whatsoever for its use by
** other parties, and makes no guarantees, expressed or implied,
** about its quality, reliability, or any other characteristic.
**
** We would appreciate acknowledgement if the model is used.
**
** NIST ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION
** AND DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
** RESULTING FROM THE USE OF THIS SOFTWARE.
**
** Primary Author:      Olivier Rebala
** Module description:  Physical Layer for Low Rate WPAN model
** Last Modification:   February, 4, 2004
*/

#ifndef __INTERFERENCE_H__
#define __INTERFERENCE_H__

/* include standards library */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "packet-stamp.h"
#include "physic.h"

/* define some constant */
#define LN_10		2.302585
#define LN_2		0.693147
#define RATIO_TO_DB	4.342944 // 10 / ln(10)
#define	DB_TO_RATIO	0.230258 // ln(10) / 10
#ifndef PI
#define PI		3.141592
#endif
#define SQRT_2PI	2.506628 // square root of 2*pi
#define INFINITY_FREQ	10000.0
#define SIGNAL_MARGIN	10.0
#define LIGHT_SPEED     300000000

/* define some macros */
/* 
 * return -1^k for integer k.  In other words,
 * return 1.0 if k is even, -1.0 if k is odd 
 */
#define MINUS1EXP(k)	(((k&1)==0)?1.0:-1.0)

/* Define a new type for the 802.11b rates */
typedef enum {
  WLAN_MBPS_11,
  WLAN_MBPS_5_5,
  WLAN_MBPS_2,
  WLAN_MBPS_1
} Wlan_Rate;


/* define the packet type */
/*
typedef enum {
	WLAN_PKT_TYPE,
	WPAN_PKT_TYPE,
	BT_MAS_PKT_TYPE,
	BT_SLA_PKT_TYPE	
} pkt_Type;
*/

/* function prototypes */

/* Util functions: file interfutil.cc */
class InterfUtil {
 public:
  static double util_dB (double ratio);
  static double	util_inverse_dB (double dB);
  static double path_loss_attenuation (double separation, double freq);
  static double util_distance (double x1, double y1, double x2, double y2);
};

/* Binomial functions: file binomial.cc */
class Binomial {
 public:
  static double binomial_factorial (int m);
  static int binomial_choose (int m, int j);
};

/* Bit Error Rate function: file ber.cc */
class Ber {
 public:
  static double bit_error_rate_802_15_4 (double sirDB);
  static double bit_error_rate_802_15_1 (double sirDB);
  static double bit_error_rate_802_11b (double sirDB, Wlan_Rate fRate);
};

/* Qerf function: file qerf.cc */
class Qerf {
 public:
  static double qerf_lerp (double x);
  static double qerf_Q (double x);
};

/* Coexistence functions: file interference.cc */
class Interference {
 public:
  /**
   * Main function to call interference
   * @param receiver The receiver node
   * @param pktRx The packet being received at the receiver node
   * @param pktRx_start The time the node started to receive the packet
   * @param pktInt The interfering packet
   * @param pktInt_start The time the interfering packet starts interfering
   */
  static int interference(MobileNode *receiver, Packet *pktRx, double pktRx_start, Packet *pktInt, double pktRx_start);

  static double coexistence_ri(pkt_Type receiver_type, double receiver_freq, double t_power, double t_distance, 
			       pkt_Type interferer_type, double interferer_freq, double i_power, double i_distance);
  
  static double coexistence_15d4r_11bi (double receiver_freq, double t_power, double t_distance,
					double interferer_freq, double i_power, double i_distance);
  static double coexistence_15d4ri (double receiver_freq, double t_power, double t_distance,
				    double interferer_freq, double i_power, double i_distance);
  static double coexistence_15d4r_15d1i (double receiver_freq, double t_power, double t_distance,
					 double interferer_freq, double i_power, double i_distance);
  
  static double coexistence_11br_15d4i (double receiver_freq,	double t_power, double t_distance, Wlan_Rate receiver_rate, 
					double interferer_freq, double i_power, double i_distance);
  static double coexistence_11bri( double receiver_freq,	double t_power, double t_distance, Wlan_Rate receiver_rate, 
				   double interferer_freq, double i_power, double i_distance);
  static double coexistence_11br_15d1i (double receiver_freq,	double t_power, double t_distance, Wlan_Rate receiver_rate, 
					double interferer_freq, double i_power, double i_distance);
  
  static double coexistence_15d1r_11bi (double receiver_freq, double t_power, double t_distance,
					double interferer_freq, double i_power, double i_distance);
  static double coexistence_15d1ri (double receiver_freq, double t_power, double t_distance,
				    double interferer_freq, double i_power, double i_distance);
  static double coexistence_15d1r_15d4i (double receiver_freq, double t_power, double t_distance,
					 double interferer_freq, double i_power, double i_distance);
  
  
  static double coexistence_coupling_15d4r_11bi (double Receiver_Freq, double Interferer_Freq);
  static double coexistence_coupling_15d4ri (double Receiver_Freq, double Interferer_Freq);
  static double coexistence_coupling_15d4r_15d1i (double Receiver_Freq, double Interferer_Freq);
  
  static double coexistence_coupling_11br_15d4i (double Receiver_Freq, double Interferer_Freq);
  static double coexistence_coupling_11bri (double Receiver_Freq, double Interferer_Freq);
  static double coexistence_coupling_11br_15d1i (double Receiver_Freq, double Interferer_Freq);
  
  static double coexistence_coupling_15d1r_11bi (double Receiver_Freq, double Interferer_Freq);
  static double coexistence_coupling_15d1ri (double Receiver_Freq, double Interferer_Freq);
  static double coexistence_coupling_15d1r_15d4i (double Receiver_Freq, double Interferer_Freq);
};

/* Spectrum functions: file Spectrum.cc */
class Spectrum {
 public:
  static void	spectrum_multiply (double ispectrum[][2], int isize, double rspectrum[][2], int rsize, double prod_spectrum[][2]);
  static double	spectrum_find_gain_at (double frequency, double spectrum[][2], int size);
  static double	spectrum_lerp (double x, double x0, double x1, double y0, double y1);
  static double	spectrum_area (double spectrum[][2], int size);
  static double	spectrum_db_trapezoid (double d0, double d1, double width);
  static void	spectrum_sort (double spectrum[][2], int size);
  static void	spectrum_display (double spectrum[][2], int size);
};

#endif /* end of __PHYSICAL_LAYER__ */
