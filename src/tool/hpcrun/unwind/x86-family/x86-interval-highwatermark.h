#ifndef X86_INTERVAL_HIGHWATERMARK_H
#define X86_INTERVAL_HIGHWATERMARK_H

#include "x86-unwind-interval.h"

typedef struct highwatermark_t {
  unwind_interval *uwi;
  void *succ_inst_ptr; // pointer to successor (support for pathscale idiom)
  int state;
} highwatermark_t;

/******************************************************************************
 * macros 
 *****************************************************************************/

#define HW_INITIALIZED      0x8
#define HW_BP_SAVED         0x4
#define HW_BP_OVERWRITTEN   0x2
#define HW_SP_DECREMENTED   0x1
#define HW_UNINITIALIZED    0x0

#define HW_TEST_STATE(state, is_set, is_clear) \
  ((((state) & (is_set)) == (is_set))  && (((state) & (is_clear)) == 0x0))

#define HW_NEW_STATE(state, set) ((state) | HW_INITIALIZED | (set))

#endif  // X86_INTERVAL_HIGHWATERMARK_H

extern highwatermark_t _hw_uninit;
