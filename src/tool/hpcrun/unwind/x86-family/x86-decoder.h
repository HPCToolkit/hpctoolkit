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
} xed_control_t;


/******************************************************************************
 * macros 
 *****************************************************************************/

#define iclass(xptr) xed_decoded_inst_get_iclass(xptr)

#define iclass_eq(xptr, class) (iclass(xptr) == (class))

#define is_reg_bp(reg) \
  (((reg) == XED_REG_RBP) | ((reg) == XED_REG_EBP) | ((reg) == XED_REG_BP))

#define is_reg_sp(reg) \
  (((reg) == XED_REG_RSP) | ((reg) == XED_REG_ESP) | ((reg) == XED_REG_SP))

#define is_reg_ax(reg) \
  (((reg) == XED_REG_RAX) | ((reg) == XED_REG_EAX) | ((reg) == XED_REG_AX))



/******************************************************************************
 * global variables 
 *****************************************************************************/

extern xed_control_t x86_decoder_settings;



/******************************************************************************
 * interface operations
 *****************************************************************************/
void x86_family_decoder_init();

#endif
