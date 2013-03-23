/*
** collision_utilities.ex.c :
**
** Bluetooth model in Opnet
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
** Primary Author:      ???
** Module description:  Bluetooth Common MAC Function Support
** Last Modification:   May, 18, 2000
*/

/* include header */
#include "collision_utilities.h"



/*
 * Function:	create_packet ()
 */
void CollisionUtil::create_packet (int size_packet, char *table)
{
  int i;
  
 
  
  table = (char*)malloc( size_packet * sizeof(char) );
  for(i = 0 ; i < size_packet ; i++)
    {
      if ( rand() > 2^15 - 1 )
	table[i] = 0 ;
      else 
	table[i] = 1;
    }
  
}


int CollisionUtil::compute_errors_number(char *start_table,char *final_table,int size_packet)
{
  int i,sum;
  int num_errors = 0 ;
  int payload_errors = 0;

  
  if (size_packet != 0)
    {
      for( i = 0 ; i < size_packet ; i++ )
	{
	  //printf("start_table[%d] = %d  |  final_table[%d] = %d\n",i,*(start_table + i),i,*(final_table + i));
	  sum = *(start_table + i) + *(final_table + i) ;
	  num_errors = num_errors + (sum % 2);
	  if (i>126)
	    payload_errors += (sum % 2);
	}
    }
  
  //printf(" errors %d \n",   num_errors);
  return(num_errors);
}

int * CollisionUtil::errors_position(int *pos,char *start_table,char *final_table,int size_packet, int num_errs)
{
  int i;
  int buf;
  int k ;

 
  k = 0;

  if(num_errs != 0 )
    {
     
      for( i = 0 ; i < size_packet ; i++ )
	{
	  buf =  *(start_table + i) + *(final_table + i);
	  buf = buf % 2;
	  if( buf != 0 )	
	   {
	     pos[k] = i  ;
	     k++;
	   }
	}
    }
  return (pos);
}


/*
void CollisionUtil::print_tables(char *table,int packet_size)
{
  int i;
 
  if ( (packet_size != 0) && (bt_message_print) )
    {
      printf("the packet is ");
      for ( i = 0 ; (i < packet_size)&&(i < 30) ; i++ )
	printf("  %d ",table[i]);
      printf(" \n");
    }
  
}


void CollisionUtil::print_interference_ptr(double** interfer_pointer, int size_of_collision)
{
  int k,l;
 
  for (k = 0; k < 6 ; k++ )
    {
      for(l = 0 ; l <= size_of_collision ; l++ )
	{
	  if (bt_message_print) printf("interfer_pointer[%d][%d] = %f \n",k,l,interfer_pointer[k][l]);
	}
    }
  
}
*/

/*
 * Function:	random1()
 */
/*
void random1 (void)
{
  int i;
 if (bt_message_print) printf(" rand is");
  for(i = 0; i < 15; i++)
    if (bt_message_print) printf(" %d", rand() );
  if (bt_message_print) printf(" \n");
}
*/


