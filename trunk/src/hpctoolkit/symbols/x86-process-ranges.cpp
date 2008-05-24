/******************************************************************************
 * include files
 *****************************************************************************/

#include <stdio.h>
#include <assert.h>
#include <string>

extern "C" {
#include "xed-interface.h"
};

#include "code-ranges.h"
#include "function-entries.h"
#include "process-ranges.h"



/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static void process_call(char *ins, xed_decoded_inst_t *xptr, long offset, 
			 void *start, void *end);

static void process_branch(char *ins, xed_decoded_inst_t *xptr);

static void after_unconditional(long offset, char *ins, xed_decoded_inst_t *xptr);



/******************************************************************************
 * local variables 
 *****************************************************************************/

static xed_state_t xed_machine_state_x86_64 = { XED_MACHINE_MODE_LONG_64, 
						XED_ADDRESS_WIDTH_64b,
						XED_ADDRESS_WIDTH_64b };


/******************************************************************************
 * interface operations 
 *****************************************************************************/

void
process_range_init()
{
  xed_tables_init();
}


void 
process_range(long offset, void *vstart, void *vend, bool fn_discovery)
{
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_error_enum_t xed_error;
  int error_count = 0;
  char *ins = (char *) vstart;
  char *end = (char *) vend;

  xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state_x86_64);

  while (ins < end) {

    xed_decoded_inst_zero_keep_mode(xptr);
    xed_error = xed_decode(xptr, (uint8_t*) ins, 15);

    if (xed_error != XED_ERROR_NONE) {
      error_count++; /* note the error      */
      ins++;         /* skip this byte      */
      continue;      /* continue onward ... */
    }

    xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);
    switch(xiclass) {
    case XED_ICLASS_CALL_FAR:
    case XED_ICLASS_CALL_NEAR:
      /* if (fn_discovery) */ process_call(ins, xptr, offset, vstart, vend);
      break;

    case XED_ICLASS_JMP: 
    case XED_ICLASS_JMP_FAR:
    case XED_ICLASS_RET_FAR:
    case XED_ICLASS_RET_NEAR:
      if (fn_discovery) after_unconditional(offset, ins ,xptr);
      break;

    case XED_ICLASS_JBE: 
    case XED_ICLASS_JL: 
    case XED_ICLASS_JLE: 
    case XED_ICLASS_JNB:
    case XED_ICLASS_JNBE: 
    case XED_ICLASS_JNL: 
    case XED_ICLASS_JNLE: 
    case XED_ICLASS_JNO:
    case XED_ICLASS_JNP:
    case XED_ICLASS_JNS:
    case XED_ICLASS_JNZ:
    case XED_ICLASS_JO:
    case XED_ICLASS_JP:
    case XED_ICLASS_JRCXZ:
    case XED_ICLASS_JS:
    case XED_ICLASS_JZ:
      if (fn_discovery) process_branch(ins + offset, xptr);
      break;

    default:
      break;
    }

    ins += xed_decoded_inst_get_length(xptr);
  }
}



/******************************************************************************
 * private operations 
 *****************************************************************************/

static int 
is_padding(int c)
{
  return (c == 0x66) || (c == 0x90);
}


static void 
after_unconditional(long offset, char *ins, xed_decoded_inst_t *xptr)
{
  ins += xed_decoded_inst_get_length(xptr);
  unsigned char *uins = (unsigned char *) ins;
  if (is_padding(*uins++)) { // try always adding
    for (; is_padding(*uins); uins++); // skip remaining padding 
    add_stripped_function_entry(uins + offset); // potential function entry point
  }
}


static void *
get_branch_target(char *ins, xed_decoded_inst_t *xptr, xed_operand_values_t *vals)
{
  int bytes = xed_operand_values_get_branch_displacement_length(vals);
  int offset = 0;
  switch(bytes) {
  case 1:
    offset = (signed char) xed_operand_values_get_branch_displacement_byte(vals,0);
    break;
  case 4:
    offset = xed_operand_values_get_branch_displacement_int32(vals);
    break;
  default:
    assert(0);
  }
  char *end_of_call_inst = ins + xed_decoded_inst_get_length(xptr);
  char *target = end_of_call_inst + offset;
  return target;
}


static void 
process_call(char *ins, xed_decoded_inst_t *xptr, long offset, void *start, 
	     void *end)
{ 
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  xed_operand_type_enum_t op0_type = xed_operand_type(op0);

  if (op0_name == XED_OPERAND_RELBR && 
      op0_type == XED_OPERAND_TYPE_IMM_CONST) {
    xed_operand_values_t *vals = xed_decoded_inst_operands(xptr);

    if (xed_operand_values_has_branch_displacement(vals)) {
      void *vaddr = get_branch_target(ins + offset,xptr,vals);
      if (consider_possible_fn_address(vaddr)) add_stripped_function_entry(vaddr);
    }

  }
}


static void 
process_branch(char *ins, xed_decoded_inst_t *xptr)
{ 
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  xed_operand_type_enum_t op0_type = xed_operand_type(op0);

  if (op0_name == XED_OPERAND_RELBR && 
      op0_type == XED_OPERAND_TYPE_IMM_CONST) {
    xed_operand_values_t *vals = xed_decoded_inst_operands(xptr);

    if (xed_operand_values_has_branch_displacement(vals)) {
      void *target = get_branch_target(ins,xptr,vals);
      void *start, *end;
      if (target < ins) {
	start = target;
	end = ins; 
      } else {
	start = ins;
	end = ((char *) target) + 1; // add one to ensure that the branch target is part of the "covered" range
      }
      add_branch_range(start, end);
    }
  }
}
