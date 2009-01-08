#ifndef x96_unwind_interval_fixup_h
#define x96_unwind_interval_fixup_h

#include "x86-unwind-interval.h"

typedef int (*x86_ui_fixup_fn_t)(char *ins, int len, interval_status *stat);

void add_x86_unwind_interval_fixup_function(x86_ui_fixup_fn_t fn);

int x86_fix_unwind_intervals(char *ins, int len, interval_status *stat);

#endif
