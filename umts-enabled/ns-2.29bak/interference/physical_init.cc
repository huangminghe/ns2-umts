/***********************************************************/
/*   physical-init.c  - contains the functions for the     */
/*                  Initialization of the physical layer.  */
/***********************************************************/

#include "physic.h"


void Interference2::initialize_physical_layer(physical_layer *PHY, int ColNum ) {

	double temp,a;
	int i;
	double phase[4]={0, PI, PI/2, 3*PI/2};
	double ph1,ph2,ph3,ph4;
	int d23,d45,d67;
  /* transmitter */

  double q[] ={
 .0003384128,.0007483472,.0012417407,.0018318097,.0025330436,.0033611672,.0043330691,.0054666923,      
 .0067808870,.0082952237,.0100297693,.0120048281,.0142406519,.0167571268,.0195734420,.0227077511,       
 .0261768345,.0299957735,.0341776458,.0387332534,.0436708907,.0489961618,.0547118514,.0608178568,       
 .0673111778,.0741859697,.0814336525,.0890430729,.0970007124,.1052909327,.1138962464,.1227976077,        
 .1319747047,.1414062494,.1510702515,.1609442679,.1710056206,.1812315788,.1915994984,.2020869202,
 .2126716262,.2233316570,.2340452932,.2447910090,.2555474036,.2662931193,.2770067556,.2876667864,
 .2982514925,.3087389142,.3191068338,.3293327920,.3393941448,.3492681611,.3589321632,.3683637078,        
 .3775408048,.3864421662,.3950474800,.4033377002,.4112953398,.4189047602,.4261524430,.4330272350,        
 .4395205560,.4456265612,.4513422510,.4566675220,.4616051594,.4661607670,.4703426394,.4741615782,        
 .4776306617,.4807649708,.4835812860,.4860977608,.4883335846,.4903086434,.4920431890,.4935575258,        
 .4948717204,.4960053436,.4969772456,.4978053692,.4985066031,.4990966721,.4995900656,.5000000000};  

  double hdf[6] = {0.0062, -0.0372, 0.4566, -0.4566, 0.0372, -0.0062};
 for ( i = 0; i<88;i++) 
  PHY->qcoef[i] = q[i]; 
  PHY->diffbuf=0;




/*CCK Table*/
for(i=0;i<64;i++){
	d23 = i&0x3;
	d45 = (i>>2)&0x3;
	d67=  (i>>4)&0x3;
	ph1=0;
    ph2=phase[d23];
	ph3=phase[d45];
	ph4=phase[d67];
	PHY->CInphase[i][0]=cos(ph1+ph2+ph3+ph4);
	PHY->CQuad[i][0]=sin(ph1+ph2+ph3+ph4);
	PHY->CInphase[i][1]=cos(ph1+ph3+ph4);
	PHY->CQuad[i][1]=sin(ph1+ph3+ph4);
	PHY->CInphase[i][2]=cos(ph1+ph2+ph4);
	PHY->CQuad[i][2]=sin(ph1+ph2+ph4);
	PHY->CInphase[i][3]=-cos(ph1+ph4);
	PHY->CQuad[i][3]=-sin(ph1+ph4);
	PHY->CInphase[i][4]=cos(ph1+ph2+ph3);
	PHY->CQuad[i][4]=sin(ph1+ph2+ph3);
	PHY->CInphase[i][5]=cos(ph1+ph3);
	PHY->CQuad[i][5]=sin(ph1+ph3);
	PHY->CInphase[i][6]=-cos(ph1+ph2);
	PHY->CQuad[i][6]=-sin(ph1+ph2);
	PHY->CInphase[i][7]=cos(ph1);
	PHY->CQuad[i][7]=sin(ph1);
	}


  /* channel and interference */
      PHY->x1=6666;PHY->x2=18888;PHY->x3=121;PHY->x4=178;PHY->x5=2140;PHY->x6=25000;
      PHY->x7=6066;PHY->x8=188;PHY->x9=10021;PHY->xa=1078;PHY->xb=21;PHY->xc=26540;


  /* receiver */
 for (  i = 0; i<6;i++) 
  PHY->hdiff[i] = hdf[i]; 

	/* IF Filter*/
 for ( i = 0; i < BPLENGTH ; i++){
	temp=   i - (BPLENGTH *1.)/2.+.5;
	temp /= Ns;
	PHY->hr[i]= sqrt(2)* Br * exp (-PI * 2 *Br * Br * temp *temp );
	}

    /* Differentiator */
 for (  i = 0; i<6;i++) 
	PHY->hdiff[i] = hdf[i];

    /* VTB */
  MatchFilter (PHY);
  SIQTable(PHY);

   /* void RaisedCosine(void){ */
        a=1.;/*Rolloff Factor*/
		for ( i = 0; i < RSLENGTH ; i++){
		 temp=   i - (RSLENGTH -1)/2.;
		 temp /= Ns;
		 temp *=11;
        if((temp) && (1-16*temp*temp*a*a))
			PHY->hraised[i]= ( sin(PI*(1-a)*temp) + 4*a*temp*cos (PI*(1+a)*temp) )
			/( (PI*temp) * (1-16*temp*temp*a*a) );
		if(!temp)
			PHY->hraised[i] = 1-a+4*a/PI;
		if(!(1-16*temp*temp*a*a)){
			PHY->hraised[i]= (a/sqrt(2))*((1+2/PI)*sin(PI/(4*a))+(1-2/PI)*cos(PI/(4*a)));
			}
			}/*	for (int i = 0; i < TDELAY; i++*/
   /* Interference   */
 	PHY->iseed=0x80000;
  


}   /* end */

/**********************************************/
void Interference2::reset_physical_layer(physical_layer *PHY, int ColNum ) {

}
/**********************************************/
void Interference2::MatchFilter (physical_layer *PHY){
double hi[Accesslength],hq[Accesslength];
int j;

hi[ 0]= 0.59455;hq[ 0]= 0.80406;
hi[ 1]=-0.39906;hq[ 1]= 0.91693;
hi[ 2]=-0.97276;hq[ 2]= 0.23181;
hi[ 3]=-0.68713;hq[ 3]= 0.72653;
hi[ 4]=-0.99361;hq[ 4]= 0.11287;
hi[ 5]=-0.68713;hq[ 5]=-0.72653;
hi[ 6]=-0.97276;hq[ 6]=-0.23181;
hi[ 7]=-0.68713;hq[ 7]=-0.72653;
hi[ 8]=-0.99361;hq[ 8]=-0.11287;
hi[ 9]=-0.59455;hq[ 9]= 0.80406;
hi[10]= 0.28563;hq[10]= 0.95834;
hi[11]=-0.28563;hq[11]= 0.95834;
hi[12]= 0.28563;hq[12]= 0.95834;
hi[13]=-0.39906;hq[13]= 0.91693;
hi[14]=-0.97276;hq[14]= 0.23181;
hi[15]=-0.59455;hq[15]= 0.80406;
hi[16]= 0.39906;hq[16]= 0.91693;
hi[17]= 0.97276;hq[17]= 0.23181;
hi[18]= 0.68713;hq[18]= 0.72653;
hi[19]= 0.99361;hq[19]= 0.11287;
hi[20]= 0.68713;hq[20]=-0.72653;
hi[21]= 0.97276;hq[21]=-0.23181;
hi[22]= 0.68713;hq[22]=-0.72653;
hi[23]= 0.99361;hq[23]=-0.11287;
hi[24]= 0.68713;hq[24]= 0.72653;
hi[25]= 0.99361;hq[25]= 0.11287;
hi[26]= 0.59455;hq[26]=-0.80406;
hi[27]=-0.28563;hq[27]=-0.95834;
hi[28]= 0.28563;hq[28]=-0.95834;
hi[29]=-0.39906;hq[29]=-0.91693;
hi[30]=-0.97276;hq[30]=-0.23181;
hi[31]=-0.68713;hq[31]=-0.72653;
hi[32]=-0.97276;hq[32]=-0.23181;
hi[33]=-0.59455;hq[33]=-0.80406;
hi[34]= 0.28563;hq[34]=-0.95834;
hi[35]=-0.39906;hq[35]=-0.91693;
hi[36]=-0.97276;hq[36]=-0.23181;
hi[37]=-0.59455;hq[37]=-0.80406;
hi[38]= 0.28563;hq[38]=-0.95834;
hi[39]=-0.39906;hq[39]=-0.91693;
hi[40]=-0.97276;hq[40]=-0.23181;
hi[41]=-0.68713;hq[41]=-0.72653;
hi[42]=-0.97276;hq[42]=-0.23181;
hi[43]=-0.68713;hq[43]=-0.72653;
hi[44]=-0.97276;hq[44]=-0.23181;
hi[45]=-0.68713;hq[45]=-0.72653;
hi[46]=-0.99361;hq[46]=-0.11287;
hi[47]=-0.59455;hq[47]= 0.80406;
hi[48]= 0.28563;hq[48]= 0.95834;
hi[49]=-0.39906;hq[49]= 0.91693;
hi[50]=-0.97276;hq[50]= 0.23181;
hi[51]=-0.59455;hq[51]= 0.80406;
hi[52]= 0.39906;hq[52]= 0.91693;
hi[53]= 0.99361;hq[53]= 0.11287;
hi[54]= 0.68713;hq[54]=-0.72653;
hi[55]= 0.97276;hq[55]=-0.23181;
hi[56]= 0.68713;hq[56]=-0.72653;
hi[57]= 0.99361;hq[57]=-0.11287;
hi[58]= 0.59455;hq[58]= 0.80406;
hi[59]=-0.39906;hq[59]= 0.91693;
hi[60]=-0.99361;hq[60]= 0.11287;
hi[61]=-0.59455;hq[61]=-0.80406;
hi[62]= 0.28563;hq[62]=-0.95834;
hi[63]=-0.39906;hq[63]=-0.91693;
PHY->Realambig[0]= 43.776;PHY->Imambig[0]=  2.504;
PHY->Realambig[1]= 64.000;PHY->Imambig[1]=  0.000;
PHY->Realambig[2]= 43.776;PHY->Imambig[2]=  2.504;
for (j = 0; j < Accesslength; j++){
		PHY->hhi[j] = hi[Accesslength-1-j];
		PHY->hhq[j] =-hq[Accesslength-1-j];
}/*for (short int i=0;i<totsamples-BPLENGTH;i++){*/

}

/*******************************
RcTable
The Last Two bits were ignored in each packet.
*******************************/
void Interference2::SIQTable(physical_layer *PHY)
{
int k;
double state[24][Ns],ph;
double ModI[24][Ns],ModQ[24][Ns];
int j,n;
union{
		 unsigned int i16;
		 struct {
		unsigned int i0:1;
		unsigned int i1:1;
			 }bit;
		}index;
                                
ph=0.;
for(n=0;n<=5;n++){
 for(index.i16=0;index.i16<=3;index.i16++){
	 for(j=0;j<=Ns-1;j++){
				  state[4*n+index.i16][j]=(1.0-2.0*index.bit.i1)*PHY->qcoef[j]+
				  (1.0-2.0*index.bit.i0)*PHY->qcoef[j+Ns]+ph;

				  ModI[4*n+index.i16][j]=cos(2.*PI*hf*state[4*n+index.i16][j]);
				  ModQ[4*n+index.i16][j]=sin(2.*PI*hf*state[4*n+index.i16][j]);
				  }/*for(j=0;j<=Ns-1;j++)*/
	  }/* for(index.i16=0;index.i16<=3;index.i16++){*/
 ph+=.5;
 }

for (k = 0; k < 24; k++){
		PHY->SI[k] = ModI[k][sampleTime];
		PHY->SQ[k] = ModQ[k][sampleTime];
	}/*	for (k = 0; k < 24; k++){ */
}


