/***********************************************************/
/*   physics-interfer.c  - contains the functions for the  */
/*                  Interference.                          */
/***********************************************************/

#include "physic.h"

/**
 * FUNCTION: generating random data
 * INPUT: none
 * OUTPUT: trbit_I,trbit_Q ; random data
 */
void  Interference2::DataGen(char *data, int length, physical_layer *PHY )
{
  unsigned char newbit;
  int jj;
  
  for ( jj=0;jj<length;jj++){
    newbit = ((PHY->iseed & IB20) >> 19) ^ ((PHY->iseed & IB3) >> 2) ;
    PHY->iseed = (PHY->iseed << 1 ) | newbit;
    data[jj] = newbit;
  }     	
}	/* end */

/**
 * FUNCTION: generating random data
 * INPUT: none
 * OUTPUT: trbit_I,trbit_Q ; random data
 */
void  Interference2::DataGenVTB(char *data, int length, physical_layer *PHY )
{
  unsigned char newbit;
  int jj;
  int ppacket[4];
  
  for ( jj=0;jj<length;jj++){
    newbit = ((PHY->iseed & IB20) >> 19) ^ ((PHY->iseed & IB3) >> 2) ;
    PHY->iseed = (PHY->iseed << 1 ) | newbit;
    data[jj] = newbit;
  }     	
  ppacket[Spsition/16]   = 0xcba4;	
  ppacket[Spsition/16+1] = 0xb72d;
  ppacket[Spsition/16+2] = 0xeacc;	
  ppacket[Spsition/16+3] = 0xc0bc;
 
  for (jj=0;jj<Accesslength;jj++){
    data[jj]= (ppacket[jj/16] >> (jj%16) ) & 0x1;
  }	
}	/* end */


/******************************************/
void Interference2::Interfere(short int n, 
			      physical_layer *PHY, 
			      double **interface_pt,
			      int collision_number){
  double lp;
  double pr2;
  double d2,pt2;
  double df;
  double CAIRdb;
  int i;
  char bit[11]; 
	

  if(!n){
    for(i=0;i< collision_number;i++)
      PHY->IntCounter[i] = 0;
  }/*	if(!n){*/

  for(i=1;i<= collision_number;i++){
    if ( (interface_pt[0][i]<=n) && (n< (interface_pt[0][i]+interface_pt[1][i])) ){
      d2=interface_pt[5][i];
      pt2 = 1e-3*interface_pt[3][i];
      if (d2<=8)
	lp= 32.45+20*log10(2.4*d2);
      else if (d2>8)
	lp= 58.3+33*log10(d2/8.);
      pr2 = 10*log10(pt2)-lp;
      CAIRdb= 10*log10(PHY->pr1)-pr2;
			
      /*			if (PHY->sw==0){
	PHY->sw=1;
	printf("\nCIR= %f\n",CAIRdb);
	}*/

      PHY->badj[i-1]= sqrt(pow(10, -.1*CAIRdb));
      DataGen(bit,1,PHY);
      df = interface_pt[4][0] - interface_pt[4][i];
      if (df<0)
	df=-df;
      if (df>=15)
	df = 15;
      if((interface_pt[2][i] == 1.)||(interface_pt[2][i] == 4.))
	AdjIntRead(bit, df, PHY, i-1,0,1);
      else if ((interface_pt[2][i] == 2.)||(interface_pt[2][i] == 3.) )
	Tx80211(bit,df, PHY,i-1,0,1);
    }/* end if*/
  }/*	for(i=1;i<= intcollision_number;i++){*/
}

/***********************************************************/ 
/******************************************/
void Interference2::Interfere11(short int n, physical_layer *PHY, double **interface_pt,int collision_number){
  double lp;
  double pr2;
  double d2,pt2;
  double df;
  double CAIRdb;
  int i,j;
  char bit[11]; 
	
  if(n>224){
    n-=224;
    n/=11;
    n+=224;
  }
  for(i=1;i<= collision_number;i++){
    if   (  (interface_pt[0][i]-4<=n) &&
	    (n<= (interface_pt[0][i]+interface_pt[1][i]-4)) &&
	    (interface_pt[1][i]>=4) ) {

      d2=interface_pt[5][i];
      pt2 = 1e-3*interface_pt[3][i];
      if (d2<=8)
	lp= 32.45+20*log10(2.4*d2);
      else if (d2>8)
	lp= 58.3+33*log10(d2/8.);
      pr2 = 10*log10(pt2)-lp;
      CAIRdb= 10*log10(PHY->pr1)-pr2;
      PHY->badj[i-1]= sqrt(pow(10, -.1*CAIRdb));
      df = interface_pt[4][0] - interface_pt[4][i];
      if (df<0)
	df=-df;
      if (df>=15)
	df = 15;
      if((interface_pt[2][i] == 1.)||(interface_pt[2][i] == 4.)){
	for (j=0;j<8; j++){
	  DataGen(bit,1,PHY);
	  AdjIntRead(bit, df, PHY, i-1,j*Ns,11);
	}/*for j*/
      }/*end if*/
      else if ((interface_pt[2][i] == 2.)||(interface_pt[2][i] == 3.) ){
	for (j=0;j<8; j++){
	  DataGen(bit,1,PHY);
	  Tx80211(bit,df, PHY,i-1,j*Ns,11);
	}/*for j */
      }/* end else if*/
    }/* end if*/
  }/* end for*/

}

/****************************************
802.11 Transmitter
*****************************************/
void  Interference2::Tx80211(char *data802,double df,physical_layer *PHY, int Intindex, int timeOffset, char swRate){
  double Quad802[Ns],Inphase802[Ns],state[Ns],tt[Ns],badj;
  int j,i;
  char Int802[Ns]={0};
  int n;
  char chip[11]= {1,-1,1,1,-1,1,1,1,-1,-1,-1};
 
  n= PHY->IntCounter[Intindex];
  badj= PHY->badj[Intindex];
  if(!n){
    PHY->phInt[Intindex] = rand()/(double)RAND_MAX;
    PHY->TdInt802[Intindex] =(short int) (Ns* PHY->phInt[Intindex]);
    if (swRate==11)
      PHY->TdInt802[Intindex]=0;
    PHY->phInt[Intindex] *= (2*PI);
    for( i=0;i<RSLENGTH;i++)
      PHY->stateInt802[Intindex][i] = 0.;
    for(j=0;j<2*Ns;j++){
      PHY->AdjIntInphaseB[Intindex][j ] = 0;
      PHY->AdjIntQuadB[Intindex][j] = 0;
    }
  }/*	if(!n){*/


  for( j=0;j<Ns;j+=4){
    Int802[j]= (2*data802[0]-1)*chip[j/4];
  }/*	for( j=0;j<Ns;j+=4){*/

  for( j=0;j<Ns;j++){
    state[j] =0;
    for( i=RSLENGTH-2;i>=0;i--)
      PHY->stateInt802[Intindex][i+1] = PHY->stateInt802[Intindex][i];
    PHY->stateInt802[Intindex][0] = Int802[j] ;
    for(i=0;i<RSLENGTH;i++){
      state [j] += PHY->stateInt802[Intindex][i] * PHY->hraised[i];
    }/*for(i=0;i<RSLENGTH;i++){*/

    tt[j] = 2.*PI*(df* (j +Ns* n)/(double)Ns);
    if( ( tt[j]> (2*PI) ) || ( tt[j] < -(2*PI) ) )
      tt[j] = fmod(tt[j],(2*PI));
    Inphase802[j] = cos(PHY->phInt[Intindex]+tt[j])*state[j];
    Quad802[j] = sin(PHY->phInt[Intindex]+tt[j])*state[j];
    Inphase802[j] *= badj;
    Quad802[j] *= badj;
    PHY->AdjIntInphaseB[Intindex] [j + PHY->TdInt802[Intindex]]= Inphase802[j];
    PHY->AdjIntQuadB[Intindex] [j + PHY->TdInt802[Intindex]]= Quad802[j];
  }/*for( j=0;j<Ns;j++){*/
  for(j=0;j<Ns;j++){
    PHY->Inphase[j+timeOffset] += PHY->AdjIntInphaseB[Intindex] [j];
    PHY->Quad[j+timeOffset] += PHY->AdjIntQuadB[Intindex] [j];
  }

  for(j=0;j<PHY->TdInt802[Intindex];j++){
    PHY->AdjIntInphaseB[Intindex] [j] = PHY->AdjIntInphaseB[Intindex] [j+Ns];
    PHY->AdjIntQuadB[Intindex] [j] = PHY->AdjIntQuadB[Intindex] [j+Ns];
  }
  PHY->IntCounter[Intindex]++;


}/* end */
/**************************************
 **************************************/
void Interference2::AdjIntRead(char *dataB,double df,physical_layer *PHY, int Intindex, int timeOffset, char swRate){
  short int j;
  int n;
  double state [Ns],tt,badj;
  double AdjIntQuad[Ns],AdjIntInphase[Ns];

	
  n= PHY->IntCounter[Intindex];
  badj= PHY->badj[Intindex];

  if(!n){
    PHY->IntCounter[Intindex] = 0;
    PHY->phInt[Intindex]= rand()/(double)RAND_MAX;
    PHY->TdIntBT[Intindex] =(short int) (Ns* PHY->phInt[Intindex]);
    if (swRate==11)
      PHY->TdIntBT[Intindex]=0;
    PHY->phInt[Intindex] = (1./hf) * rand()/(double)RAND_MAX;; 
    PHY->a1Int[Intindex] = ALPHA_N_1;
    for(j=0;j<2*Ns;j++){
      PHY->AdjIntInphaseB[Intindex] [j] = 0;
      PHY->AdjIntQuadB[Intindex] [j] = 0;
    }
  }
  for(j=0;j<Ns;j++){
    tt = 2.*PI*(df* (j +Ns* n)/(double)Ns);
    if( tt> (2*PI))
      tt = fmod(tt,(2*PI));
    state[j]=(1.0-2.0*dataB[0])*PHY->qcoef[j] + (1.0-2.0*PHY->a1Int[Intindex])*PHY->qcoef[j+Ns]+PHY->phInt[Intindex];
    AdjIntInphase[j] = cos(2.*PI*hf*state[j]+tt );
    AdjIntQuad[j] = sin(2.*PI*hf*state[j]+tt );
    AdjIntInphase[j] *= badj;
    AdjIntQuad[j] *= badj;
    PHY->AdjIntInphaseB[Intindex] [j + PHY->TdIntBT[Intindex]] = AdjIntInphase[j];
    PHY->AdjIntQuadB[Intindex] [j + PHY->TdIntBT[Intindex]] = AdjIntQuad[j];
  }
  for(j=0;j<Ns;j++){
    PHY->Inphase[j+timeOffset] += PHY->AdjIntInphaseB[Intindex] [j];
    PHY->Quad[j+timeOffset] += PHY->AdjIntQuadB[Intindex] [j];
  }
  for(j=0;j<PHY->TdIntBT[Intindex];j++){
    PHY->AdjIntInphaseB[Intindex] [j] = PHY->AdjIntInphaseB[Intindex] [j+Ns];
    PHY->AdjIntQuadB[Intindex] [j] = PHY->AdjIntQuadB[Intindex] [j+Ns];
  }

  if(!PHY->a1Int[Intindex])
    PHY->phInt[Intindex] += 0.5;
  else{
    PHY->phInt[Intindex] -= 0.5;
  }

  PHY->IntCounter[Intindex]++;
  PHY->a1Int[Intindex] = dataB[0];

}



