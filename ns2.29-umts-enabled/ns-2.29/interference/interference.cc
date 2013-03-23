/*
** Coexistence.ex.c :
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

/* include the header */
#include "interference.h"
#include "packet.h"
#include "mobilenode.h"
#include "random.h"

/**
 * Main function to call interference
 * @param receiver The receiver node
 * @param pktRx The packet being received at the receiver node
 * @param pktRx_start The time the node started to receive the packet
 * @param pktInt The interfering packet
 * @param pktInt_start The time the interfering packet starts interfering
 */
int Interference::interference (MobileNode *receiver, Packet *pktRx, double pktRx_start, Packet *pktInt, double pktInt_start)
{
  double distRT = InterfUtil::util_distance (pktRx->txinfo_.getNode()->X(),
					      pktRx->txinfo_.getNode()->Y(),
					      receiver->X(), receiver->Y());
  double distRI = InterfUtil::util_distance (pktInt->txinfo_.getNode()->X(),
					     pktInt->txinfo_.getNode()->Y(),
					     receiver->X(), receiver->Y());
  double fRT = LIGHT_SPEED / (1000*1000*pktRx->txinfo_.getLambda());
  double fRI = LIGHT_SPEED / (1000*1000*pktInt->txinfo_.getLambda());
  
  double ber = coexistence_ri (pktRx->txinfo_.getPktType(), 
			       fRT, pktRx->txinfo_.getTxPr(), distRT,
			       pktInt->txinfo_.getPktType(), 
			       fRI, pktInt->txinfo_.getTxPr(), distRI);

  //compute overlapping period
  double pktRx_stop = pktRx_start + HDR_CMN(pktRx)->txtime();
  double pktInt_stop = pktInt_start + HDR_CMN(pktInt)->txtime();
  //printf ("%f %f %f\n", ber, pktRx_stop, pktInt_stop);

  double overlapping = 0;
  if (pktRx_start < pktInt_start)
    overlapping = (pktRx_stop < pktInt_stop)? pktRx_stop - pktInt_start : pktInt_stop - pktInt_start; 
  else
    overlapping = (pktInt_stop < pktRx_stop)? pktInt_stop - pktRx_start : pktRx_stop - pktRx_start; 
  
  //printf ("overlapping=%.3fms\n", overlapping*1000);
  //we assume uniform repartition of bits in the message
  //compute the number of bit being that will be interfered
  int bit_int = (int)ceil (HDR_CMN(pktRx)->size()*8*overlapping/HDR_CMN(pktRx)->txtime());

  //compute the number of errors
  int nb_err = 0; 
  for (int i = 0 ; i < bit_int ; i++) {
    if (Random::uniform(0, 1) <= ber)
      nb_err++;
  }

  //try with other method
  //int nb_err2 = Interference2::interference2(pktRx, distRT, pktInt, distRI, pktInt_start-pktRx_start, 1); 
  //printf ("nbErr1=%d nbErr2=%d\n", nb_err, nb_err2);

  return nb_err;
}




/*
 * Function:	coexistence_ri
 *
 * Description:	The coexistence metric: compute bit error rate (BER)
 *				as a function of distance between an interferer
 *				and receiver.
 *
 * ParamIn:		pkt_Type receiver_type
 *				type of the receiver (cf. lr_wpan_support.h)
 *
 *				double receiver_freq
 *		   		Frequency center of the receiver
 *
 *				double t_power
 *				Power of the Transmitter
 *
 *				double t_distance
 *				distance beetween transmitter and receiver
 *
 *				pkt_Type interferer_type
 *				type of the interferer (cf. lr_wpan_support.h)
 *
 *				double interferer_freq
 *				frequency center of the interferer
 *
 *				double i_power
 *				Power of the interferer
 *
 *				double i_distance
 *				distance beetween interferer and receiver
 *
 * ParamOut:	double ber
 *				bit error rate
 */
double Interference::coexistence_ri( pkt_Type receiver_type, double receiver_freq, double t_power, double t_distance, 
	pkt_Type interferer_type, double interferer_freq, double i_power, double i_distance)
	{
	
	double ber = 0.0;
	
 	/*
	printf("*************\n");
	printf("trans type receiv %d\n", 	receiver_type);
	printf("trans freq trans %f\n", receiver_freq);
	printf("trans power trans coex %f\n", t_power);
	printf("trans dist %f\n", t_distance);
	printf("int type receiv %d\n", 	interferer_type);
	printf("int freq trans %f\n", interferer_freq);
	printf("int power trans coex %f\n", i_power);
	printf("int  trans dist %f\n", i_distance);
	*/

	/* compute the BER according to the following cases */
	
	/*receiver is a 802.15.4 device */
	/* if interferer types is 802.15.4 */
	if (receiver_type == WPAN_PKT_TYPE && interferer_type == WPAN_PKT_TYPE)
		ber = coexistence_15d4ri (receiver_freq, t_power, t_distance,
			interferer_freq, i_power, i_distance);
	/* if interferer types is 802.11b */
	if (receiver_type == WPAN_PKT_TYPE && interferer_type == WLAN_PKT_TYPE)
		ber = coexistence_15d4r_11bi (receiver_freq, t_power, t_distance,
			interferer_freq, i_power, i_distance);
	/* if interferer types is 802.15.1 */
    if (receiver_type == WPAN_PKT_TYPE && interferer_type == BT_PKT_TYPE)
		ber = coexistence_15d4r_15d1i (receiver_freq, t_power, t_distance,
			interferer_freq, i_power, i_distance);
	
	/* receiver is a 802.11b device */
	/* if interferer types is 802.11b */
	if (receiver_type == WLAN_PKT_TYPE && interferer_type == WLAN_PKT_TYPE)
		ber = coexistence_11bri(receiver_freq, t_power, t_distance, WLAN_MBPS_11,
			interferer_freq, i_power, i_distance);
	/* if interferer types is 802.15.4 */
	if (receiver_type == WLAN_PKT_TYPE && interferer_type == WPAN_PKT_TYPE)
		ber = coexistence_11br_15d4i (receiver_freq, t_power, t_distance, WLAN_MBPS_11,
			interferer_freq, i_power, i_distance);
	/* if interferer types is 802.15.1 */
	if (receiver_type == WLAN_PKT_TYPE && interferer_type == BT_PKT_TYPE)
		ber = coexistence_11br_15d1i (receiver_freq, t_power, t_distance, WLAN_MBPS_11,
			interferer_freq, i_power, i_distance);
	
	/* receiver is a 802.15.1 device */ 
	/* if interferer types is 802.15.1 */
	if (receiver_type == BT_PKT_TYPE && interferer_type == BT_PKT_TYPE)		
		ber = coexistence_15d1ri (receiver_freq, t_power, t_distance,
			interferer_freq, i_power, i_distance);
	/* if interferer types is 802.15.4 */
	if (receiver_type == BT_PKT_TYPE && interferer_type == WPAN_PKT_TYPE)
		ber = coexistence_15d1r_15d4i (receiver_freq, t_power, t_distance,
			interferer_freq, i_power, i_distance);
	/* if interferer types is 802.11b */
	if (receiver_type == BT_PKT_TYPE && interferer_type == WLAN_PKT_TYPE)
		ber = coexistence_15d1r_11bi (receiver_freq, t_power, t_distance,
			interferer_freq, i_power, i_distance);
	
	return (ber);
}



/*
 * Function:	coexistence_15d4r_11bi
 *
 * Description:	The coexistence metric: compute bit error rate (BER)
 *				as a function of distance between interferer (802.11b)
 *				and receiver (802.15.4).
 *
 * ParamIn:		double receiver_freq
 *		   		Frequency center of the receiver (802.15.4)
 *
 *				double interferer_freq
 *				frequency center of the interferer (802.11b)
 *
 *				double distance
 *				distance beetween interferer and receiver
 *
 * ParamOut:	double ber
 *				bit error rate
 */

double Interference::coexistence_15d4r_11bi (double receiver_freq, double t_power, double t_distance,
			       double interferer_freq, double i_power, double i_distance)
{
  double coupling, interference, signal, sirDB, ber;

  

  /* 
   * Compute the spectrum factor for each combination of transmitter
   * and receiver center frequencies.
   */
  coupling = coexistence_coupling_15d4r_11bi (receiver_freq, interferer_freq);
	
  /* SIGNAL received from the transmitter */
  signal = InterfUtil::util_dB(t_power*1000) -  InterfUtil::path_loss_attenuation (t_distance,receiver_freq/1000);

  /*
   * INTERFERENCE is the interferer's transmit power
   * attenuated by distance and by spectrum coupling.
   */
  interference =  InterfUtil::util_dB(i_power*1000) -  InterfUtil::path_loss_attenuation (i_distance, interferer_freq/1000) + coupling;

  /* SIR is simply the SIGNAL divided by INTERFERENCE */
  sirDB = signal - interference;

  /* Convert to BER according to receiver's modulation scheme */
  ber = Ber::bit_error_rate_802_15_4 (sirDB);

  return (ber);
}


/*
 * Function:	coexistence_15d4ri
 *
 * Description:	The coexistence metric: compute bit error rate (BER)
 *				as a function of distance between interferer and receiver
 * 				(802.15.4).
 *
 * ParamIn:		double receiver_freq
 *		   		Frequency center of the receiver (802.15.4)
 *
 *				double interferer_freq
 *				frequency center of the interferer (802.15.4)
 *
 *				double distance
 *				distance beetween interferer and receiver
 *
 * ParamOut:	double ber
 *				bit error rate
 */

double Interference::coexistence_15d4ri (double receiver_freq, double t_power, double t_distance,
			       double interferer_freq, double i_power, double i_distance)
{
  double coupling, interference, signal, sirDB, ber;

 
  /* 
   * Compute the spectrum factor for each combination of transmitter
   * and receiver center frequencies.
   */
  coupling = coexistence_coupling_15d4ri (receiver_freq, interferer_freq);
	
  /* SIGNAL */
  signal =  InterfUtil::util_dB(t_power*1000) -  InterfUtil::path_loss_attenuation (t_distance,receiver_freq/1000);
  /*
   * INTERFERENCE is the interferer's transmit power
   * attenuated by distance and by spectrum coupling.
   */
  interference =  InterfUtil::util_dB(i_power*1000) -  InterfUtil::path_loss_attenuation (i_distance, interferer_freq/1000) + coupling;
  /* SIR is simply the SIGNAL divided by INTERFERENCE */
  sirDB = signal - interference;

  /* Convert to BER according to receiver's modulation scheme */
  ber = Ber::bit_error_rate_802_15_4 (sirDB);

  return (ber);
}



/*
 * Function:	coexistence_15d4r_15d1i
 *
 * Description:	The coexistence metric: compute bit error rate (BER)
 *				as a function of distance between interferer (802.15.1)
 *				and receiver (802.15.4).
 *
 * ParamIn:		double receiver_freq
 *		   		Frequency center of the receiver (802.15.4)
 *
 *				double interferer_freq
 *				frequency center of the interferer (802.15.1)
 *
 *				double distance
 *				distance beetween interferer and receiver
 *
 * ParamOut:	double ber
 *				bit error rate
 */

double Interference::coexistence_15d4r_15d1i (double receiver_freq, double t_power, double t_distance,
			       double interferer_freq, double i_power, double i_distance)
{
  double coupling, interference, signal, sirDB, ber;

  
  /* 
   * Compute the spectrum factor for each combination of transmitter
   * and receiver center frequencies.
   */
  coupling =  coexistence_coupling_15d4r_15d1i (receiver_freq, interferer_freq);
	
  /* SIGNAL received from the transmitter */
  signal =  InterfUtil::util_dB(t_power*1000) -  InterfUtil::path_loss_attenuation (t_distance,receiver_freq/1000);
  /*
   * INTERFERENCE is the interferer's transmit power
   * attenuated by distance and by spectrum coupling.
   */
  interference =  InterfUtil::util_dB(i_power*1000) -  InterfUtil::path_loss_attenuation (i_distance, interferer_freq/1000) + coupling;

  /* SIR is simply the SIGNAL divided by INTERFERENCE */
  sirDB = signal - interference;

  /* Convert to BER according to receiver's modulation scheme */
  ber = Ber::bit_error_rate_802_15_4 (sirDB);

  return (ber);
}

/*
 * Function:	coexistence_11br_15d4i
 *
 * Description:	The coexistence metric: compute bit error rate (BER)
 *				as a function of distance between interferer (802.15.4)
 *				and receiver (802.11b).
 *
 * ParamIn:		double receiver_freq
 *				Frequency center of the receiver (802.11b)
 *
 *				double interferer_freq
 *				frequency center of the interferer (802.15.4)
 *
 *				Wlan_Rate receiver_rate
 *				rate of the WLAN receiver (11, 5.5, 2 or 1 Mbps)
 *
 *				double distance
 *				distance beetween interferer and receiver
 *
 * ParamOut:	double ber
 *				bit error rate
 */
	
double Interference::coexistence_11br_15d4i (double receiver_freq,	double t_power, double t_distance, Wlan_Rate receiver_rate, 
	double interferer_freq, double i_power, double i_distance)
{
  double coupling, interference, signal, sirDB, ber;

   /* 
   * Compute the spectrum factor for each combination of transmitter
   * and receiver center frequencies.
   */  coupling = coexistence_coupling_11br_15d4i (receiver_freq, interferer_freq);
	
  /* SIGNAL received from the transmitter */
  signal = InterfUtil::util_dB(t_power*1000) - InterfUtil::path_loss_attenuation (t_distance,receiver_freq/1000);

  /*
   * INTERFERENCE is the interferer's transmit power
   * attenuated by distance and by spectrum coupling.
   */
  interference = InterfUtil::util_dB(i_power*1000) - InterfUtil::path_loss_attenuation (i_distance, interferer_freq/1000) + coupling;

  /* SIR is simply the SIGNAL divided by INTERFERENCE */
  sirDB = signal - interference;

  /* Convert to BER according to receiver's modulation scheme */
  ber = Ber::bit_error_rate_802_11b (sirDB, receiver_rate);

  return (ber);
}


/*
 * Function:	coexistence_11bri
 *
 * Description:	The coexistence metric: compute bit error rate (BER)
 *				as a function of distance between interferer and receiver
 *				(802.11b).
 *
 * ParamIn:		double receiver_freq
 *				Frequency center of the receiver
 *
 *				double t_power
 *				Power of the Transmitter
 *
 *				double t_distance
 *				distance beetween transmitter and receiver
 *
 *				double interferer_freq
 *				frequency center of the interferer
 *
 *				Wlan_Rate receiver_rate
 *				rate of the WLAN receiver (11, 5.5, 2 or 1 Mbps)
 *
 *				double i_power
 *				Power of the interferer
 *
 *				double i_distance
 *				distance beetween interferer and receiver
 * 
 * ParamOut:	double ber
 *				bit error rate
 */
double Interference::coexistence_11bri( double receiver_freq,	double t_power, double t_distance, Wlan_Rate receiver_rate, 
	double interferer_freq, double i_power, double i_distance)
{
  double coupling, interference, signal, sirDB, ber;

  
  /* 
   * Compute the spectrum factor for each combination of transmitter
   * and receiver center frequencies.
   */
  coupling = coexistence_coupling_11bri (receiver_freq, interferer_freq);
	
  /* SIGNAL received from the transmitter */
  signal = InterfUtil::util_dB(t_power*1000) - InterfUtil::path_loss_attenuation (t_distance,receiver_freq/1000);
  
  /*
   * INTERFERENCE is the interferer's transmit power
   * attenuated by distance and by spectrum coupling.
   */
  interference = InterfUtil::util_dB(i_power*1000) - InterfUtil::path_loss_attenuation (i_distance, interferer_freq/1000) + coupling;
  
  /* SIR is simply the SIGNAL divided by INTERFERENCE */
  sirDB = signal - interference;
 
  /* Convert to BER according to receiver's modulation scheme */
  ber = Ber::bit_error_rate_802_11b (sirDB, receiver_rate);

  return (ber);
}

/*
 * Function:	coexistence_11br_15d1i
 *
 * Description:	The coexistence metric: compute bit error rate (BER)
 *				as a function of distance between interferer (802.15.1)
 *				and receiver (802.11b).
 *
 * ParamIn:		double receiver_freq
 *				Frequency center of the receiver (802.11b)
 *
 *				double interferer_freq
 *				frequency center of the interferer (802.15.1)
 *
 *				Wlan_Rate receiver_rate
 *				rate of the WLAN receiver (11, 5.5, 2 or 1 Mbps)
 *
 *				double distance
 *				distance beetween interferer and receiver
 *
 * ParamOut:	double ber
 *				bit error rate
 */

double Interference::coexistence_11br_15d1i (double receiver_freq,	double t_power, double t_distance, Wlan_Rate receiver_rate, 
	double interferer_freq, double i_power, double i_distance)
{
  double coupling, interference, signal, sirDB, ber;

  
  /* 
   * Compute the spectrum factor for each combination of transmitter
   * and receiver center frequencies.
   */
  coupling = coexistence_coupling_11br_15d1i (receiver_freq, interferer_freq);
	
  /* SIGNAL received from the transmitter */
  signal = InterfUtil::util_dB(t_power*1000) - InterfUtil::path_loss_attenuation (t_distance,receiver_freq/1000);

  /*
   * INTERFERENCE is the interferer's transmit power
   * attenuated by distance and by spectrum coupling.
   */
  interference = InterfUtil::util_dB(i_power*1000) - InterfUtil::path_loss_attenuation (i_distance,interferer_freq/1000) + coupling;

  /* SIR is simply the SIGNAL divided by INTERFERENCE */
  sirDB = signal - interference;

  /* Convert to BER according to receiver's modulation scheme */
  ber = Ber::bit_error_rate_802_11b (sirDB, receiver_rate);

  return (ber);
}



/*
 * Function:	coexistence_15d1r_15d4i
 *
 * Description:	The coexistence metric: compute bit error rate (BER)
 *				as a function of distance between interferer (802.15.4)
 *				and receiver (802.15.1).
 *
 * ParamIn:		double receiver_freq
 *		   		Frequency center of the receiver (802.15.1)
 *
 *				double interferer_freq
 *				frequency center of the interferer (802.15.4)
 *
 *				double distance
 *				distance beetween interferer and receiver
 *
 * ParamOut:	double ber
 *				bit error rate
 */

double Interference::coexistence_15d1r_15d4i (double receiver_freq, double t_power, double t_distance,
			       double interferer_freq, double i_power, double i_distance)
{
  double coupling, interference, signal, sirDB, ber;

 

  /* 
   * Compute the spectrum factor for each combination of transmitter
   * and receiver center frequencies.
   */
  coupling = coexistence_coupling_15d1r_15d4i (receiver_freq, interferer_freq);
	
  /* SIGNAL received from the transmitter */
  signal = InterfUtil::util_dB(t_power*1000) - InterfUtil::path_loss_attenuation (t_distance,receiver_freq/1000);

  /*
   * INTERFERENCE is the interferer's transmit power
   * attenuated by distance and by spectrum coupling.
   */
  interference = InterfUtil::util_dB(i_power*1000) - InterfUtil::path_loss_attenuation (i_distance, interferer_freq/1000) + coupling;

  /* SIR is simply the SIGNAL divided by INTERFERENCE */
  sirDB = signal - interference;

  /* Convert to BER according to receiver's modulation scheme */
  ber = Ber::bit_error_rate_802_15_1 (sirDB);

  return (ber);
}


/*
 * Function:	coexistence_15d1ri
 *
 * Description:	The coexistence metric: compute bit error rate (BER)
 *				as a function of distance between interferer and receiver
 * 				(802.15.1).
 *
 * ParamIn:		double receiver_freq
 *		   		Frequency center of the receiver (802.15.1)
 *
 *				double interferer_freq
 *				frequency center of the interferer (802.15.1)
 *
 *				double distance
 *				distance beetween interferer and receiver
 *
 * ParamOut:	double ber
 *				bit error rate
 */

double Interference::coexistence_15d1ri (double receiver_freq, double t_power, double t_distance,
			       double interferer_freq, double i_power, double i_distance)
{
  double coupling, interference, signal, sirDB, ber;

  
  /* 
   * Compute the spectrum factor for each combination of transmitter
   * and receiver center frequencies.
   */
  coupling = coexistence_coupling_15d1ri (receiver_freq, interferer_freq);
  
  /* SIGNAL received from the transmitter */
  signal = InterfUtil::util_dB(t_power*1000) -InterfUtil:: path_loss_attenuation (t_distance,receiver_freq/1000);

  /*
   * INTERFERENCE is the interferer's transmit power
   * attenuated by distance and by spectrum coupling.
   */
  interference = InterfUtil::util_dB(i_power*1000) - InterfUtil::path_loss_attenuation (i_distance, interferer_freq/1000) + coupling;

  /* SIR is simply the SIGNAL divided by INTERFERENCE */
  sirDB = signal - interference;
  
  /* Convert to BER according to receiver's modulation scheme */
  ber = Ber::bit_error_rate_802_15_1 (sirDB);

  return (ber);
}

/*
 * Function:	coexistence_15d1r_11bi
 *
 * Description:	The coexistence metric: compute bit error rate (BER)
 *				as a function of distance between interferer (802.11b)
 *				and receiver (802.15.1).
 *
 * ParamIn:		double receiver_freq
 *		   		Frequency center of the receiver (802.15.1)
 *
 *				double interferer_freq
 *				frequency center of the interferer (802.11b)
 *
 *				double distance
 *				distance beetween interferer and receiver
 *
 * ParamOut:	double ber
 *				bit error rate
 */

double Interference::coexistence_15d1r_11bi (double receiver_freq, double t_power, double t_distance,
			       double interferer_freq, double i_power, double i_distance)
{
  double coupling, interference, signal, sirDB, ber;

	  /* 
   * Compute the spectrum factor for each combination of transmitter
   * and receiver center frequencies.
   */
  coupling = coexistence_coupling_15d1r_11bi (receiver_freq, interferer_freq);
	
  /* SIGNAL received from the transmitter */
  signal = InterfUtil::util_dB(t_power*1000) - InterfUtil::path_loss_attenuation (t_distance,receiver_freq/1000);

  /*
   * INTERFERENCE is the interferer's transmit power
   * attenuated by distance and by spectrum coupling.
   */
  interference = InterfUtil::util_dB(i_power*1000) -InterfUtil:: path_loss_attenuation (i_distance, interferer_freq/1000) + coupling;

  /* SIR is simply the SIGNAL divided by INTERFERENCE */
  sirDB = signal - interference;

  /* Convert to BER according to receiver's modulation scheme */
  ber = Ber::bit_error_rate_802_15_1 (sirDB);
 // printf("ber %f, sir %f, signal %f, interference %f \n", ber,  sirDB, signal, interference);
  return (ber);
}

/*
 * Function:	coexistence_coupling_15d4r_11bi
 *
 * Description:	Given frequencies center for receiver and interferer and their respective
 *				center frequencies, compute a Spectrum Factor that reflects the
 *				amount of coupling between interferer's generated spectrum and
 *				receiver's input filter.
 *
 * ParamIn:		double Receiver_Freq
 *				Frequency center of the receiver
 *
 *				double Interferer_Freq
 *				Frequency center of the interferer
 *
 * ParamOut:	double coupling
 *				amount of coupling between interferer's generated spectrum and
 *				receiver's input filter.
 */

double Interference::coexistence_coupling_15d4r_11bi (double Receiver_Freq, double Interferer_Freq)
{
  int i; // loop variable
  /* 
   * Receive mask of the receiver 802.15.4 in the form of {f_offset, gain} pairs.
   * At f0 +/- 7.5MHz, 0 dB down
   * At f0 +/- 14MHz, 40 dB down
   * Maximum stopband attenuation is 40 dB.
   */
  double mask_15d4r[4][2] = {{-14.0, -40.0}, {-7.5, 0.0}, 
			     {7.5, 0.0}, {14.0, -40.0}};
  /* 
   * Transmit mask of the interferer 802.11b
   * At f0 +/- 11MHz, 0 dB down
   * At f0 +/- 12MHz, 30 dB down
   * At f0 +/- 22MHz, 30 dB down
   * At f0 +/- 23MHz, 55.3 dB down
   * At f0 +/- 27.5MHz, 55.3 dB down
   * negligible sidelobe power beyond 27.5Mhz
   *
   * Source: 802.11b standard, subclause 18.4.7.3
   */
  double mask_11bi[10][2] = {{-27.5, -55.3}, {-23.0, -55.3}, 
			     {-22.0, -30.0}, {-12.0, -30.0}, 
			     {-11.0, 0.0}, {11.0, 0.0}, 
			     {12.0, -30.0}, {22.0, -30.0}, 
			     {23.0, -55.3}, {27.5, -55.3}};

  double prod_spectrum[14][2];
  double normalized_area;
  double coupling = 0;

 
  /* compute the spectrum of the receiver */
  for (i=0; i<4; i++)
    mask_15d4r[i][0] += Receiver_Freq;

  /* compute the spectrum of the interferer */
  for (i=0; i<10; i++)
    mask_11bi[i][0] += Interferer_Freq;

  /* 
   * Take the product of the two spectra.  Since the two spectra
   * are theoretically infinitely wide, we limit the integration
   * from the lowest and highest frequencies given in the
   * inteferer's spectrum.
   */

  Spectrum::spectrum_multiply (mask_11bi, 10, mask_15d4r, 4, prod_spectrum);

  /* Sort the product of the two spectra */
  Spectrum::spectrum_sort (prod_spectrum, 14);

  /* display spectrum */
  /*spectrum_display (mask_15d4r, 4);
    spectrum_display (mask_11bi, 10);
    spectrum_display (prod_spectrum, 14);*/

  /* 
   * If the interferer has a 10Mhz spectrum and the receiver a 2MHz
   * spectrum, then only 2/10 of the interferer's spectrum will get
   * past the receiver's filter.  Normalize accordingly (### and 
   * fix this comment).
   */
  normalized_area = Spectrum::spectrum_area(prod_spectrum, 14)/Spectrum::spectrum_area(mask_11bi, 10);
  coupling = InterfUtil::util_dB (normalized_area);

  return (coupling);
}


/*
 * Function:	coexistence_coupling_15d4ri
 *
 * Description:	Given frequencies center for receiver and interferer and their respective
 *				center frequencies, compute a Spectrum Factor that reflects the
 *				amount of coupling between interferer's generated spectrum and
 *				receiver's input filter.
 *
 * ParamIn:		double Receiver_Freq
 *				Frequency center of the receiver
 *
 *				double Interferer_Freq
 *				Frequency center of the interferer
 *
 * ParamOut:	double coupling
 *				amount of coupling between interferer's generated spectrum and
 *				receiver's input filter.
 */

double Interference::coexistence_coupling_15d4ri (double Receiver_Freq, double Interferer_Freq)
{
  int i; // loop variable
  /* 
   * Receive mask of the receiver 802.15.4 in the form of {f_offset, gain} pairs.
   * At f0 +/- 7.5MHz, 0 dB down
   * At f0 +/- 14MHz, 40 dB down
   * Maximum stopband attenuation is 40 dB.
   */
  double mask_15d4r[4][2] = {{-14.0, -40.0}, {-7.5, 0.0}, 
			     {7.5, 0.0}, {14.0, -40.0}};
  /* 
   * Transmit mask of the interferer 802.15.4
   * At f0 +/- 3.5MHz, 0 dB down
   * At f0 +/- 7MHz, 20 dB down
   * negligible sidelobe power beyond 7Mhz
   */
  double mask_15d4i[4][2] = {{-7.0, -20.0}, {-3.5, 0.0}, 
			     {3.5, 0.0}, {7.0, -20.0}};

  double prod_spectrum[8][2];
  double normalized_area;
  double coupling = 0;


  /* compute the spectrum of the receiver */
  for (i=0; i<4; i++)
    mask_15d4r[i][0] += Receiver_Freq;

  /* compute the spectrum of the interferer */
  for (i=0; i<4; i++)
    mask_15d4i[i][0] += Interferer_Freq;

  /* 
   * Take the product of the two spectra.  Since the two spectra
   * are theoretically infinitely wide, we limit the integration
   * from the lowest and highest frequencies given in the
   * inteferer's spectrum.
   */

  Spectrum::spectrum_multiply (mask_15d4i, 4, mask_15d4r, 4, prod_spectrum);

  /* Sort the product of the two spectra */
  Spectrum::spectrum_sort (prod_spectrum, 8);

  /* display spectrum */
  /*spectrum_display (mask_15d4r, 4);
    spectrum_display (mask_15d4i, 4);
    spectrum_display (prod_spectrum, 8);*/

  /* 
   * If the interferer has a 10Mhz spectrum and the receiver a 2MHz
   * spectrum, then only 2/10 of the interferer's spectrum will get
   * past the receiver's filter.  Normalize accordingly (### and 
   * fix this comment).
   */
  normalized_area = Spectrum::spectrum_area (prod_spectrum, 8) / Spectrum::spectrum_area (mask_15d4i, 4);
  coupling =InterfUtil:: util_dB (normalized_area);

  return (coupling);
}

/*
 * Function:	coexistence_coupling_15d4r_15d1i
 *
 * Description:	Given frequencies center for receiver and interferer and their respective
 *				center frequencies, compute a Spectrum Factor that reflects the
 *				amount of coupling between interferer's generated spectrum and
 *				receiver's input filter.
 *
 * ParamIn:		double Receiver_Freq
 *				Frequency center of the receiver
 *
 *				double Interferer_Freq
 *				Frequency center of the interferer
 *
 * ParamOut:	double coupling
 *				amount of coupling between interferer's generated spectrum and
 *				receiver's input filter.
 */

double Interference::coexistence_coupling_15d4r_15d1i (double Receiver_Freq, double Interferer_Freq)
{
  int i; // loop variable
  /* 
   * Receive mask of the receiver 802.15.4 in the form of {f_offset, gain} pairs.
   * At f0 +/- 7.5MHz, 0 dB down
   * At f0 +/- 14MHz, 40 dB down
   * Maximum stopband attenuation is 40 dB.
   */
  double mask_15d4r[4][2] = {{-14.0, -40.0}, {-7.5, 0.0}, 
			     {7.5, 0.0}, {14.0, -40.0}};
  /* 
   * Transmit mask of the interferer 802.15.4 int the form of ( f_offset, gain) pairs.
   * At f0 +/- 1MHz, 20 dB down
   * At f0 +/- 2MHz, 40 dB down
   * At f0 +/- 3MHz, 60 dB down
   * Maximum stopband attenuation is 51 dB.
   */
  double mask_15d1i[6][2] = {{-3.0, -60.0}, {-2.0, -40.0}, 
			     {-1.0, -20.0}, {1.0, -20.0}, 
			     {2.0, -40.0}, {3.0, -60.0}};

  double prod_spectrum[10][2];
  double normalized_area;
  double coupling = 0;

  
  /* compute the spectrum of the receiver */
  for (i=0; i<4; i++)
    mask_15d4r[i][0] += Receiver_Freq;

  /* compute the spectrum of the interferer */
  for (i=0; i<6; i++)
    mask_15d1i[i][0] += Interferer_Freq;

  /* 
   * Take the product of the two spectra.  Since the two spectra
   * are theoretically infinitely wide, we limit the integration
   * from the lowest and highest frequencies given in the
   * inteferer's spectrum.
   */

  Spectrum::spectrum_multiply (mask_15d1i, 6, mask_15d4r, 4, prod_spectrum);

  /* Sort the product of the two spectra */
  Spectrum::spectrum_sort (prod_spectrum, 10);

  /* display spectrum */
  /*spectrum_display (mask_15d4r, 4);
    spectrum_display (mask_15d1i, 6);
    spectrum_display (prod_spectrum, 10);*/

  /* 
   * If the interferer has a 10Mhz spectrum and the receiver a 2MHz
   * spectrum, then only 2/10 of the interferer's spectrum will get
   * past the receiver's filter.  Normalize accordingly (### and 
   * fix this comment).
   */
  normalized_area = Spectrum::spectrum_area(prod_spectrum, 10)/Spectrum::spectrum_area(mask_15d1i, 6);
  coupling = InterfUtil::util_dB (normalized_area);

  return (coupling);
}

/*
 * Function:	coexistence_coupling_11br_15d4i
 *
 * Description:	Given frequencies center for receiver and interferer and their respective
 *				center frequencies, compute a Spectrum Factor that reflects the
 *				amount of coupling between interferer's generated spectrum and
 *				receiver's input filter.
 *
 * ParamIn:		double Receiver_Freq
 *				Frequency center of the receiver
 *
 *				double Interferer_Freq
 *				Frequency center of the interferer
 *
 * ParamOut:	double coupling
 *				amount of coupling between interferer's generated spectrum and
 *				receiver's input filter.
 */

double Interference::coexistence_coupling_11br_15d4i (double Receiver_Freq, double Interferer_Freq)
{
  int i; // loop variable
  /* 
   * Receive mask of the receiver 802.11b in the form of {f_offset, gain} pairs.
   * At f0 +/- 5.5MHz, 0 dB down
   * At f0 +/- 35MHz, 35 dB down
   * Maximum stopband attenuation is -35 dB.
   */
  double mask_11br[4][2] = {{-35.0, -35.0}, {-5.5, 0.0}, 
			     {5.5, 0.0}, {35.0, -35.0}};
  /* 
   * Transmit mask of the interferer 802.15.4
   * At f0 +/- 3.5MHz, 0 dB down
   * At f0 +/- 7MHz, 20 dB down
   * negligible sidelobe power beyond 7Mhz
   */
  double mask_15d4i[4][2] = {{-7.0, -20.0}, {-3.5, 0.0}, 
			     {3.5, 0.0}, {7.0, -20.0}};

  double prod_spectrum[8][2];
  double normalized_area;
  double coupling = 0;

  
  /* compute the spectrum of the receiver */
  for (i=0; i<4; i++)
    mask_11br[i][0] += Receiver_Freq;

  /* compute the spectrum of the interferer */
  for (i=0; i<4; i++)
    mask_15d4i[i][0] += Interferer_Freq;

  /* 
   * Take the product of the two spectra.  Since the two spectra
   * are theoretically infinitely wide, we limit the integration
   * from the lowest and highest frequencies given in the
   * inteferer's spectrum.
   */

  Spectrum::spectrum_multiply (mask_15d4i, 4, mask_11br, 4, prod_spectrum);

  /* Sort the product of the two spectra */
  Spectrum::spectrum_sort (prod_spectrum, 8);

  /* display spectrum */
  /*spectrum_display (mask_15d4r, 4);
    spectrum_display (mask_11bi, 10);
    spectrum_display (prod_spectrum, 14);*/

  /* 
   * If the interferer has a 10Mhz spectrum and the receiver a 2MHz
   * spectrum, then only 2/10 of the interferer's spectrum will get
   * past the receiver's filter.  Normalize accordingly (### and 
   * fix this comment).
   */
  normalized_area = Spectrum::spectrum_area(prod_spectrum, 8)/Spectrum::spectrum_area(mask_15d4i, 4);
  coupling = InterfUtil::util_dB (normalized_area);
			 
  return (coupling);
}

/*
 * Function:	coexistence_coupling_11br_15d1i
 *
 * Description:	Given frequencies center for receiver and interferer and their respective
 *				center frequencies, compute a Spectrum Factor that reflects the
 *				amount of coupling between interferer's generated spectrum and
 *				receiver's input filter.
 *
 * ParamIn:		double Receiver_Freq
 *				Frequency center of the receiver
 *
 *				double Interferer_Freq
 *				Frequency center of the interferer
 *
 * ParamOut:	double coupling
 *				amount of coupling between interferer's generated spectrum and
 *				receiver's input filter.
 */

double Interference::coexistence_coupling_11br_15d1i (double Receiver_Freq, double Interferer_Freq)
{
  int i; // loop variable
  /* 
   * Receive mask of the receiver 802.11b in the form of {f_offset, gain} pairs.
   * At f0 +/- 5.5MHz, 0 dB down
   * At f0 +/- 35MHz, 35 dB down
   * Maximum stopband attenuation is -35 dB.
   */
  	 
  double mask_11br[4][2] = {{-35.0, -35.0}, {-5.5, 0.0}, 
		     {5.5, 0.0}, {35.0, -35.0}};			 
			 
  /* 
   * Transmit mask of the interferer 802.15.1 int the form of ( f_offset, gain) pairs.
   * At f0 +/- 1MHz, 20 dB down
   * At f0 +/- 2MHz, 40 dB down
   * At f0 +/- 3MHz, 60 dB down
   * Maximum stopband attenuation is 51 dB.
   */
			 
  double mask_15d1i[6][2] = {{-3.0, -60.0}, {-2.0, -40.0}, 
			     {-1.0, -20.0}, {1.0, -20.0}, 
			     {2.0, -40.0}, {3.0, -60.0}};


  double prod_spectrum[10][2];
  double normalized_area;
  double coupling = 0;

 
  /* compute the spectrum of the receiver */
  for (i=0; i<4; i++)
    mask_11br[i][0] += Receiver_Freq;

  /* compute the spectrum of the interferer */
  for (i=0; i<6; i++)
    mask_15d1i[i][0] += Interferer_Freq;

  /* 
   * Take the product of the two spectra.  Since the two spectra
   * are theoretically infinitely wide, we limit the integration
   * from the lowest and highest frequencies given in the
   * inteferer's spectrum.
   */

  Spectrum::spectrum_multiply (mask_15d1i, 6, mask_11br, 4, prod_spectrum);

  /* Sort the product of the two spectra */
  Spectrum::spectrum_sort (prod_spectrum, 10);

  /* display spectrum */
  /*spectrum_display (mask_15d1r, 4);
    spectrum_display (mask_11bi, 10);
    spectrum_display (prod_spectrum, 14);*/

  /* 
   * If the interferer has a 10Mhz spectrum and the receiver a 2MHz
   * spectrum, then only 2/10 of the interferer's spectrum will get
   * past the receiver's filter.  Normalize accordingly (### and 
   * fix this comment).
   */
  normalized_area = Spectrum::spectrum_area(prod_spectrum, 10)/Spectrum::spectrum_area(mask_15d1i, 6);
  coupling = InterfUtil::util_dB (normalized_area);

			 
  return (coupling);
}
/*
 * Function:	coexistence_coupling_11bri
 *
 * Description:	Given frequencies center for receiver and interferer and their respective
 *				center frequencies, compute a Spectrum Factor that reflects the
 *				amount of coupling between interferer's generated spectrum and
 *				receiver's input filter.
 *
 * ParamIn:		double Receiver_Freq
 *				Frequency center of the receiver
 *
 *				double Interferer_Freq
 *				Frequency center of the interferer
 *
 * ParamOut:	double coupling
 *				amount of coupling between interferer's generated spectrum and
 *				receiver's input filter.
 */

double Interference::coexistence_coupling_11bri (double Receiver_Freq, double Interferer_Freq)
{
  int i; // loop variable
  /* 
   * Receive mask of the receiver 802.11b in the form of {f_offset, gain} pairs.
   * At f0 +/- 5.5MHz, 0 dB down
   * At f0 +/- 35MHz, 35 dB down
   * Maximum stopband attenuation is -35 dB.
   */
  double mask_11br[4][2] = {{-35.0, -35.0}, {-5.5, 0.0}, 
			     {5.5, 0.0}, {35.0, -35.0}};
  /* 
   * Transmit mask of the interferer 802.11b
   * At f0 +/- 11MHz, 0 dB down
   * At f0 +/- 12MHz, 30 dB down
   * At f0 +/- 22MHz, 30 dB down
   * At f0 +/- 23MHz, 55.3 dB down
   * At f0 +/- 27.5MHz, 55.3 dB down
   * negligible sidelobe power beyond 27.5Mhz
   *
   * Source: 802.11b standard, subclause 18.4.7.3
   */
  
  double mask_11bi[10][2] = {{-27.5, -55.3}, {-23.0, -55.3}, 
			     {-22.0, -30.0}, {-12.0, -30.0}, 
			     {-11.0, 0.0}, {11.0, 0.0}, 
			     {12.0, -30.0}, {22.0, -30.0}, 
			     {23.0, -55.3}, {27.5, -55.3}};

  double prod_spectrum[14][2];
  double normalized_area;
  double coupling = 0;

   /* compute the spectrum of the receiver */
  for (i=0; i<4; i++)
    mask_11br[i][0] += Receiver_Freq;

  /* compute the spectrum of the interferer */
  for (i=0; i<10; i++)
    mask_11bi[i][0] += Interferer_Freq;

  /* 
   * Take the product of the two spectra.  Since the two spectra
   * are theoretically infinitely wide, we limit the integration
   * from the lowest and highest frequencies given in the
   * inteferer's spectrum.
   */

  Spectrum::spectrum_multiply (mask_11bi, 10, mask_11br, 4, prod_spectrum);
			 

  /* Sort the product of the two spectra */
  Spectrum::spectrum_sort (prod_spectrum, 14);

  /* display spectrum */
  /*spectrum_display (mask_11br, 4);
    spectrum_display (mask_11bi, 10);
    spectrum_display (prod_spectrum, 14);*/

  /* 
   * If the interferer has a 10Mhz spectrum and the receiver a 2MHz
   * spectrum, then only 2/10 of the interferer's spectrum will get
   * past the receiver's filter.  Normalize accordingly (### and 
   * fix this comment).
   */
  normalized_area = Spectrum::spectrum_area (prod_spectrum, 14) / Spectrum::spectrum_area (mask_11bi, 10);
  coupling =InterfUtil::util_dB (normalized_area);

			 
  return (coupling);
}

/*
 * Function:	coexistence_coupling_15d1r_15d4i
 *
 * Description:	Given frequencies center for receiver and interferer and their respective
 *				center frequencies, compute a Spectrum Factor that reflects the
 *				amount of coupling between interferer's generated spectrum and
 *				receiver's input filter.
 *
 * ParamIn:		double Receiver_Freq
 *				Frequency center of the receiver
 *
 *				double Interferer_Freq
 *				Frequency center of the interferer
 *
 * ParamOut:	double coupling
 *				amount of coupling between interferer's generated spectrum and
 *				receiver's input filter.
 */

double Interference::coexistence_coupling_15d1r_15d4i (double Receiver_Freq, double Interferer_Freq)
{
  int i; // loop variable
  
  /* 
   * Transmit mask of the interferer 802.15.1 int the form of ( f_offset, gain) pairs.
   * At f0 +/- 1MHz, 0 dB down
   * At f0 +/- 2MHz, 41 dB down
   * At f0 +/- 3MHz, 51 dB down
   * Maximum stopband attenuation is 51 dB.
   */
  double mask_15d1r[6][2] = {{-3.0, -51.0}, {-2.0, -41.0}, 
			     {-1.0, 0.0}, {1.0, 0.0}, 
			     {2.0, -41.0}, {3.0, -51.0}};
			 /* 
   * Transmit mask of the interferer 802.15.4
   * At f0 +/- 3.5MHz, 0 dB down
   * At f0 +/- 7MHz, 20 dB down
   * negligible sidelobe power beyond 7Mhz
   */
  double mask_15d4i[4][2] = {{-7.0, -20.0}, {-3.5, 0.0}, 
			     {3.5, 0.0}, {7.0, -20.0}};

  double prod_spectrum[10][2];
  double normalized_area;
  double coupling = 0;

 

  /* compute the spectrum of the receiver */
  for (i=0; i<6; i++)
    mask_15d1r[i][0] += Receiver_Freq;

  /* compute the spectrum of the interferer */
  for (i=0; i<4; i++)
    mask_15d4i[i][0] += Interferer_Freq;

  /* 
   * Take the product of the two spectra.  Since the two spectra
   * are theoretically infinitely wide, we limit the integration
   * from the lowest and highest frequencies given in the
   * inteferer's spectrum.
   */

  Spectrum::spectrum_multiply (mask_15d4i, 4, mask_15d1r, 6, prod_spectrum);

  /* Sort the product of the two spectra */
  Spectrum::spectrum_sort (prod_spectrum, 10);

  /* display spectrum */
  /*spectrum_display (mask_15d4r, 4);
    spectrum_display (mask_15d1i, 6);
    spectrum_display (prod_spectrum, 10);*/

  /* 
   * If the interferer has a 10Mhz spectrum and the receiver a 2MHz
   * spectrum, then only 2/10 of the interferer's spectrum will get
   * past the receiver's filter.  Normalize accordingly (### and 
   * fix this comment).
   */
  normalized_area = Spectrum::spectrum_area(prod_spectrum, 10)/Spectrum::spectrum_area(mask_15d4i, 4);
  coupling = InterfUtil::util_dB (normalized_area);

  return (coupling);
}

/*
 * Function:	coexistence_coupling_15d1r_11bi
 *
 * Description:	Given frequencies center for receiver and interferer and their respective
 *				center frequencies, compute a Spectrum Factor that reflects the
 *				amount of coupling between interferer's generated spectrum and
 *				receiver's input filter.
 *
 * ParamIn:		double Receiver_Freq
 *				Frequency center of the receiver
 *
 *				double Interferer_Freq
 *				Frequency center of the interferer
 *
 * ParamOut:	double coupling
 *				amount of coupling between interferer's generated spectrum and
 *				receiver's input filter.
 */

double Interference::coexistence_coupling_15d1r_11bi (double Receiver_Freq, double Interferer_Freq)
{
  int i; // loop variable
  /* 
   * Transmit mask of the interferer 802.15.1 int the form of ( f_offset, gain) pairs.
   * At f0 +/- 1MHz, 0 dB down
   * At f0 +/- 2MHz, 41 dB down
   * At f0 +/- 3MHz, 51 dB down
   * Maximum stopband attenuation is 51 dB.
   */
			 
  double mask_15d1r[6][2] = {{-3.0, -51.0}, {-2.0, -41.0}, 
			     {-1.0, 0.0}, {1.0, 0.0}, 
			     {2.0, -41.0}, {3.0, -51.0}};
  /* 
   * Transmit mask of the interferer 802.11b
   * At f0 +/- 11MHz, 0 dB down
   * At f0 +/- 12MHz, 30 dB down
   * At f0 +/- 22MHz, 30 dB down
   * At f0 +/- 23MHz, 55.3 dB down
   * At f0 +/- 27.5MHz, 55.3 dB down
   * negligible sidelobe power beyond 27.5Mhz
   *
   * Source: 802.11b standard, subclause 18.4.7.3
   */
						 
  double mask_11bi[10][2] = {{-27.5, -55.3}, {-23.0, -55.3}, 
			     {-22.0, -30.0}, {-12.0, -30.0}, 
			     {-11.0, 0.0}, {11.0, 0.0}, 
			     {12.0, -30.0}, {22.0, -30.0}, 
			     {23.0, -55.3}, {27.5, -55.3}};

  double prod_spectrum[16][2];
  double normalized_area;
  double coupling = 0;

  

  /* compute the spectrum of the receiver */
  for (i=0; i<6; i++)
    mask_15d1r[i][0] += Receiver_Freq;

  /* compute the spectrum of the interferer */
  for (i=0; i<10; i++)
    mask_11bi[i][0] += Interferer_Freq;

  /* 
   * Take the product of the two spectra.  Since the two spectra
   * are theoretically infinitely wide, we limit the integration
   * from the lowest and highest frequencies given in the
   * inteferer's spectrum.
   */

  Spectrum::spectrum_multiply (mask_11bi, 10, mask_15d1r, 6, prod_spectrum);

  /* Sort the product of the two spectra */
  Spectrum::spectrum_sort (prod_spectrum, 16);

  /* display spectrum */
  /*spectrum_display (mask_15d1r, 6);
  spectrum_display (mask_11bi, 10);
  spectrum_display (prod_spectrum, 8);
*/
  /* 
   * If the interferer has a 10Mhz spectrum and the receiver a 2MHz
   * spectrum, then only 2/10 of the interferer's spectrum will get
   * past the receiver's filter.  Normalize accordingly (### and 
   * fix this comment).
   */
  normalized_area = Spectrum::spectrum_area(prod_spectrum, 16)/Spectrum::spectrum_area(mask_11bi, 10);
  coupling = InterfUtil::util_dB (normalized_area);

  return (coupling);
}

/*
 * Function:	coexistence_coupling_15d1ri
 *
 * Description:	Given frequencies center for receiver and interferer and their respective
 *				center frequencies, compute a Spectrum Factor that reflects the
 *				amount of coupling between interferer's generated spectrum and
 *				receiver's input filter.
 *
 * ParamIn:		double Receiver_Freq
 *				Frequency center of the receiver
 *
 *				double Interferer_Freq
 *				Frequency center of the interferer
 *
 * ParamOut:	double coupling
 *				amount of coupling between interferer's generated spectrum and
 *				receiver's input filter.
 */

double Interference::coexistence_coupling_15d1ri (double Receiver_Freq, double Interferer_Freq)
{
  int i; // loop variable
  
  /* 
   * Transmit mask of the interferer 802.15.1 int the form of ( f_offset, gain) pairs.
   * At f0 +/- 1MHz, 0 dB down
   * At f0 +/- 2MHz, 41 dB down
   * At f0 +/- 3MHz, 51 dB down
   * Maximum stopband attenuation is 51 dB.
   */
  double mask_15d1r[6][2] = {{-3.0, -51.0}, {-2.0, -41.0}, 
			     {-1.0, 0.0}, {1.0, 0.0}, 
			     {2.0, -41.0}, {3.0, -51.0}};
  
 /* 
   * Transmit mask of the interferer 802.15.1 int the form of ( f_offset, gain) pairs.
   * At f0 +/- 1MHz, 20 dB down
   * At f0 +/- 2MHz, 40 dB down
   * At f0 +/- 3MHz, 60 dB down
   * Maximum stopband attenuation is 60 dB.
   */
  double mask_15d1i[6][2] = {{-3.0, -60.0}, {-2.0, -40.0}, 
			     {-1.0, -20.0}, {1.0, -20.0}, 
			     {2.0, -40.0}, {3.0, -60.0}};

  double prod_spectrum[12][2];
  double normalized_area;
  double coupling = 0;

 

  /* compute the spectrum of the receiver */
  for (i=0; i<6; i++)
    mask_15d1r[i][0] += Receiver_Freq;

  /* compute the spectrum of the interferer */
  for (i=0; i<6; i++)
    mask_15d1i[i][0] += Interferer_Freq;

  /* 
   * Take the product of the two spectra.  Since the two spectra
   * are theoretically infinitely wide, we limit the integration
   * from the lowest and highest frequencies given in the
   * inteferer's spectrum.
   */

  Spectrum::spectrum_multiply (mask_15d1i, 6, mask_15d1r, 6, prod_spectrum);
		 
  /* Sort the product of the two spectra */
  Spectrum::spectrum_sort (prod_spectrum, 12);

  /* display spectrum */
  /*spectrum_display (mask_15d1r, 6);
    spectrum_display (mask_15d1i, 6);
    spectrum_display (prod_spectrum, 12);*/

  /* 
   * If the interferer has a 10Mhz spectrum and the receiver a 2MHz
   * spectrum, then only 2/10 of the interferer's spectrum will get
   * past the receiver's filter.  Normalize accordingly (### and 
   * fix this comment).
   */
  normalized_area = Spectrum::spectrum_area (prod_spectrum, 12) / Spectrum::spectrum_area (mask_15d1i, 6);
  coupling = InterfUtil::util_dB (normalized_area);

  return (coupling);
}
