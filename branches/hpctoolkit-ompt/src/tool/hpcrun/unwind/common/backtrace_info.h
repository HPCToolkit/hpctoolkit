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
  fence_enum_t fence; // Type of stop -- thread or main *only meaninful when good unwind
  bool     has_tramp; // true when a trampoline short-circuited the unwind
  bool     trolled;   // true when ANY frame in the backtrace came from a troll
  size_t   n_trolls;  // # of frames that resulted from trolling
  bool     bottom_frame_elided; // true if bottom frame has been elided 
  bool     partial_unwind; // true if not a full unwind
  void    *trace_pc;  // in/out value: modified to adjust trace when modifying backtrace
} backtrace_info_t;

#endif // BACKTRACE_INFO_H
