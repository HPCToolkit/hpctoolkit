#ifndef x86_decoder_h
#define x86_decoder_h

/******************************************************************************
 * include files
 *****************************************************************************/

#include "xed-interface.h"

/******************************************************************************
 * type declarations
 *****************************************************************************/

typedef struct {
  xed_state_t xed_settings;
  int sp_reg;
  int bp_reg;
  int ip_reg;
} xed_control_t;


/******************************************************************************
 * macros 
 *****************************************************************************/
#define iclass(xptr) xed_decoded_inst_get_iclass(xptr)
#define iclass_eq(xptr, class) (iclass(xptr) == (class))


/******************************************************************************
 * global variables 
 *****************************************************************************/

extern xed_control_t x86_decoder_settings;



/******************************************************************************
 * interface operations
 *****************************************************************************/
void x86_family_decoder_init();

#endif
