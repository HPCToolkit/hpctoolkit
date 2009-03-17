#ifndef X86_INTERVAL_ARG_H
#define X86_INTERVAL_ARG_H

#include <stdbool.h>
#include "x86-unwind-analysis.h"

// as extra arguments are added to the process inst routine, better
// to add fields to the structure, rather than keep changing the function signature

typedef struct interval_arg_t {
  // read only:
  void *beg;
  void *end;
  unwind_interval *first;

  // read/write:
  void *ins;
  unwind_interval *current;
  bool bp_just_pushed;
  highwatermark_t highwatermark;
  unwind_interval *canonical_interval;
  bool bp_frames_found;
  void *rax_rbp_equivalent_at;
  void *return_addr; // A place to store void * return values.
} interval_arg_t;

#endif // X86_INTERVAL_ARG_H
