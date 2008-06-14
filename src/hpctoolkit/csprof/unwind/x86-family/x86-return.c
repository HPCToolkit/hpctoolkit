#include "x86-canonical.h"
#include "x86-decoder.h"
#include "x86-unwind-analysis.h"

/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static bool plt_is_next(char *ins);


/******************************************************************************
 * interface operations
 *****************************************************************************/

unwind_interval *
process_return(xed_decoded_inst_t *xptr, unwind_interval *current, 
	       char *ins, char *end, bool irdebug, unwind_interval *first, 
	       highwatermark_t *highwatermark, 
	       unwind_interval **canonical_interval, bool bp_frames_found)
{
  unwind_interval *next = current;
  if (ins + xed_decoded_inst_get_length(xptr) < end) {
    //-------------------------------------------------------------------------
    // the return is not the last instruction in the interval; 
    // set up an interval for code after the return 
    //-------------------------------------------------------------------------
    if (plt_is_next(ins + xed_decoded_inst_get_length(xptr))) {
      //-------------------------------------------------------------------------
      // the code following the return is a program linkage table. each entry in 
      // the program linkage table should be invoked with the return address at 
      // top of the stack. this is exactly what the interval containing this
      // return instruction looks like. set the current interval as the 
      // "canonical interval" to be restored after then jump at the end of each
      // entry in the PLT. 
      //-------------------------------------------------------------------------
      *canonical_interval = current;
    } else {
      reset_to_canonical_interval(xptr, current, &next, ins, end, irdebug, first, 
				  highwatermark, canonical_interval, bp_frames_found); 
    }
  }
  return next;
}


/******************************************************************************
 * private operations
 *****************************************************************************/

static bool plt_is_next(char *ins)
{
  
  // Assumes: 'ins' is pointing at the instruction from which
  // lookahead is to occur (i.e, the instruction prior to the first
  // lookahead).

  xed_state_t *xed_settings = &(x86_decoder_settings.xed_settings);

  xed_error_enum_t xed_err;
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  char *val_pushed = NULL;
  char *push_succ_addr = NULL;
  char *jmp_target = NULL;

  // skip optional padding if there appears to be any
  while ((((long) ins) & 0x11) && (*ins == 0x0)) ins++; 

  // -------------------------------------------------------
  // requirement 1: push of displacement relative to IP 
  // -------------------------------------------------------
  xed_decoded_inst_zero_set_mode(xptr, xed_settings);
  xed_err = xed_decode(xptr, (uint8_t*) ins, 15);
  if (xed_err != XED_ERROR_NONE) {
    return false;
  }

  if (iclass_eq(xptr, XED_ICLASS_PUSH)) {
    if (xed_decoded_inst_number_of_memory_operands(xptr) == 2) {
      const xed_inst_t* xi = xed_decoded_inst_inst(xptr);
      const xed_operand_t* op0 = xed_inst_operand(xi, 0);
      if ((xed_operand_name(op0) == XED_OPERAND_MEM0) && 
	    (xed_decoded_inst_get_base_reg(xptr, 0) == XED_REG_RIP)) {
	int64_t offset = xed_decoded_inst_get_memory_displacement(xptr, 0);
	push_succ_addr = ins + xed_decoded_inst_get_length(xptr);
	val_pushed = push_succ_addr + offset;
      }
    }
  }

  if (val_pushed == NULL) {
    // push of proper type not recognized 
    return false;
  }

  // -------------------------------------------------------
  // requirement 2: jump target affects stack
  // -------------------------------------------------------
  xed_decoded_inst_zero_set_mode(xptr, xed_settings);
  xed_err = xed_decode(xptr, (uint8_t*) push_succ_addr, 15);
  if (xed_err != XED_ERROR_NONE) {
    return false;
  }

  if (iclass_eq(xptr, XED_ICLASS_JMP) || 
      iclass_eq(xptr, XED_ICLASS_JMP_FAR)) {
    if (xed_decoded_inst_number_of_memory_operands(xptr) == 1) {

      const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
      const xed_operand_t *op0 =  xed_inst_operand(xi,0);
      if ((xed_operand_name(op0) == XED_OPERAND_MEM0) && 
	    (xed_decoded_inst_get_base_reg(xptr, 0) == XED_REG_RIP)) {
	long long offset = xed_decoded_inst_get_memory_displacement(xptr,0);
	jmp_target = push_succ_addr + xed_decoded_inst_get_length(xptr) + offset;
      }
    }
  }

  if (jmp_target == NULL) {
    // jump of proper type not recognized 
    return false;
  }

  if ((jmp_target - val_pushed) == 8) return true;

  return false;
}

