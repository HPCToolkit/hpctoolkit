#include <string.h>
#include "x86-unwind-interval-fixup.h"

static int _dl_runtime_resolve_signature[] = { 
  0x38ec8348,      0x24048948,      0x244c8948,      0x54894808,
  0x89481024,      0x48182474,      0x20247c89,      0x2444894c,
  0x4c894c28,      0x8b483024,      0x49402474,      0x014cf389,
  0xde014cde,      0x03e6c148,      0x247c8b48
};


static int 
adjust_dl_runtime_resolve_unwind_intervals(char *ins, int len, interval_status *stat)
{
  int siglen = sizeof(_dl_runtime_resolve_signature);

  if (len > siglen && strncmp((char *)_dl_runtime_resolve_signature, ins, siglen) == 0) {
    // signature matched 
    unwind_interval *ui = stat->first;
    while(ui) {
	ui->sp_ra_pos += 16;
	ui = ui->next;
    }
    return 1;
  } 
  return 0;
}


static void 
__attribute__ ((constructor))
register_unwind_interval_fixup_function(void)
{
  add_x86_unwind_interval_fixup_function(adjust_dl_runtime_resolve_unwind_intervals);
}


