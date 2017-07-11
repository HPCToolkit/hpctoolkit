//
// This software was produced with support in part from the Defense Advanced
// Research Projects Agency (DARPA) through AFRL Contract FA8650-09-C-1915. 
// Nothing in this work should be construed as reflecting the official policy or
// position of the Defense Department, the United States government, or
// Rice University.
//
#ifndef UNW_DATATYPES_SPECIFIC_H 
#define UNW_DATATYPES_SPECIFIC_H 

#include <libunwind.h>

#include <unwind/common/fence_enum.h>
#include <hpcrun/loadmap.h>
#include <utilities/ip-normalized.h>

typedef struct {
  load_module_t* lm;
} intvl_t;


typedef struct hpcrun_unw_cursor_t {
  void* pc_unnorm;

  unw_cursor_t uc;
  // normalized ip for first instruction in enclosing function
  ip_normalized_t the_function;
  ip_normalized_t pc_norm;
  fence_enum_t fence;

} hpcrun_unw_cursor_t;

#endif // UNW_DATATYPES_SPECIFIC_H 
