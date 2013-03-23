/***********************************************************/
/*   physics-rx.c  - contains the functions for the         */
/*                  Receiver.                               */
/***********************************************************/

#include "physic.h"

char Interference2::Demodulator_Ldid(short int n, physical_layer *PHY){
  char decode;
  decode =	receiver (n, PHY);
  return decode;
}

/*******************************
receiver
The Last Two bits were ignored in each packet.
*******************************/
char Interference2::receiver (short int n,physical_layer *PHY){
  double X[Ns] , Y[Ns];
  double Xdot[Ns] , Ydot[Ns]; 
  double Phidot[Ns],Phi;
  short int i, j;
  char decod;

  /*****************************************
IF Filter
  *****************************************/
  if(!n){ 
    for (i = 0; i < BPLENGTH; i++){
      PHY->IM[i] = 0;PHY->QM[i] = 0;
    }/*for (short int i=0;i<BPLENGTH-1;i++){*/
  }
	
  for (i = 0; i < Ns; i++){
    X[i] = 0.;
    Y[i] = 0.;
    for ( j = BPLENGTH -2; j >=0 ; j--){
      PHY->IM[j+1] = PHY->IM[j];
      PHY->QM[j+1] = PHY->QM[j];
    }/*for (short int j=0;j<BPLENGTH;j++){*/
    PHY->IM[0] = PHY->Inphase[i]; 
    PHY->QM[0] = PHY->Quad[i]; 
    for ( j = 0; j < BPLENGTH; j++){
      X[i] += PHY->IM[j]*PHY->hr[BPLENGTH-j-1];
      Y[i] += PHY->QM[j]*PHY->hr[BPLENGTH-j-1];
    }/*for (short int j=0;j<BPLENGTH;j++){*/
  }/*for (short int i=0;i<totsamples-BPLENGTH;i++){*/


  /********************************************
Diffrentiator
  ***********************************************/
  if(!n){ 
    for (i = 0; i < DIFFLENGTH ; i++){
      PHY->IMD[i] = 0;PHY->QMD[i] = 0;
    }/*for (short int i=0;i<BPLENGTH-1;i++){*/
  }
	
  for (i = 0; i < Ns; i++){
    Xdot[i] =0.;
    Ydot[i] =0.;
    for ( j = DIFFLENGTH-2; j >=0 ; j--){
      PHY->IMD[j+1] = PHY->IMD[j];
      PHY->QMD[j+1] = PHY->QMD[j];
    }/*for (short int j=0;j<BPLENGTH;j++){*/
    PHY->IMD[0] = X[i]; 
    PHY->QMD[0] = Y[i]; 
    for ( j = 0; j < DIFFLENGTH; j++){
      Xdot[i] += PHY->IMD[j]*PHY->hdiff[j];
      Ydot[i] += PHY->QMD[j]*PHY->hdiff[j];
    }/*for (short int j=0;j<BPLENGTH;j++){*/
  }/*/for (short int i=0;i<totsamples-BPLENGTH;i++){*/


  /*********************************
LimiterDiscriminator
  ***********************************/
  for (i = 0; i < Ns; i++){
    Phidot[i] = ( X[i]*Ydot[i] - Y[i]*Xdot[i] )/( X[i]*X[i] + Y[i]*Y[i] )  ;
  }/*for (i = 0; i < totsamples-16; i++){*/
  /*********************************
Integrate and dump Fillter and detection
  **********************************/
  Phi = 0;
  for (j = 0; j < Ns; j++){
    Phi += Phidot[j];
  }

  if(Phi < 0){
    decod= 1;
  }/*if*/
  else if(Phi>= 0){
    decod= 0;
  }/*else if*/	
  return decod ; 
}

/*******************************/
void Interference2::Demodulator_CCK(short int n, physical_layer *PHY, int *recbit){
  w11receiver (n, PHY, recbit);
}

/*******************************
receiver
The Last Two bits were ignored in each packet.
*******************************/
void Interference2::w11receiver (short int n,physical_layer *PHY, int *decod){
  double X[11*8*Nscck] , Y[11*8*Nscck];
  /*		static	double XB[2*11*8] , YB[2*11*8];*/
  static	double IM[RSLENGTH], QM[RSLENGTH];
  double x11[4][2],x33[16][2],x22[8][2],xr[32];
  double max,ph1,ph2,ph3,ph4,ii,qq,phb;
  static double ph0;
  double i1[4]={1,-1,0,0};
  double q1[4]={0,0,1,-1};
  int    d1[4]={0,1,3,2};
  double i2[4]={0,-1,0,1};
  double q2[4]={-1,0,1,0};
  int    d2[4]={2,1,3,0};
  double i3[4]={1,0,-1,0};
  double q3[4]={0,1,0,-1};
  int    d3[4]={0,3,1,2};
  unsigned short int i, k,d10,d32,d54,d76;
  int j,t;
  double cc;
  /*FILE *fp;*/

  for (i=0;i<6;i++)
    decod[i] = 0;

  for (i = 0; i < 11*8*Nscck; i++){
    X[i] = 0.;
    Y[i] = 0.;
    for ( j = RSLENGTH -2; j >=0 ; j--){
      IM[j+1] = IM[j];
      QM[j+1] = QM[j];
    }/*for (short int j=0;j<BPLENGTH;j++){*/
    IM[0] = PHY->Inphase[i]; 
    QM[0] = PHY->Quad[i];
    for ( j = 0; j < RSLENGTH; j++){
      X[i] += IM[j]*PHY->hraised[RSLENGTH-j-1];
      Y[i] += QM[j]*PHY->hraised[RSLENGTH-j-1];
    }/*for (short int j=0;j<BPLENGTH;j++){*/
  }/*for (short int i=0;i<totsamples-BPLENGTH;i++){*/
  if(n==224){ 
    ph0=0;
  }/*if(!n){*/

  for (t = 0; t < 11; t++){
    for (i = 0; i < 8; i++){
      PHY->Inphase[i]=X[i*Nscck+t*32];
      PHY->Quad[i]=Y[i*Nscck+t*32];
    }
    PHY->Inphase[3]=-PHY->Inphase[3];
    PHY->Quad[3]=-PHY->Quad[3];
    PHY->Inphase[6]=-PHY->Inphase[6];
    PHY->Quad[6]=-PHY->Quad[6];
    max=0;
    for ( i = 0; i <4; i++){
      for (j = 0; j < 4; j++){
	x11[j][0] =(PHY->Inphase[2*j]*i1[i]-PHY->Quad[2*j]*q1[i]);
	x11[j][1] =(PHY->Inphase[2*j]*q1[i]+PHY->Quad[2*j]*i1[i]);
	x11[j][0] +=PHY->Inphase[2*j+1];
	x11[j][1] +=PHY->Quad[2*j+1];
      }
      for ( j = 0; j <4; j++){
	x22[j][0] =(x11[0][0]*i2[j]-x11[0][1]*q2[j]);
	x22[j][1] =(x11[0][0]*q2[j]+x11[0][1]*i2[j]);
	x22[j][0] +=x11[1][0];
	x22[j][1] +=x11[1][1];
      }
      for ( j = 4; j <8; j++){
	x22[j][0] =(x11[2][0]*i2[j-4]-x11[2][1]*q2[j-4]);
	x22[j][1] =(x11[2][0]*q2[j-4]+x11[2][1]*i2[j-4]);
	x22[j][0] +=x11[3][0];
	x22[j][1] +=x11[3][1];
      }
      for ( j = 0; j <4; j++){
	x33[j][0] =(x22[0][0]*i3[j]-x22[0][1]*q3[j]);
	x33[j][1] =(x22[0][0]*q3[j]+x22[0][1]*i3[j]);
	x33[j][0] +=x22[4][0];
	x33[j][1] +=x22[4][1];
      }
      for ( j = 4; j <8; j++){
	x33[j][0] =(x22[1][0]*i3[j-4]-x22[1][1]*q3[j-4]);
	x33[j][1] =(x22[1][0]*q3[j-4]+x22[1][1]*i3[j-4]);
	x33[j][0] +=x22[5][0];
	x33[j][1] +=x22[5][1];
      }
      for ( j = 8; j <12; j++){
	x33[j][0] =(x22[2][0]*i3[j-8]-x22[2][1]*q3[j-8]);
	x33[j][1] =(x22[2][0]*q3[j-8]+x22[2][1]*i3[j-8]);
	x33[j][0] +=x22[6][0];
	x33[j][1] +=x22[6][1];
      }
      for ( j = 12; j <16; j++){
	x33[j][0] =(x22[3][0]*i3[j-12]-x22[3][1]*q3[j-12]);
	x33[j][1] =(x22[3][0]*q3[j-12]+x22[3][1]*i3[j-12]);
	x33[j][0] +=x22[7][0];
	x33[j][1] +=x22[7][1];
      }
      for ( j = 0; j <32; j++)
	xr[j]=x33[j/2][j%2];
      for ( j = 0; j <32; j+=2){
	cc=xr[j]*xr[j]+xr[j+1]*xr[j+1];
	if(cc>=max){
	  max=cc;
	  k=j/2;
	  ii=xr[j];
	  qq=xr[j+1];
	  ph1=atan2(xr[j+1],xr[j]);
	  ph2=atan2(-q1[i],i1[i]);
	  d32=d1[i];
	  ph4=atan2(-q3[k%4],i3[k%4]);
	  d76=d3[k%4];
	  ph3=atan2(-q2[k/4],i2[k/4]);
	  d54=d2[k/4];
	}/*	if(cc>=max){*/
      }/*for ( j = 0; j <32; j+=2){*/
    }/*		for ( i = 0; i <4; i++){*/
    k=(d76<<4)|(d54<<2)|d32;
    if(ph1<0)
      ph1+=2*PI;
    phb=ph1;
    ph1-=ph0;
    ph0=phb;
    if(ph1<0)
      ph1+=2*PI;
    if((0<=ph1) &&(ph1<PI/4))
      d10=0;
    if((7*PI/4<=ph1) &&(ph1<2*PI))
      d10=0;
    if((PI/4<=ph1) &&(ph1<3*PI/4))
      d10=2;
    if((3*PI/4<=ph1) &&(ph1<5*PI/4))
      d10=1;
    if((5*PI/4<=ph1) &&(ph1<7*PI/4))
      d10=3;

    k<<=2;
    k |=d10;

    k <<=((t*8)%16);
    decod[(t*8)/16] |= k;

  }/*	for (t = 0; t < 11; t++){*/


}

/*******************************/
char Interference2::Demodulator_DBPSK(short int n, physical_layer *PHY){
  char decode;
  decode =	wreceiver (n, PHY);
  return decode;
}

/*******************************
receiver
The Last Two bits were ignored in each packet.
*******************************/
char Interference2::wreceiver (short int n,physical_layer *PHY){
  double X[Ns] , Y[Ns];
  double Xdes, Ydes,Xdec,Ydec; 
  short int i, j;
  double NrzChip[11]={1,-1,1,1,-1,1,1,1,-1,-1,-1};
  double N44Chip[44];
  char decod;

  if(!n){ 
    for (i = 0; i < RSLENGTH; i++){
      PHY->wIM[i] = 0;PHY->wQM[i] = 0;
    }/*for (short int i=0;i<BPLENGTH-1;i++){*/
    PHY->XMem = -44;
    PHY->YMem =-44;
			
  }/*if(!n){ */
  for (i = 0; i < Ns; i++){
    X[i] = 0.;
    Y[i] = 0.;
    for ( j = RSLENGTH -2; j >=0 ; j--){
      PHY->wIM[j+1] = PHY->wIM[j];
      PHY->wQM[j+1] = PHY->wQM[j];
    }/*for (short int j=0;j<BPLENGTH;j++){*/
    PHY->wIM[0] = PHY->Inphase[i]; 
    PHY->wQM[0] = PHY->Quad[i];
    /*		cc += (Inphase802[i]*Inphase802[i]+Quad802[i]*Quad802[i]);*/
    for ( j = 0; j < RSLENGTH; j++){
      X[i] += PHY->wIM[j]*PHY->hraised[RSLENGTH-j-1];
      Y[i] += PHY->wQM[j]*PHY->hraised[RSLENGTH-j-1];
    }/*for (short int j=0;j<BPLENGTH;j++){*/
  }/*for (short int i=0;i<totsamples-BPLENGTH;i++){*/

  for(i = 0;i<Ns;i++)
    N44Chip[i]=NrzChip[i/4];
  if(!n){ 
    for (i = 0; i < Ns-DLY ; i++){
      PHY->wIMD[i] = X[i+DLY];
      PHY->wQMD[i] = Y[i+DLY];
    }/*for ( int i=0;i<Ns;i++){*/
  }/*if(!n){*/
  else if(n){
    Xdes=Ydes=0;

    for (i = 0; i < Ns; i++){
      PHY->wIMD[i+Ns-DLY]=X[i];
      PHY->wQMD[i+Ns-DLY]=Y[i];
      /*			cc += (X[i]*X[i]+Y[i]*Y[i]);*/
    }/*	for (i = 0; i < Ns; i++){*/
    for (i = 0; i < Ns; i++){
      Xdes +=(PHY->wIMD[i] *N44Chip[i]);
      Ydes +=(PHY->wQMD[i] *N44Chip[i]);
    }/*for (short int i=0;i<11;i++){*/
    for (i = 0; i < Ns-DLY; i++){
      PHY->wIMD[i] = PHY->wIMD[i+Ns];
      PHY->wQMD[i] = PHY->wQMD[i+Ns];
    }
    Xdec =  PHY->XMem * Xdes;/*diffdecod*/
    Ydec =  PHY->YMem * Ydes;
    PHY->XMem = Xdes;
    PHY->YMem = Ydes;
    /*		cc+=(Xdes*Xdes)+(Ydes*Ydes);*/

    if(Xdec+Ydec <= 0){
      decod =1;
    }/*if*/
    else if(Xdec+Ydec > 0){
      decod = 0;
    }/*else if	*/
  }/*	else if(n){	*/
		
  return decod;	

}

/*******************************
receiver
The Last Two bits were ignored in each packet.
*******************************/
char Interference2::Receiver_VTB (short int n,physical_layer *PHY){
  double X[Ns] , Y[Ns];
  double Xdot[Ns] , Ydot[Ns]; 
  double Phidot[Ns],Phi;
  short int i, j;
  char decod;

  /*****************************************
IF Filter
  *****************************************/
  if(!n){ 
    for (i = 0; i < BPLENGTH; i++){
      PHY->IM[i] = 0;PHY->QM[i] = 0;
    }/*for (short int i=0;i<BPLENGTH-1;i++){*/
  }
	
  for (i = 0; i < Ns; i++){
    X[i] = 0.;
    Y[i] = 0.;
    for ( j = BPLENGTH -2; j >=0 ; j--){
      PHY->IM[j+1] = PHY->IM[j];
      PHY->QM[j+1] = PHY->QM[j];
    }/*for (short int j=0;j<BPLENGTH;j++){*/
    PHY->IM[0] = PHY->Inphase[i]; 
    PHY->QM[0] = PHY->Quad[i]; 
    for ( j = 0; j < BPLENGTH; j++){
      X[i] += PHY->IM[j]*PHY->hr[BPLENGTH-j-1];
      Y[i] += PHY->QM[j]*PHY->hr[BPLENGTH-j-1];
    }/*for (short int j=0;j<BPLENGTH;j++){*/
  }/*for (short int i=0;i<totsamples-BPLENGTH;i++){*/


  /********************************************
Diffrentiator
  ***********************************************/
  if(!n){ 
    for (i = 0; i < DIFFLENGTH ; i++){
      PHY->IMD[i] = 0;PHY->QMD[i] = 0;
    }/*for (short int i=0;i<BPLENGTH-1;i++){*/
  }
	
  for (i = 0; i < Ns; i++){
    Xdot[i] =0.;
    Ydot[i] =0.;
    for ( j = DIFFLENGTH-2; j >=0 ; j--){
      PHY->IMD[j+1] = PHY->IMD[j];
      PHY->QMD[j+1] = PHY->QMD[j];
    }/*for (short int j=0;j<BPLENGTH;j++){*/
    PHY->IMD[0] = X[i]; 
    PHY->QMD[0] = Y[i]; 
    for ( j = 0; j < DIFFLENGTH; j++){
      Xdot[i] += PHY->IMD[j]*PHY->hdiff[j];
      Ydot[i] += PHY->QMD[j]*PHY->hdiff[j];
    }/*for (short int j=0;j<BPLENGTH;j++){*/
  }/*/for (short int i=0;i<totsamples-BPLENGTH;i++){*/


  /*********************************
LimiterDiscriminator
  ***********************************/
  for (i = 0; i < Ns; i++){
    Phidot[i] = ( X[i]*Ydot[i] - Y[i]*Xdot[i] )/( X[i]*X[i] + Y[i]*Y[i] )  ;
  }/*for (i = 0; i < totsamples-16; i++){*/
  /*********************************
Integrate and dump Fillter and detection
  **********************************/
  Phi = 0;
  for (j = 0; j < Ns; j++){
    Phi += Phidot[j];
  }

  if(Phi < 0){
    decod= 1;
  }/*if*/
  else if(Phi>= 0){
    decod= 0;
  }/*else if*/	
  return decod ; 
}
