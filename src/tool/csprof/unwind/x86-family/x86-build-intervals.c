#include "pmsg.h"
#include "intervals.h"

#include "x86-decoder.h"
#include "x86-process-inst.h"
#include "x86-unwind-analysis.h"
#include "x86-unwind-interval-fixup.h"

/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static void set_status(interval_status *status, char *fui, int errcode, 
			 unwind_interval *first);

interval_status 
x86_build_intervals(char *ins, unsigned int len, int noisy);



/******************************************************************************
 * interface operations 
 *****************************************************************************/

interval_status 
build_intervals(char *ins, unsigned int len)
{
  return x86_build_intervals(ins, len, 0);
}


interval_status 
x86_build_intervals(char *ins, unsigned int len, int noisy)
{
  char *first_ins = ins;

  xed_decoded_inst_t xedd;
  xed_decoded_inst_t *xptr = &xedd;

  interval_status status;
  xed_error_enum_t xed_error;

  unwind_interval *prev = NULL, *current = NULL, *next = NULL, *first = NULL;

  highwatermark_t highwatermark = { NULL, NULL, HW_NONE };
  unwind_interval *canonical_interval = 0;
  int error_count = 0;

  // handle return is different if there are any bp frames
  bool bp_frames_found = false;  
  bool bp_just_pushed = false;

  char *end = ins + len;

  current = new_ui(ins, RA_SP_RELATIVE, 0, 0, BP_UNCHANGED, 0, 0, NULL);

  xed_decoded_inst_zero_set_mode(xptr, &x86_decoder_settings.xed_settings);

  if (noisy) dump_ui(current, true);

  first = current;
  while (ins < end) {
    
    xed_decoded_inst_zero_keep_mode(xptr);
    xed_error = xed_decode(xptr, (uint8_t*) ins, 15);

    if (xed_error != XED_ERROR_NONE) {
      error_count++; /* note the error      */
      ins++;         /* skip this byte      */
      continue;      /* continue onward ... */
    }

    next = process_inst(xptr, ins, end, current, first, &bp_just_pushed, 
			&highwatermark, &canonical_interval, &bp_frames_found);
    
    if (next == &poison_ui) {
      set_status(&status, ins, -1, NULL);
      return status;
    }

#if 0
    printf("ins %p current = %p, next = %p, iclass = %s, noisy = %d\n", 
	   ins, current, next, xed_iclass_enum_t2str(iclass(xptr)), noisy);
#endif

    if (next != current) {
      link_ui(current, next);
      prev = current;
      current = next;

      if (noisy) dump_ui(current, true);
    }
    ins += xed_decoded_inst_get_length(xptr);
    current->endaddr = ins;
  }

  current->endaddr = end;

  set_status(&status, ins, error_count, first);

  x86_fix_unwind_intervals(first_ins, len, &status);
  return status;
}



/******************************************************************************
 * private operations 
 *****************************************************************************/

static void set_status(interval_status *status, char *fui, int errcode, 
		       unwind_interval *first) 
{
  status->first_undecoded_ins = fui;
  status->errcode = errcode;
  status->first   = first;
}

