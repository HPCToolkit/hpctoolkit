//
// Validate a return address obtained from an unw_step:
//   Most useful after a troll operation, but may still be used for QC on all unwinds
//

#include "x86-decoder.h"
#include "x86-validate-retn-addr.h"
#include "x86-unwind-analysis.h"
#include "fnbounds_interface.h"
#include "validate_return_addr.h"
#include "unwind_cursor.h"
#include "pmsg.h"

extern void *x86_get_branch_target(void *ins, xed_decoded_inst_t *xptr);

static void *
vdecode_call(void *ins,xed_decoded_inst_t *xptr)
{
  return x86_get_branch_target(ins,xptr);
}

/* static */ bool
confirm_call(void *addr, void *routine)
{
  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;
  xed_error_enum_t xed_error;
  xed_decoded_inst_zero_set_mode(xptr, &x86_decoder_settings.xed_settings);
  xed_decoded_inst_zero_keep_mode(xptr);
  void *possible_call = addr - 5;
  xed_error = xed_decode(xptr, (uint8_t *)possible_call, 15);

  TMSG(VALIDATE_UNW,"trying to confirm a call from return addr %p", addr);

  if (xed_error != XED_ERROR_NONE) {
    TMSG(VALIDATE_UNW, "addr %p has XED decode error when attempting confirm",
	 possible_call);
    return false;
  }

  xed_iclass_enum_t xiclass = xed_decoded_inst_get_iclass(xptr);
  switch(xiclass) {
    case XED_ICLASS_CALL_FAR:
    case XED_ICLASS_CALL_NEAR:
      TMSG(VALIDATE_UNW,"call instruction confirmed @ %p", possible_call);
      void *the_call = vdecode_call(possible_call, xptr);
      TMSG(VALIDATE_UNW,"comparing discovered call %p to actual routine %p",
	   the_call, routine);
      return (the_call == routine);
      break;
    default:
      return false;
  }
  EMSG("confirm call shouldn't get here!");
  return false;
}


static bool
indirect_or_tail(void *addr, void *my_routine)
{
  TMSG(VALIDATE_UNW,"checking for indirect or tail call");
  return true;
}


validation_status
deep_validate_return_addr(void *addr, void *generic)
{
  unw_cursor_t *cursor = (unw_cursor_t *)generic;

  void *beg, *end;
  if (fnbounds_enclosing_addr(addr, &beg, &end)) {
    return UNW_ADDR_WRONG;
  }

  void *my_routine = cursor->pc;
  if (fnbounds_enclosing_addr(my_routine,&beg,&end)) {
    return UNW_ADDR_WRONG;
  }
  my_routine = beg;
  TMSG(VALIDATE_UNW,"beginning of my routine = %p", my_routine);
  if (confirm_call(addr, my_routine)) {
    return UNW_ADDR_CONFIRMED;
  }
  if (indirect_or_tail(addr, my_routine)) {
    return UNW_ADDR_PROBABLE;
  }

  return UNW_ADDR_WRONG;
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
