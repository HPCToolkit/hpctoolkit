//
// Validate a return address obtained from an unw_step:
//   Most useful after a troll operation, but may still be used for 
//   QC on all unwinds
//

//***************************************************************************
// system include files 
//***************************************************************************

#include <stdbool.h>



//***************************************************************************
// local include files 
//***************************************************************************

#include "x86-decoder.h"
#include "x86-validate-retn-addr.h"
#include "x86-unwind-analysis.h"
#include "fnbounds_interface.h"
#include "validate_return_addr.h"
#include "unwind_cursor.h"
#include "ui_tree.h"
#include "x86-unwind-interval.h"
#include "pmsg.h"



//****************************************************************************
// forward declarations 
//****************************************************************************

extern void *x86_get_branch_target(void *ins, xed_decoded_inst_t *xptr);



//****************************************************************************
// local types 
//****************************************************************************

typedef struct xed_decode_t {
  xed_decoded_inst_t xedd;
  xed_error_enum_t   err;
} xed_decode_t;



//****************************************************************************
// local operations 
//****************************************************************************

// this function provides a convenient single breakpoint location for stopping
// when an invalid unwind is found. 
static validation_status
status_is_wrong()
{
  return UNW_ADDR_WRONG;
}


static void
xed_decode_i(void *ins, xed_decode_t *res)
{
  xed_decoded_inst_t *xptr = &(res->xedd);
  xed_decoded_inst_zero_set_mode(xptr, &x86_decoder_settings.xed_settings);
  xed_decoded_inst_zero_keep_mode(xptr);
  res->err = xed_decode(xptr, (uint8_t *)ins, 15);
}


static bool
confirm_call_fetch_addr(void *addr, size_t offset, void **the_call)
{
  xed_decode_t xed;

  void *possible_call = addr - offset;

  *the_call = NULL;
  xed_decode_i(possible_call, &xed);

  if (xed.err != XED_ERROR_NONE) {
    return false;
  }
  xed_decoded_inst_t *xptr = &xed.xedd;
  if ( xed_decoded_inst_get_length(xptr) != offset ){
    TMSG(VALIDATE_UNW,"instruction @ offset %d from %p does not have length corresponding to the offset",
         offset, addr);
    return false;
  }
  switch(xed_decoded_inst_get_iclass(xptr)) {
    case XED_ICLASS_CALL_FAR:
    case XED_ICLASS_CALL_NEAR:
      TMSG(VALIDATE_UNW,"call instruction confirmed @ %p", possible_call);
      *the_call = x86_get_branch_target(possible_call, xptr);
      return true;
      break;
    default:
      return false;
  }
  EMSG("MAJOR PROGRAMMING ERROR: impossible fall thru @confirm_call_fetch_addr");
  return false;
}


static bool
confirm_call(void *addr, void *routine)
{
  TMSG(VALIDATE_UNW,"Checking for true call immediately preceding %p",addr);

  void *the_call;

  if ( ! confirm_call_fetch_addr(addr, 5, &the_call)) {
    TMSG(VALIDATE_UNW,"No true call instruction found, so true call REJECTED");
    return false;
  }
  TMSG(VALIDATE_UNW,"comparing called routine %p to actual routine %p",
       the_call, routine);
  return (the_call == routine);
}


static bool
confirm_indirect_call_specific(void *addr, size_t offset)
{
  void *the_call;

  if ( ! confirm_call_fetch_addr(addr, offset, &the_call)) {
    TMSG(VALIDATE_UNW,"No call instruction found @ %p, so indirect call at this location rejected",
         addr - offset);
    return false;
  }
  return (the_call == NULL);
  
}


static bool
confirm_indirect_call(void *addr)
{
  TMSG(VALIDATE_UNW,"trying to confirm an indirect call preceeding %p", addr);
  for (size_t i=1;i <= 7;i++) {
    if (confirm_indirect_call_specific(addr, i)){
      return true;
    }
  }
  return false;
}

static validation_status
contains_tail_call_to_f(void *callee, void *target_fn)
{
  void *routine_start, *routine_end;
  if (fnbounds_enclosing_addr(callee, &routine_start, &routine_end)) {
    TMSG(VALIDATE_UNW,"unwind addr %p does NOT have function bounds, so it is invalid",callee);
    return status_is_wrong(); // hard error: callee is nonsense
  }

  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_error_enum_t xed_error;
  validation_status status = UNW_ADDR_WRONG;

  xed_decoded_inst_zero_set_mode(xptr, &x86_decoder_settings.xed_settings);

  void *ins = routine_start;
  while (ins < routine_end) {
    xed_decoded_inst_zero_keep_mode(xptr);
    xed_error = xed_decode(xptr, (uint8_t*) ins, 15);

    if (xed_error != XED_ERROR_NONE) {
      ins++;         /* skip this byte      */
      continue;      /* continue onward ... */
    }

    xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);

    switch(xiclass) {
      // branches
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
      // jumps
    case XED_ICLASS_JMP: 
    case XED_ICLASS_JMP_FAR:
      {
	void *target = x86_get_branch_target(ins, xptr);
	if ((target >= routine_end) || (target < routine_start)) {
	  // tail call
	  if (target == target_fn) return UNW_ADDR_CONFIRMED;
	  status = UNW_ADDR_PROBABLE_TAIL;
	}
      }
      break;
    default:
      break;
    }
    ins = ins + xed_decoded_inst_get_length(xptr);
  }
  return status;
}


static validation_status
confirm_tail_call(void *addr, void *target_fn)
{
  TMSG(VALIDATE_UNW,"trying to confirm that instruction before %p is call to a routine with tail calls", addr);

  void *callee;

  if ( ! confirm_call_fetch_addr(addr, 5, &callee)) {
    TMSG(VALIDATE_UNW,"No call instruction found @ %p, so tail call REJECTED",
         addr - 5);
    return UNW_ADDR_WRONG; // soft error: this case doesn't apply
  }

  TMSG(VALIDATE_UNW,"Checking routine %p for possible tail calls", callee);
  unwind_interval *ri = (unwind_interval *) csprof_addr_to_interval(callee);
  bool rv = (ri && ri->has_tail_calls);

  if (rv) return contains_tail_call_to_f(callee, target_fn);

  return status_is_wrong();
}


static void *
x86_plt_branch_target(void *ins, xed_decoded_inst_t *xptr)
{
  const xed_inst_t *xi = xed_decoded_inst_inst(xptr);
  const xed_operand_t *op0 =  xed_inst_operand(xi, 0);
  xed_operand_enum_t   op0_name = xed_operand_name(op0);
  xed_operand_type_enum_t op0_type = xed_operand_type(op0);

  if (op0_name == XED_OPERAND_MEM0 && 
      op0_type == XED_OPERAND_TYPE_IMM_CONST) {
    xed_operand_values_t *vals = xed_decoded_inst_operands(xptr);
    xed_reg_enum_t reg = xed_operand_values_get_base_reg(vals,0);
    if (reg == XED_REG_RIP) {
      int ins_len = xed_decoded_inst_get_length(xptr);
      xed_int64_t disp =  xed_operand_values_get_memory_displacement_int64(vals);
      xed_int64_t **memloc = ins + disp + ins_len;
      xed_int64_t *memval = *memloc;
      return memval;
    }
  }
  return NULL;
}


static 
validation_status
confirm_plt_call(void *addr, void *callee)
{

  TMSG(VALIDATE_UNW,"trying to confirm that instruction before %p is call to a routine through the PLT", addr);

  void *plt_ins;

  if ( ! confirm_call_fetch_addr(addr, 5, &plt_ins)) {
    TMSG(VALIDATE_UNW,"No call instruction found @ %p, so PLT call REJECTED",
         addr - 5);
    return UNW_ADDR_WRONG;
  }

  TMSG(VALIDATE_UNW,"Checking at %p for PLT call", plt_ins);

  xed_decode_t xed;
  xed_decode_i(plt_ins, &xed);
  xed_decoded_inst_t *xptr = &xed.xedd;

  void *plt_callee = x86_plt_branch_target(plt_ins, xptr);
  if (plt_callee == callee) return UNW_ADDR_CONFIRMED;

  unwind_interval *plt_callee_ui = (unwind_interval *) csprof_addr_to_interval(plt_callee);
  if (plt_callee_ui && plt_callee_ui->has_tail_calls) return contains_tail_call_to_f(plt_callee, callee);

  return UNW_ADDR_WRONG;
}

//****************************************************************************
// interface operations 
//****************************************************************************
validation_status
deep_validate_return_addr(void *addr, void *generic)
{
  unw_cursor_t *cursor = (unw_cursor_t *)generic;

  TMSG(VALIDATE_UNW,"validating unwind step from %p ==> %p",cursor->pc,addr);
  void *beg, *end;
  if (fnbounds_enclosing_addr(addr, &beg, &end)) {
    TMSG(VALIDATE_UNW,"unwind addr %p does NOT have function bounds, so it is invalid",addr);
    return status_is_wrong();
  }
  if (fnbounds_enclosing_addr(cursor->pc, &beg, &end)) {
    TMSG(VALIDATE_UNW,"***The pc in the unwind cursor (= %p) does not have function bounds\n"
         "***INTERNAL ERROR: please check arguments",cursor->pc);
    return status_is_wrong();
  }
  void *callee = beg;
  TMSG(VALIDATE_UNW,"beginning of my routine = %p", callee);
  if (confirm_call(addr, callee)) {
    TMSG(VALIDATE_UNW,"Instruction preceeding %p is a call to this routine. Unwind confirmed",addr);
    return UNW_ADDR_CONFIRMED;
  }
  validation_status result = confirm_plt_call(addr, callee);
  if (result != UNW_ADDR_WRONG) {
    TMSG(VALIDATE_UNW,"Instruction preceeding %p is a call through the PLT to this routine. Unwind confirmed",addr);
    return result;
  }
  if (confirm_indirect_call(addr)){
    TMSG(VALIDATE_UNW,"Instruction preceeding %p is an indirect call. Unwind is LIKELY ok",addr);
    return UNW_ADDR_PROBABLE_INDIRECT;
  }
  result = confirm_tail_call(addr, callee);
  if (result != UNW_ADDR_WRONG) {
    TMSG(VALIDATE_UNW,"Instruction preceeding %p is a call to a routine that has tail calls. Unwind is LIKELY ok",addr);
    return result;
  }
  TMSG(VALIDATE_UNW,"Unwind addr %p is NOT confirmed",addr);
  return status_is_wrong();
}


validation_status
dbg_val(void *addr, void *pc)
{
  unw_cursor_t cursor;

  cursor.pc = pc;
  return deep_validate_return_addr(addr, &cursor);
}


validation_status
validate_return_addr(void *addr, void *generic)
{
  void *beg, *end;
  if (fnbounds_enclosing_addr(addr, &beg, &end)) {
    return UNW_ADDR_WRONG;
  }

  return UNW_ADDR_PROBABLE;
}


