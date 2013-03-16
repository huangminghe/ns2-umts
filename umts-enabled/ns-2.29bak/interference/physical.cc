/***********************************************************/
/*   physical.c  - contains the functions for the          */
/*                  physical layer.                        */
/***********************************************************/

#include "physic.h"



/***********************************************************/ 

void 	Interference2::Wlan_bt_collision(physical_layer *PHY, char *packet,double **interface_pt,
					 int collision_number, char BypassInterfer ) {

  int i;
  char recbit;
   
  Interface(PHY,interface_pt);
  if(collision_number){
    if(BypassInterfer){
      for(i = 0; i < interface_pt[1][0] ; i++) {
	modulator(i, PHY, packet[i]);
	Interfere(i, PHY, interface_pt, collision_number); 
	Channel_AWGN(PHY);
	recbit = Demodulator_Ldid(i, PHY);
	if(i>=2)
	  packet[i-2] = recbit;
      }

      i=(int)interface_pt[1][0];
      modulator(i, PHY, 0);
      Interfere(i, PHY, interface_pt, collision_number); 
      Channel_AWGN(PHY);
      recbit = Demodulator_Ldid(i, PHY);
      packet[i-2]=recbit;
      i++;
      modulator(i, PHY, 0);
      Interfere(i, PHY, interface_pt, collision_number); 
      Channel_AWGN(PHY);
      recbit = Demodulator_Ldid(i, PHY);
      packet[i-2]=recbit;
    }/*if(BypassInterfer){*/
    else if(!BypassInterfer)
      IntBypassB(PHY, packet, interface_pt,collision_number); 
  }/*	if(collision_number){*/
  else
    Bypass(PHY,packet,interface_pt);


}   /* end */
/***********************************************************/ 

void 	Interference2::Wlan_bt_collision_VTB(physical_layer *PHY, char *packet,double **interface_pt,
					     int collision_number, char BypassInterfer ) {


	
  BypassInterfer=0; 
  Interface(PHY,interface_pt);
  if(collision_number){
    if(BypassInterfer){
      /*			for(i = 0; i < interface_pt[1][0] ; i++) {
	modulator(i, PHY, packet[i]);
	Interfere(i, PHY, interface_pt, collision_number); 
	Channel_AWGN(PHY);
	recbit = Demodulator_Ldid(i, PHY);
	if(i>=2)
	packet[i-2] = recbit;
	}

	i=(int)interface_pt[1][0];
	modulator(i, PHY, 0);
	Interfere(i, PHY, interface_pt, collision_number); 
	Channel_AWGN(PHY);
	recbit = Demodulator_Ldid(i, PHY);
	packet[i-2]=recbit;
	i++;
	modulator(i, PHY, 0);
	Interfere(i, PHY, interface_pt, collision_number); 
	Channel_AWGN(PHY);
	recbit = Demodulator_Ldid(i, PHY);
	packet[i-2]=recbit;*/
    }/*if(BypassInterfer){*/
    else if(!BypassInterfer)
      IntBypassB_VTB(PHY, packet, interface_pt,collision_number); 
  }/*	if(collision_number){*/
  else
    Bypass_VTB(PHY,packet,interface_pt);


}   /* end */
/****************************************************************/
void 	Interference2::Bt802_Wlan1_collision(physical_layer *PHY, char *packet,double **interface_pt,
					     int collision_number,char BypassInterfer) {

  int i;
  char recbit;
   
  wInterface(PHY,interface_pt);
  if(collision_number){
    if(BypassInterfer){
      for(i = 0; i < interface_pt[1][0] ; i++) {
	wmodulator(i, PHY, packet[i]);
	Interfere(i, PHY, interface_pt, collision_number); 
	Channel_AWGN(PHY);
	recbit = Demodulator_DBPSK(i, PHY);
	if((i>=2) && (i<interface_pt[1][0]-2))
	  packet[i-1] = recbit;
      }/*for(i = 0; i < interface_pt[1][0] ; i++) {*/
    }/*if(BypassInterfer){*/
    else if(!BypassInterfer)
      IntBypassW(PHY, packet, interface_pt,collision_number); 
  }/*	if(collision_number){*/
  else
    wBypass(PHY,packet,interface_pt);


}   /* end */

/****************************************************************/
void 	Interference2::Bt802_Wlan11_collision(physical_layer *PHY, 
					      char *packet,
					      double **interface_pt, 
					      int collision_number,
					      char BypassInterfer) {

  int i,j;
  short bit8[6],sl8[88];
  int recbit[6];
  int trlength;
  int packetbuf=(int)interface_pt[1][0];
  wInterface(PHY,interface_pt);
  /* header is 1 Mb/sec */
  interface_pt[1][0]=192;
  Bt802_Wlan1_collision(PHY, packet, interface_pt,collision_number,BypassInterfer); 
  interface_pt[1][0]=packetbuf;
  /*payload 11 Mb/sec */
  wInterface11(PHY,interface_pt);
  trlength=(int)((interface_pt[1][0]-192)*11)/88;
  
  if(collision_number){
    if(BypassInterfer){
      for(i=192;i<192+88*trlength; i+=88) {
	for(j=0;j<88;j++)
	  sl8[j]=packet[i+j];
	for(j=0;j<6;j++)
	  bit8[j]=0;
	for(j=0;j<88;j++)
	  bit8[j/16]|= ( sl8[j]<<(j%16) ) ;
	
	w11modulator(i, PHY, bit8);
	Interfere11(i, PHY, interface_pt, collision_number); 
	Channel_AWGN11(PHY);
	Demodulator_CCK(i, PHY,recbit);
	if (i==192){
	  for (j=10;j<88;j++)
	    packet[i+j-8] = ((recbit[j/16]>>(j%16))&0x1);
	}
	else{
	  for (j=0;j<88;j++)
	    packet[i+j-8] = ((recbit[j/16]>>(j%16))&0x1);
	}
	i=i;
      }/*for(i = 0; i < interface_pt[1][0] ; i++) {*/
    }/*if(BypassInterfer){*/
    else if(!BypassInterfer)
      IntBypassW11(PHY, packet, interface_pt,collision_number); 
  }/*	if(collision_number){*/
  else
    w11Bypass(PHY,packet,interface_pt);


}   /* end */

/****************************************************************/
void 	Interference2::Bt_WlanFH1_collision(physical_layer *PHY, char *packet,double **interface_pt,
					    int collision_number,char BypassInterfer) {
	
  Wlan_bt_collision(PHY,packet, interface_pt,collision_number,BypassInterfer);

}   /* end */

/******************************************/
void Interference2::Interface( physical_layer *PHY, double **interface_pt){
  double lp;
  double eb,eer,nn0,pr1;
  double d1,pt1;
  double EbNodb; 

  eb = sensitivity-30;
  eb = pow(10,.1*eb);
  eb *= 1e-6;
  nn0 = eb/pow(10,(16*.1));/*16 dB*/
  d1 = interface_pt[5][0];
  pt1 = interface_pt[3][0];
  if (d1<=8)
    lp= 32.45+20*log10(2.4*d1);
  else if (d1>8)
    lp= 58.3+33*log10(d1/8.);
 
  pr1 = 10*log10(pt1*1e-3)-lp;
  PHY->pr1 = pow(10,.1*pr1);
  eer = PHY->pr1*1e-6;
  EbNodb = 10*log10(eer/nn0); 
  PHY->n0=Ns*.5*pow(10.,(-.1*EbNodb));
 
}
/******************************************/
void Interference2::wInterface( physical_layer *PHY, double **interface_pt){
  double lp;
  double eb,eer,nn0,pr1;
  double d1,pt1;
  double EbNodb;
  /*static char swn=0;*/

  eb = sensitivity-30;
  eb = pow(10,.1*eb);
  eb *= 1e-6;
  nn0 = eb/pow(10,(8*.1));/*8 dB*/
 
  d1 = interface_pt[5][0];
  pt1 = interface_pt[3][0];
  if (d1<=8)
    lp= 32.45+20*log10(2.4*d1);
  else if (d1>8)
    lp= 58.3+33*log10(d1/8.);
 
  pr1 = 10*log10(pt1*1e-3)-lp;
  PHY->pr1 = pow(10,.1*pr1);
  eer = PHY->pr1*1e-6;
  EbNodb = 10*log10(eer/nn0);
  /*if (swn==0){
  //printf("\nEB/no=%4f\n",EbNodb);
  //swn=1;
  //}*/
  PHY->n0=Ns*.5*pow(10.,(-.1*EbNodb));
 
}

/******************************************/
void Interference2::wInterface11( physical_layer *PHY, double **interface_pt){
  double lp;
  double eb,eer,nn0,pr1;
  double d1,pt1;
  double EbNodb;


  eb = sensitivity-30;
  eb = pow(10,.1*eb);
  eb *= 1e-6;
  nn0 = eb/pow(10,(8*.1));/*8 dB*/
 
  d1 = interface_pt[5][0];
  pt1 = interface_pt[3][0];
  if (d1<=8)
    lp= 32.45+20*log10(2.4*d1);
  else if (d1>8)
    lp= 58.3+33*log10(d1/8.);
 
  pr1 = 10*log10(pt1*1e-3)-lp;
  PHY->pr1 = pow(10,.1*pr1);
  eer = PHY->pr1*1e-6;
  EbNodb = 10*log10(eer/nn0); 
  PHY->n0=Nscck*.5*pow(10.,(-.1*EbNodb));
 
}
