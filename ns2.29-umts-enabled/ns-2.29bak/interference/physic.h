#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "packet.h"
#include "collision_utilities.h"

#define IB1 1
#define IB3 4
#define IB20 524288
#define MASK IB3
#define Ns 44
#define hf 1./3.
#ifndef PI
#define PI 3.141592654
#endif
#define INTPHASE 0
#define ALPHA_N_1 0
#define BPLENGTH 3*Ns-1
#define RSLENGTH 33
#define DIFFLENGTH 6 
#define Br 1.1
#define RBUF 2*Ns
#define DLY 31
#define DLY11 32
#define sensitivity -80
#define Nscck 4
#define sampleTime Ns/2-1
#define Accesslength  64
#define Spsition 0  
#define lenmax 400
#define totsamples 10


/*********************************************************************/
/* physics.h - contains initializion for the Bluetoth		     */
/*       	   physical layer				     */
/*********************************************************************/

#ifndef PHYLAYER
#define PHYLAYER

typedef struct {
  /*  Interface   */
  
  char sw; 
  

  /* transmitter parameters  */
  
  double qcoef[88];
  double a1;
  double ph;
  double Inphase[8*Ns], Quad[8*Ns];
  char diffbuf;
  double CInphase[256][8],CQuad[256][8];
  double phcck,I1B[RSLENGTH],Q1B[RSLENGTH],stateB802[RSLENGTH];
  /*	double Inphasecck[11*8*Nscck],Quadcck[11*8*Nscck];*/
  
  /* channel parameters   */
  int    x1,x2,x3,x4,x5,x6;
  int    x7,x8,x9,xa,xb,xc;
  double n0,pr1;
  
  
  /* receiver parameters  */
  double wIM[RSLENGTH], wQM[RSLENGTH];
  double wIMD[RBUF], wQMD[RBUF];
  double IM[BPLENGTH], QM[BPLENGTH];
  double IMD[DIFFLENGTH], QMD[DIFFLENGTH];
  double hr[BPLENGTH], hdiff[11], hraised[RSLENGTH];
  double XMem,YMem;
  /*VTB*/
  double hhi[64],hhq[64];
  double Realambig[3],Imambig[3];
  double SI[4],SQ[24];
  double inphases[lenmax],quads[lenmax];
  double InphaseF[Ns],QuadF[Ns];
  
  /* Interference parameters  */
  unsigned int iseed;
  double badj[100];
  double phInt[100],stateInt802[100][RSLENGTH];
  short int TdIntBT[100];
  short int TdInt802[100];
  double a1Int[100]; 
  double AdjIntQuadB[100][2*Ns], AdjIntInphaseB[100][2*Ns];
  int IntCounter[100];
  
  /* Temp  */
  /*char ppacket[160];*/
  
} physical_layer;


class Interference2 {
 public:
  static int interference2(Packet *pktRx, double distRT, Packet *pktInt, double distRI, double difftime, int bypass);

  static void initialize_physical_layer(physical_layer *, int );
  static void reset_physical_layer(physical_layer *, int );
  static void DataGen(char *, int, physical_layer * );
  static void DataGenVTB(char *, int, physical_layer * );
  static void Wlan_bt_collision(physical_layer *, char *, double **, int, char);
  static void Wlan_bt_collision_VTB(physical_layer *, char *, double **, int, char);
  static void VTB_Rayleigh(physical_layer *, char *, double **, int, char);
  static void Bt802_Wlan1_collision(physical_layer *, char *, double **, int, char);
  static void Bt802_Wlan11_collision(physical_layer *, char *, double **, int, char);
  static void Bt_WlanFH1_collision(physical_layer *, char *, double **, int, char);
  static void Interface( physical_layer *, double **);
  static void wInterface( physical_layer *, double **);
  static void wInterface11( physical_layer *, double **);
  static void Interfere(short int, physical_layer *, double **, int); 
  static void Interfere11(short int, physical_layer *, double **, int); 
  static void IntBypassB(physical_layer *, char *,double **,int);
  static void IntBypassB_VTB(physical_layer *, char *,double **,int);
  static void IntBypassW(physical_layer *, char *,double **,int);
  static void IntBypassW11(physical_layer *, char *,double **,int);
  static void Tx80211(char *d,double ,physical_layer *, int, int, char);
  static void AdjIntRead(char *,double,physical_layer *, int, int, char);
  static void Bypass(physical_layer *, char *, double **);
  static void Bypass_VTB(physical_layer *, char *, double **);
  static void modulator(short int, physical_layer *, char );
  static void Channel_AWGN(physical_layer *);
  static void Channel_Rayleigh(physical_layer *);
  static void Channel_AWGN11(physical_layer *);
  static void NoiseGen(physical_layer *, int);
  static char Demodulator_Ldid(short int, physical_layer *);
  static char Demodulator_DBPSK(short int, physical_layer *);
  static void Demodulator_CCK(short int, physical_layer *, int *);
  static char receiver (short int ,physical_layer *);
  static void wBypass(physical_layer *, char *, double **);
  static void w11Bypass(physical_layer *, char *, double **);
  static void wmodulator(short int, physical_layer *, char );
  static void w11modulator(short int, physical_layer *, short *);
  static char DiffEncode( physical_layer *, char );
  static char wreceiver (short int ,physical_layer *);
  static void w11receiver (short int ,physical_layer *, int *);
  static double  Uniform( physical_layer *);
  static void MatchFilter (physical_layer *);
  static void SIQTable(physical_layer *);
  static void Sampler (int ,physical_layer *);
  //void Receiver_VTB (physical_layer *,char *,double **);
  static char Receiver_VTB (short int ,physical_layer *);
  static void ChanRespConv(physical_layer *,double **);
};

#endif	

