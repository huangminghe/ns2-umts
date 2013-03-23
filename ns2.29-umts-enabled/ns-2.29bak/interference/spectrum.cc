/*
** Spectrum.ex.c :
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
 * Function:	spectrum_multiply
 *
 * Description:	Create a filter that is the product of the interferer's spectrum and
 *				the receiver input filter. The resulting filter has the same min
 *				and max frequencies as the interferer's spectrum.
 *
 * ParamIn:		double ispectrum[][2]
 *				interferer's spectrum
 *
 *				int isize
 *				size of the interferer spectrum (number of pairs)
 *
 *				double rspectrum[][2]
 *				receiver's filter
 *
 *				int rsize
 *				size of the receiver's spectrum (number of pairs)
 *
 *				double (*prod_spectrum)[][2]
 *				pointer to the product spectrum
 */

void Spectrum::spectrum_multiply (double ispectrum[][2], int isize, double rspectrum[][2], int rsize, double prod_spectrum[][2])
{
  int i, j; // loop variables
  double fLow = ispectrum[0][0];
  double fHigh = ispectrum[isize-1][0];

  
  /* init the prod spectrum */
  for (i=0; i<isize+rsize; i++)
    {
      prod_spectrum[i][0] = INFINITY_FREQ;
      prod_spectrum[i][1] = 0.0;
    }

  /* 
   * first multiply breakpoints of this filter with interpolated
   * values from the other filter
   */
  for (i=0; i<isize; i++)
    {
      /* copy the frequency of the ispectrum to the prod */
      prod_spectrum[i][0] = ispectrum[i][0];
      /* compute a Gain */
      prod_spectrum[i][1] = ispectrum[i][1] + spectrum_find_gain_at (ispectrum[i][0], rspectrum, rsize);
    }

  /*
   * now multiply breakpoints from other filter with
   * interpolated values from this filter, but only those that
   * fall within fLow and fHigh of this filter
   */
  for (i=0; i<rsize; i++)
    {
      j = i + isize;

      if (rspectrum[i][0] >= fHigh)
	{
	  break;
	}
      else if (rspectrum[i][0] > fLow)
	{
	  /* copy the frequency of the rspectrum to the prod */
	  prod_spectrum[j][0] = rspectrum[i][0];
	  /* compute a Gain */
	  prod_spectrum[j][1] = rspectrum[i][1] + spectrum_find_gain_at (rspectrum[i][0], ispectrum, isize);
	}
    }

}

/*
 * Function:	find_gain_at
 *
 * Description:	Return the gain at the given frequency.  Uses linear
 *				interpolation between breakpoints.
 *
 * ParamIn:		double frequency
 *				given frequency
 *
 *				double spectrum[][2]
 *				given spectrum to find a gain
 *
 *				int size
 *				size of the given spectrum
 *
 * ParamOut:	double gain
 *				gain found
 */

double Spectrum::spectrum_find_gain_at (double frequency, double spectrum[][2], int size)
{
  double gain = 0.0;
  int i; // loop variable

  
  if (frequency < spectrum[0][0]) 
    {
      gain = spectrum[0][1];
    } 
  else if (frequency >= spectrum[size-1][0])
    {
      gain = spectrum[size-1][1];
    }
  else
    {
      for (i=1; i<size; i++) 
	{
	  if (frequency == spectrum[i][0]) 
	    {
	      gain = spectrum[i][1];
	      break;
	    } 
	  else if (frequency < spectrum[i][0]) 
	    {
	      gain = spectrum_lerp (frequency, spectrum[i-1][0], spectrum[i][0], spectrum[i-1][1], spectrum[i][1]);
	      break;
	    }
	}
    }

  return (gain);
}


/*
 * Function:	spectrum_lerp
 *
 * Description:	Linear Interpolation
 */

double Spectrum::spectrum_lerp (double x, double x0, double x1, double y0, double y1)
{
  double m = (y1-y0) / (x1-x0);

 
  return (y0 + (x-x0) * m);
}


/*
 * Function:	spectrum_area
 *
 * Description:	Return the area under this filter
 *
 * ParamIn:		double spectrum[][2]
 *				given filter
 *
 *				int size
 *				size of the spectrum
 *
 * ParamOut:	double area
 *				area under the given filter
 */

double Spectrum::spectrum_area (double spectrum[][2], int size)
{
  double area = 0.0;
  int i; // loop variable
  double g0, g1, f0, f1;

 
  /* compute the area */
  for (i = 0; i<size-1; i++) 
    {
      g0 = spectrum[i][1];
      g1 = spectrum[i+1][1];
      f0 = spectrum[i][0];
      f1 = spectrum[i+1][0];

      if (f0 == INFINITY_FREQ || f1 == INFINITY_FREQ)
	{
	  break;
	}

      area += spectrum_db_trapezoid (g0, g1, f1-f0);
    }

  return (area);
}

/*
 * Function:	spectrum_db_trapezoid
 *
 * Description:	Return the area under the trapezoid with starting and ending
 * 				gain of d0 and d1 (dB), and width (Hertz).  This is where the
 * 				conversion from db to linear space happens. 
 *
 * How it works:
 *
 *				 Define x0 = d0 / 10 and x1 = d1 / 10.  This means the the
 * 				conversion from dB to linear is 10^x rather than 10^(d/10).
 *
 * 				The integral of 10^x from minus infinity to x0 is given by:
 *					10^x0 / log(10)
 * 				The definite integral of 10^x from x0 to x1 is given by:
 *					(10^x1 - 10^x0) / log(10)
 * 				W   e compute the integral using this formula as if width always
 * 				equalled x1-x0.  But since width in independant of x1-x0, we
 * 				perform a post-scaling to account for the difference.
 */

double Spectrum::spectrum_db_trapezoid (double d0, double d1, double width)
{
  double area;
  double aNatural;
  double wNatural;

  
  
  d0 *= 0.1;
  d1 *= 0.1;
  
  if (d0 == d1)
    {		// handle easy case
      area = pow (10, d0) * width;
      return (fabs(area));
    } 

  /*
   * aNatural is the definite integral under 10^x from x0 to x1,
   * assuming x0 = d0 and x1 = d1
   */
  aNatural = (pow(10, d1) - pow(10, d0))/LN_10;

  /* wNatural is the 'natural' width */
  wNatural = d1-d0;

  /* scale by the actual width. */
  area = aNatural * width / wNatural;
  
  return (fabs(area));
}

/*
 * Function:	sort_spectrum
 */

void Spectrum::spectrum_sort (double spectrum[][2], int size)
{
  int i,j; // loop variables
  double tmp;

 

  /* */
  for (i=0; i<size; i++)
    for (j=i+1; j<size; j++)
      {
	if (spectrum[j][0] < spectrum[i][0])
	  {
	    /* permute the two pairs */
	    tmp = spectrum[j][0];
	    spectrum[j][0] = spectrum[i][0];
	    spectrum[i][0] = tmp;

	    tmp = spectrum[j][1];
	    spectrum[j][1] = spectrum[i][1];
	    spectrum[i][1] = tmp;
	  }
	else if (spectrum[j][0] == spectrum[i][0])
	  {
	    spectrum[j][0] = INFINITY_FREQ; // erase the pairs
	    spectrum[i][1] = spectrum[j][1];
	    spectrum[j][1] = 0.0;
	    }
      }

}

/*
 * Function:	spectrum_display
 */

void Spectrum::spectrum_display (double spectrum[][2], int size)
{
  int i; // loop variable

 
  /* print out the spectrum value */
  printf ("Frequency\tGain\n");

  for (i=0; i<size; i++)
    if (spectrum[i][0] != INFINITY_FREQ)
      printf ("%.1f\t%f\n", spectrum[i][0], spectrum[i][1]);

 
}
