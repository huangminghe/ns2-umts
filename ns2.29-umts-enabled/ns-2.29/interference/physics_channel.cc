/***********************************************************/
/*   physics-channel.c  - contains the functions for the   */
/*                  channel model.                         */
/***********************************************************/

#include "physic.h"


/***********************************************************/ 
void  Interference2::Channel_AWGN(physical_layer *PHY){

 NoiseGen(PHY,0);


}    /* end channel */
/*******************************************/
void  Interference2::Channel_AWGN11(physical_layer *PHY){

  int i;

	for (i=0;i<8;i++)
		 NoiseGen(PHY,Ns*i);

}    /* end channel */



/***********************************************************/ 
 void  Interference2::NoiseGen( physical_layer *PHY, int index){
	 double m1,m2;
	 double NI,NQ;
	 int j;
	for( j=0; j<Ns; j++)
			{
		 PHY->x1 = (171 *  PHY->x1)%30269;
		 PHY->x2 = (172 *  PHY->x2)%30307;
		 PHY->x3 = (170 *  PHY->x3)%30323;
		 m1 = fmod( ((double)(PHY->x1)/30269. + (double)(PHY->x2)/30307. + (double)(PHY->x3)/30323.), 1.);
		
		 PHY->x4 = (171 *  PHY->x4)%30269;
		 PHY->x5 = (172 *  PHY->x5)%30307;
		 PHY->x6 = (170 *  PHY->x6)%30323;
		 m2 = fmod( ((double)(PHY->x4)/30269. + (double)(PHY->x5)/30307. + (double)(PHY->x6)/30323.), 1.);
		 
		
		 NI=sqrt(-2*PHY->n0*log(m1))*cos(PI*2*m2);
		 NQ=sqrt(-2*PHY->n0*log(m1))*sin(PI*2*m2);
		 PHY->Inphase[index+j] += NI ;
		 PHY->Quad[index+j] += NQ ;
	
	}/*	for(j=0; j<Ns*16*palen; j++)*/
 }  /* end  */
/***********************************************************/ 
void  Interference2::Channel_Rayleigh(physical_layer *PHY){

 NoiseGen(PHY,0);


}    /* end channel */

/******************************************************
Convolve MODULATOR output with CHANNEL IMPULSE RESPONSE.
******************************************************/
void Interference2::ChanRespConv(physical_layer *PHY,double **interface_pt){



}

