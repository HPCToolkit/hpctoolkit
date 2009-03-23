// -*-Mode: C++;-*- // technically C99
// $Id$

//************************* System Include Files ****************************

#include <ucontext.h>
#include <assert.h>

//*************************** User Include Files ****************************

#include "x86-decoder.h"
#include "unwind.h"

//*************************** Forward Declarations **************************


//***************************************************************************
// interface functions
//***************************************************************************

static void *
actual_get_branch_target(void *ins, xed_decoded_inst_t *xptr,
		   xed_operand_values_t *vals)
{
  int bytes = xed_operand_values_get_branch_displacement_length(vals);
  int offset = 0;
  switch(bytes) {
  case 1:
    offset = (signed char) 
      xed_operand_values_get_branch_displacement_byte(vals,0);
    break;
  case 4:
    offset = xed_operand_values_get_branch_displacement_int32(vals);
    break;
  default:
    assert(0);
  }
  void *end_of_call_inst = ins + xed_decoded_inst_get_length(xptr);
  void *target = end_of_call_inst + offset;
  return target;
}


void *
x86_get_branch_target(void *ins, xed_decoded_inst_t *xptr)
{
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  xed_operand_type_enum_t op0_type = xed_operand_type(op0);

  if (op0_name == XED_OPERAND_RELBR && 
      op0_type == XED_OPERAND_TYPE_IMM_CONST) {
    xed_operand_values_t *vals = xed_decoded_inst_operands(xptr);

    if (xed_operand_values_has_branch_displacement(vals)) {
      void *vaddr = actual_get_branch_target(ins, xptr, vals);
      return vaddr;
    }
  }
  return NULL;
}
