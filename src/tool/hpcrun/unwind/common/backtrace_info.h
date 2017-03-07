#ifndef BACKTRACE_INFO_H
#define BACKTRACE_INFO_H

//
// Information about backtrace
// NOTE:
//    Minor data structure for transitional use
//    There are no operations, so type is concrete
//
//    Likely to be eventually replaced by an opaque backtrace datatype
//
#include <stdbool.h>
#include <stdint.h>

#include <hpcrun/frame.h>
#include <unwind/common/fence_enum.h>

typedef struct {
  frame_t* begin;     // beginning frame of backtrace
  frame_t* last;      // ending frame of backtrace (inclusive)
  size_t   n_trolls;  // # of frames that resulted from trolling
  fence_enum_t fence:3; // Type of stop -- thread or main *only meaninful when good unwind
  bool     has_tramp:1; // true when a trampoline short-circuited the unwind
  bool     bottom_frame_elided:1; // true if bottom frame has been elided 
  bool     partial_unwind:1; // true if not a full unwind
  void    *trace_pc;  // in/out value: modified to adjust trace when modifying backtrace
} backtrace_info_t;

#endif // BACKTRACE_INFO_H
