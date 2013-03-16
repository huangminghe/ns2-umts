/*
** Binomial.ex.c :
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
 * Function:	binomial_factorial
 *
 * Description:	classic factorial.
 */

double Binomial::binomial_factorial (int m)
{
  double factorial = 1.0;
  int i;

  
  for (i=1; i<=m; i++)
    factorial *= i;

  return (factorial);
}

/*
 * Function:	binomial_choose
 *
 * Description:	binomial_choose (m, j) returns "m choose j", the binomial distribution.
 *				Generates and caches an internal lookup table for a specific value of m,
 *				so repeated calls with the same value of m are efficient.
 *
 * ParamIn:	   	int m, int j
 *				given integer of the function
 *
 * ParamOut		int binomial_coeffs[j]
 *				value of "m choose j"
 */

int Binomial::binomial_choose (int m, int j)
{
  static int binomial_fM = 0;
  static int * binomial_coeffs = NULL;
  int i; // loop variable

  
  
  /* check for valid arguments */
  if ((m < 0) || (j < 0) || (j > m)) 
    {
      return (-1);
    }
  
  if ((m != binomial_fM) && (binomial_coeffs != NULL))
    {
      free (binomial_coeffs);
      binomial_coeffs = NULL;
    }

  if (binomial_coeffs == NULL)
    {
      binomial_fM = m;
      binomial_coeffs = (int *) malloc ((m+1)*sizeof(int));
      
      for (i=0; i<=m; i++)
	binomial_coeffs[i] = (int) floor (binomial_factorial (m)/(binomial_factorial (i) * binomial_factorial (m-i)));
    }

  return (binomial_coeffs[j]);
}
