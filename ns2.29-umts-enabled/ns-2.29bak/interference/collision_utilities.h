/*
** collision_utilities.h :
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

#ifndef __COLLISION_UTILITIES_H__
#define __COLLISION_UTILITIES_H__

/* Standard includes */
#include <stdio.h>
#include <math.h> 
#include <stdlib.h>

/* function prototypes */
class CollisionUtil {
 public:
  static void create_packet (int size_packet,char *table);
  static int compute_errors_number (char *start_table,char *final_table,int size_packet);
  static int  *errors_position (int *pos,char *start_table,char *final_table,int size_packet, int num_errs);
  //static void print_interference_collision_ptr (double** interfer_pointer, int size_of_collision);
  //static void print_tables (char *table,int packet_size);
  //static void random1 (void);
};

#endif /* end of __COLLISION_UTILITIES_H__*/
