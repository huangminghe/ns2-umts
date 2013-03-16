/*
** Coexistence.ex.c :
**
** 
** National Institute of Standards and Technology
**
** This model was developed at the National Institute of Standards
** and Technology by employees of the Federal Government in the course
** of their official duties. Pursuant to title 17 Section 105 of the
** United States Code this software is not subject to copyright
** protection and is in the public domain. This is an experimental
** system.	NIST assumes no responsibility whatsoever for its use by
** other parties, and makes no guarantees, expressed or implied,
** about its quality, reliability, or any other characteristic.
**
** We would appreciate acknowledgement if the model is used.
**
** NIST ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION
** AND DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
** RESULTING FROM THE USE OF THIS SOFTWARE.
**
** Primary Author:		Nicolas Chevrollier
** Last Modification:  
*/

/* include the header */
#include "physic.h"
#include "interference.h"
#include "mobilenode.h"
#include "random.h"

int Interference2::interference2(Packet *pktRx, double distRT, Packet *pktInt, double distRI, double difftime, int bypass) 
{  
  int numErrs;

  physical_layer phy; 
  int test;
  int num_collisions;
  int size_packet;
  int Phy_Char;
  int wlan_bit_rate;
  double **interference_ptr; 
  char * start_packet;
  char * final_packet;
  int j;
  int a;
  
  numErrs = 0;

  //number of collision == 1
  num_collisions = 1;

  // Bypass 1 or DSP 0
  test = 1;
  
  //type of wlan baseband
  Phy_Char = 1;	
  
  //wlan_bit_rate
  wlan_bit_rate = 11000000;
  //wlan_bit_rate = 1000000;
  
  //interference_ptr = (double**) malloc( 12 * sizeof(double) ); 
  interference_ptr = (double**) malloc( 6 * sizeof(double*) ); 
  /*
    
  first row  |	0		        |  size (bit) | type(BT or WLAN) | Power (mW)  |freq (Mghz) | d(rxi,txi) |
  packet i   |			        |	      | 		 |   	       |	    |            | 
             |--------------------------|-------------|------------------|-------------|------------|------------|
  Second row |   	                |             |			 | 	       |	    |            |
  packet j   | begining j - beginning i |  size (bit) | type(BT or WLAN) | Power (mW)  |freq (Mghz) | d(rxi,txj) | 	
  
  */
  
  
  for(j = 0; j < 6 ; j++) {
    interference_ptr[j] = (double*) malloc( (num_collisions+1) * sizeof(double) ); 
  }
  
  interference_ptr[0][0] = 0.0;
  interference_ptr[1][0] = HDR_CMN(pktRx)->size()*8;
  interference_ptr[2][0] = pktRx->txinfo_.getPktType();
  interference_ptr[3][0] = pktRx->txinfo_.getTxPr()*1000;
  interference_ptr[4][0] = LIGHT_SPEED / (1000*1000*pktRx->txinfo_.getLambda());
  interference_ptr[5][0] = distRT;
  
  interference_ptr[0][1] = difftime*-1000000;
  interference_ptr[1][1] = HDR_CMN(pktInt)->size()*8;
  interference_ptr[2][1] = pktInt->txinfo_.getPktType();
  interference_ptr[3][1] = pktInt->txinfo_.getTxPr()*1000;
  interference_ptr[4][1] = LIGHT_SPEED / (1000*1000*pktInt->txinfo_.getLambda());
  interference_ptr[5][1] = distRI;  
  
  /*
  interference_ptr[0][0] = 0.0;
  interference_ptr[1][0] = 304.0;
  interference_ptr[2][0] = 1.0;
  interference_ptr[3][0] = 25.0;
  interference_ptr[4][0] = 2437.0;
  interference_ptr[5][0] = 14.0;
  
  interference_ptr[0][1] = -832.907314;
  interference_ptr[1][1] = 2871.0;
  interference_ptr[2][1] = 3.0;
  interference_ptr[3][1] = 1.0;
  interference_ptr[4][1] = 2433.0;
  interference_ptr[5][1] = 1.0;
  */
  //errors 10
  
  /*
    interference_ptr[0][0] = 0.0;
    interference_ptr[1][0] = 304.0;
    interference_ptr[2][0] = 3.0;
    interference_ptr[3][0] = 25.0;
    interference_ptr[4][0] = 2437.0;
    interference_ptr[5][0] = 14.0;
  
    interference_ptr[0][1] = -791.089132;
    interference_ptr[1][1] = 2871.0;
    interference_ptr[2][1] = 1.0;
    interference_ptr[3][1] = 1.0;
    interference_ptr[4][1] = 2432.0;
    interference_ptr[5][1] = 1.0;
    //errors 15
    */

  printf ("pktRx: %.4f %.0f %.0f %.3f %.4f %.4f\n", interference_ptr[0][0], 
	  interference_ptr[1][0], interference_ptr[2][0], interference_ptr[3][0],
	  interference_ptr[4][0], interference_ptr[5][0]);

  printf ("pkInt: %.4f %.0f %.0f %.3f %.4f %.4f\n", interference_ptr[0][1], 
	  interference_ptr[1][1], interference_ptr[2][1], interference_ptr[3][1],
	  interference_ptr[4][1], interference_ptr[5][1]);

  //Create the start packet and the final packet
  //Final packet is going to be modified
  size_packet = (int) interference_ptr[1][0];
  //printf(" packet_size %d \n",  size_packet);
  start_packet = (char*)malloc( size_packet * sizeof(char) );
  final_packet = (char*)malloc( size_packet * sizeof(char) );
  
  /*to fill randomly the packet by zeros and ones*/
  for(j = 0 ; j < size_packet ; j++){
    a=rand();
    start_packet[j] = a%2;
    final_packet[j] = a%2;
  }	
  
 
  //MAX_COLLISIONS = 1250
  //Initialization of the physical layer 
  initialize_physical_layer(&phy,1250);
  
  // if transmitted packet is BT and interferer BT or WLAN
  if(interference_ptr[2][0] == 1) {
    if (test)	
      Wlan_bt_collision(&phy,final_packet,interference_ptr,num_collisions,1); /* bypass */
    else
      Wlan_bt_collision(&phy,final_packet,interference_ptr,num_collisions,0); /* real DSP */
  } else {
    
    // if transmitted packet is WLAN and interferer BT or WLAN
    //if frequency hopping	FH == 0
    //if ( Phy_Char == FH ){
    if ( Phy_Char == 0 ){
      if (test)
	Bt_WlanFH1_collision(&phy,final_packet,interference_ptr,num_collisions,1);
      else
	Bt_WlanFH1_collision(&phy,final_packet,interference_ptr,num_collisions,0);					
    }			
    
    //if direct sequence DS == 1
    //if ( Phy_Char == DS ) {
    if ( Phy_Char == 1 ) {
      //if bit rate is 11 Mbit/s
      if ( wlan_bit_rate == 11000000 ) {
	//if ack
	if (size_packet == 304) {
	  if (test)
	    Bt802_Wlan1_collision(&phy, final_packet, interference_ptr, num_collisions,1);
	  else
	    Bt802_Wlan1_collision(&phy, final_packet, interference_ptr, num_collisions,0);
	} else {
	  if (test)
	    Bt802_Wlan11_collision(&phy,final_packet,interference_ptr,num_collisions,1);
	  else
	    Bt802_Wlan11_collision(&phy,final_packet,interference_ptr,num_collisions,0);
	}						
      }
      //if but rate is 1 Mbit/s
      if ( wlan_bit_rate == 1000000 ) {
	if (test)						
	  Bt802_Wlan1_collision(&phy, final_packet, interference_ptr, num_collisions,1);
	else
	  Bt802_Wlan1_collision(&phy, final_packet, interference_ptr, num_collisions,0);
      }
    }
  }
  numErrs = CollisionUtil::compute_errors_number(start_packet, final_packet,size_packet);

  //printf(" number of errors %d\n", numErrs);
  for(j = 0; j < 6 ; j++) {
    free (interference_ptr[j]);
  }
  free (interference_ptr);

  return numErrs;
}
