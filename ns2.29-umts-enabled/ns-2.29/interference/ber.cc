/*
** BER.ex.c :
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
** Last Modification:   January, 21, 2004
*/

/* include the header */
#include "interference.h"

/*
 * Function:	bit_error_rate_802_15_4
 *
 * Description:	Given a Signal To Interference Ratio, return the bit error
 *				rate predicted for this receiver.  The ber() function should
 *				take into account the modulation scheme, processing gain and
 *				bits per symbol.
 *
 *				## This routine has been verified to match Paul Gorday's 
 *				## recommendations.  - rdp 6 Aug 2002
 *
 * ParamIn:		double sirDB
 *				Signal To Interference (dB)
 *
 * ParamOut:	double ber
 *				Bit Error rate
 */

double Ber::bit_error_rate_802_15_4 (double sirDB) 
{
  double EbN0, EsN0, ser, sum, ber;
  double sir = InterfUtil::util_inverse_dB (sirDB);
  int k; // loop variable

  const int M = 16;
  const double Rb = 250000;		// bit rate
  const double BW = 1.25e6;		// bandwidth

  
  EbN0 = sir*(BW/Rb);	// conversion from SNR to EbN0
  EsN0 = EbN0*log(M)/LN_2;	// conversion from bit to symbol
  sum = 0.0;
  for (k=2; k<=M; k++)
    sum += MINUS1EXP (k) * Binomial::binomial_choose (M, k) * exp ((EsN0 / (double) k) - EsN0);
  
  ser = sum / (double) M;
  /* convert from symbol error rate (ser) to bit error rate (ber) */
  ber = ser*((double) M / 2.0)/((double) M - 1.0);

  return (ber);
}

/*
 * Function:	bit_error_rate_802_15_1
 *
 * Description:	Given a Signal To Interference Ratio, return the bit error
 *				rate predicted for this receiver.  The ber() function should
 *				take into account the modulation scheme, processing gain and
 *				bits per symbol.
 *
 *
 * ParamIn:		double sirDB
 *				Signal To Interference (dB)
 *
 * ParamOut:	double ber
 *				Bit Error rate
 */

double Ber::bit_error_rate_802_15_1 (double sirDB) 
{
  double ber;
  double sir = InterfUtil::util_inverse_dB (sirDB);
  double temp;	  
  
  /* convert from symbol error rate (ser) to bit error rate (ber) */
  temp = 0.5 * exp( - sir / 2.0);
  if ( temp < 0.5)
	  ber = temp;
  else	
	  ber = 0.5;  
  return (ber);
}



/*
 * Function:	bit_error_rate_802_11b
 *
 * Description:	Given a Signal To Interference Ratio, return the bit error
 *				rate predicted for this receiver.  The ber() function should
 *				take into account the modulation scheme, processing gain and
 *				bits per symbol.
 *
 *				## Adapted from TG2 models.
 *
 * ParamIn:		double sirDB
 *				Signal To Interference (dB)
 *
 * ParamOut:	double ber
 *				Bit Error rate
 */

double Ber::bit_error_rate_802_11b (double sirDB, Wlan_Rate fRate)
{
  double sir = InterfUtil::util_inverse_dB (sirDB);
  double ser, ber;
  int M;

  switch (fRate)
    {
    case WLAN_MBPS_1:
      M = 2;
      ser = Qerf::qerf_Q (sqrt(11.0*sir));
      break;

    case WLAN_MBPS_2:
      M = 2;
      ser = Qerf::qerf_Q (sqrt(5.5*2.0*sir/2.0));
      break;

    case WLAN_MBPS_5_5:
      M = 8;
      ser = 14.0*Qerf::qerf_Q (sqrt(8.0*sir)) + Qerf::qerf_Q (sqrt(16.0*sir));
      break;
    
    case WLAN_MBPS_11:
      M = 256;
      ser = 24.0*Qerf::qerf_Q (sqrt(4.0*sir)) + 16.0*Qerf::qerf_Q (sqrt(6.0*sir)) + 174.0*Qerf::qerf_Q (sqrt(8.0*sir)) +
	16.0*Qerf::qerf_Q (sqrt(10.0*sir)) + 24.0*Qerf::qerf_Q (sqrt(12.0*sir)) + Qerf::qerf_Q (sqrt(16.0*sir));
    }
  
  /* convert from symbol error rate (ser) to bit error rate (ber) */
  ber = ser * ((double) M / 2.0)/ ((double) M - 1.0);

  /* return the minimum beetween the ber value and 0.5 */
  ber = (ber < 0.5)?ber:0.5;

  return (ber);
}
