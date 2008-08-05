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
 * macros
 *****************************************************************************/

#define is_aligned(addr) ((((unsigned long) (addr)) & 0x3) == 0) 


#define is_call_iclass(x) \
   ((x == XED_ICLASS_CALL_FAR) || (x == XED_ICLASS_CALL_NEAR))



/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static void process_call(char *ins, long offset, xed_decoded_inst_t *xptr,
			 void *start, void *end);

static void process_branch(char *ins, long offset, xed_decoded_inst_t *xptr);

static void after_unconditional(char *ins, long offset, xed_decoded_inst_t *xptr);

static bool is_mov_rax(unsigned char *ins);


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
  xed_iclass_enum_t prev_xiclass = XED_ICLASS_INVALID;

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
      /* if (fn_discovery) */ process_call(ins, offset, xptr, vstart, vend);
      break;

    case XED_ICLASS_JMP: 
    case XED_ICLASS_JMP_FAR:
      if (fn_discovery  && !is_call_iclass(prev_xiclass)) {
        // regarding use of !is_call above: don't infer function start 
        // if we run into code from C++ that consists of a call followed 
        // by an unconditional jump
	after_unconditional(ins, offset, xptr);
      }
      break;
    case XED_ICLASS_RET_FAR:
    case XED_ICLASS_RET_NEAR:
      if (fn_discovery) after_unconditional(ins, offset, xptr);
      break;

    case XED_ICLASS_JB:
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
      if (fn_discovery) process_branch(ins, offset , xptr);
      break;

    default:
      break;
    }
    prev_xiclass = xiclass;

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
after_unconditional(char *ins, long offset, xed_decoded_inst_t *xptr)
{
  ins += xed_decoded_inst_get_length(xptr);
  unsigned char *uins = (unsigned char *) ins;
  unsigned char *potential_function_addr = uins + offset;
  for (; is_padding(*uins); uins++); // skip remaining padding 

  // only infer a function entry before padding bytes if there isn't already one after padding bytes
  if (contains_function_entry(uins + offset) == false) {
    if (is_aligned(uins + offset) && !is_mov_rax(uins)) {
      add_stripped_function_entry(potential_function_addr); // potential function entry point
    }
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
process_call(char *ins, long offset, xed_decoded_inst_t *xptr, void *start, void *end)
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
process_branch(char *ins, long offset, xed_decoded_inst_t *xptr)
{ 
  char *relocated_ins = ins + offset; 
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  xed_operand_type_enum_t op0_type = xed_operand_type(op0);

  if (op0_name == XED_OPERAND_RELBR && 
      op0_type == XED_OPERAND_TYPE_IMM_CONST) {
    xed_operand_values_t *vals = xed_decoded_inst_operands(xptr);

    if (xed_operand_values_has_branch_displacement(vals)) {
      char *target = (char *) get_branch_target(relocated_ins, xptr, vals);
      void *start, *end;
      if (target < relocated_ins) {
        unsigned char *tloc = (unsigned char *) target - offset;
        for (; is_padding(*(tloc-1)); tloc--) { // extend branch range to before padding
            target--;
	}
	start = target;
	end = relocated_ins; 
      } else {
	start = relocated_ins;
	end = ((char *) target) + 1; // add one to ensure that the branch target is part of the "covered" range
      }
      add_branch_range(start, end);
    }
  }
}



static bool
is_mov_rax(unsigned char *ins)
{
  xed_decoded_inst_t xdi;

  xed_decoded_inst_zero_set_mode(&xdi, &xed_machine_state_x86_64);
  xed_error_enum_t xed_error = xed_decode(&xdi, (uint8_t*) ins, 15);

  if ((xed_error == XED_ERROR_NONE) && 
      (xed_decoded_inst_get_iclass(&xdi) == XED_ICLASS_MOV)) {
    const xed_inst_t *xi = xed_decoded_inst_inst(&xdi);
    const xed_operand_t *op1 =  xed_inst_operand(xi, 1);
    xed_operand_enum_t   op1_name = xed_operand_name(op1);

    if ((op1_name == XED_OPERAND_REG1) && 
	(xed_decoded_inst_get_reg(&xdi, op1_name) == XED_REG_RAX)) {
      return true;
    }
  }

  return false;
}



void x86_dump_ins(void *ins)
{
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_error_enum_t xed_error;
  char inst_buf[1024];

  xed_decoded_inst_zero_set_mode(xptr, &xed_machine_state_x86_64);
  xed_error = xed_decode(xptr, (uint8_t*) ins, 15);

  xed_format_xed(xptr, inst_buf, sizeof(inst_buf), (uint64_t) ins);
  printf("(%p, %d bytes, %s) %s \n" , ins, xed_decoded_inst_get_length(xptr),
         xed_iclass_enum_t2str(xed_decoded_inst_get_iclass(xptr)), inst_buf);
}

