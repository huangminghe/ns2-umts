/***********************************************************/
/*   physics-tx  - contains the functions for the          */
/*                  Transmiter.                            */
/***********************************************************/

#include "physic.h"

/***********************************************************/ 
void Interference2::modulator(short int n, physical_layer *PHY, char databit) {

  short int j;
  double state [Ns];
 
  if(!n){
    PHY->a1 = ALPHA_N_1;
    PHY->ph = INTPHASE; 
  }
  for(j=0;j<Ns;j++){
    state[j]=(1.0-2.0*databit)*(PHY->qcoef[j]) + (1.0-2.0*PHY->a1)*(PHY->qcoef[j+Ns])+PHY->ph;
    PHY->Inphase[j] = cos(2.*PI*hf*state[j]);
    PHY->Quad[j] = sin(2.*PI*hf*state[j]);
  }
  if(!PHY->a1)
    PHY->ph += 0.5;
  else{
    PHY->ph -= 0.5;
  }
  PHY->a1 = databit;
}     /* end modulator */

/***********************************************************/ 
void Interference2::wmodulator(short int n, physical_layer *PHY, char databit) {
  char bit;
  double state[Ns];
  //static double ph,stateB[RSLENGTH];
  int i,j;
  char chips[11] = {1,0,1,1,0,1,1,1,0,0,0}; 
  char chip802[Ns]={0};
 
  bit=DiffEncode( PHY, databit);
  if(!n){
    PHY->ph = rand()/(double)RAND_MAX;
    PHY->ph *= (2*PI);
    for(i=0;i<RSLENGTH;i++)
      PHY->stateB802[i] = 0.;
  }
  for(j=0;j<11;j++){
    chip802[4*j]=(2*bit-1)*(2*chips[j]-1);
  }
  for( j=0;j<Ns;j++){
    state[j] =0;
    for(i=RSLENGTH-2;i>=0;i--)
      PHY->stateB802[i+1] = PHY->stateB802[i];
    PHY->stateB802[0] = chip802[j]; 
    for(i=0;i<RSLENGTH;i++){
      state [j] += PHY->stateB802[i] * PHY->hraised[i];
    }
  }
  for( j=0;j<Ns;j++){
    PHY->Inphase[j] = cos(PHY->ph)*state[j];
    PHY->Quad[j] = sin(PHY->ph)*state[j];
  }	
}     /* end modulator */

/***********************************************************/ 
void Interference2::w11modulator(short int n, physical_layer *PHY, short *databit) {
  double Istate[8*Nscck],Qstate[8*Nscck];
  unsigned int Buf8;
  static double ph1; 
  double phase[4]={0, PI, PI/2, 3*PI/2};
  double i1[8],q1[8];
  int i,j,k;

  if(n==224){
    PHY->ph *=0;
    for( i=0;i<RSLENGTH;i++){
      PHY->I1B[i] = 0;
      PHY->Q1B[i] = 0.;
    }
  }
  for(i=0;i<8*Nscck;i++){
    Istate[i]=0;
    Qstate[i]=0;
  }
  for(k=0;k<11;k++){
    Buf8=(databit[k/2]>> ((k*8)%16)) &0xff;
    PHY->ph=phase[Buf8&0x3];
    ph1 +=PHY->ph;
    if( ph1> (2*PI))
      ph1 = fmod(ph1,(2*PI));
    Buf8 >>=2;
    for( i=0;i<8;i++){
      i1[i] = cos(ph1)*PHY->CInphase[Buf8][i]-sin(ph1)*PHY->CQuad[Buf8][i];
      q1[i] =sin(ph1)*PHY->CInphase[Buf8][i]+cos(ph1)*PHY->CQuad[Buf8][i];
    }
    for( j=0;j<8*Nscck;j+=Nscck){
      Istate[j]=i1[j/Nscck];
      Qstate[j]=q1[j/Nscck];
    }
    for( j=0;j<8*Nscck;j++){
      PHY->Inphase[k*32+j] =0;
      PHY->Quad[k*32+j] =0;
      for(i=RSLENGTH-2;i>=0;i--){
	PHY->I1B[i+1] =PHY->I1B[i];
	PHY->Q1B[i+1] =PHY->Q1B[i];
      }
      PHY->I1B[0] = Istate[j];
      PHY->Q1B[0] = Qstate[j];
      for(i=0;i<RSLENGTH;i++){
	PHY->Inphase [k*32+j] += PHY->I1B[i] * PHY->hraised[i];
	PHY->Quad [k*32+j] += PHY->Q1B[i] * PHY->hraised[i];
      }/*for(i=0;i<RSLENGTH;i++){*/
    }/*for( j=0;j<8*Nscck;j++){*/
  }/*for(k=0;k<11;k++){*/

}     /* end modulator */

/***********************************************************/ 
char Interference2::DiffEncode( physical_layer *PHY, char bit){
  char temp;

  temp= PHY->diffbuf ^bit;
  PHY->diffbuf = temp; 

  return (temp);
				
}



 







 



