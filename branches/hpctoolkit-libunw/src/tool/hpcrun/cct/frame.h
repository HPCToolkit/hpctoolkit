#ifndef FRAME_H
#define FRAME_H

//
// !! The location of this include file might be *configuration dependent* !!
//  DO NOT PATH QUALIFY THIS INCLUDE until the configuration question is settled
//
#include "unwind_cursor.h"

#include <lib/prof-lean/lush/lush-support.h>

// --------------------------------------------------------------------------
// frame_t: similar to cct_node_t, but specialized for the backtrace buffer
// --------------------------------------------------------------------------

typedef struct frame_t {
  unw_cursor_t cursor;       // hold a copy of the cursor for this frame
  lush_assoc_info_t as_info;
  void* ip;
  void* ra_loc;
  lush_lip_t* lip;
} frame_t;

#endif // FRAME_H
