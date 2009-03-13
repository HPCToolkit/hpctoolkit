#ifndef X86_COLD_PATH_H
#define X86_COLD_PATH_H

#include <stdbool.h>
#include "x86-unwind-analysis.h"
#include "x86-interval-arg.h"

extern bool hpcrun_is_cold_code(void *ins, xed_decoded_inst_t *xptr, interval_arg_t *iarg);
extern void hpcrun_cold_code_fixup(unwind_interval *current, unwind_interval *warm);

#endif // X86_COLD_PATH_H
