// -*-Mode: C++;-*- // technically C99
// $Id$

#ifndef UNWIND_CURSOR_H
#define UNWIND_CURSOR_H

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include "unwind-cfg.h"

//*************************** Forward Declarations **************************

// HPC_UNW_LITE: It is not safe to have a pointer to the interval
// since we cannot use dynamic storage.
#if (HPC_UNW_LITE)

   // there should probably have a check to ensure this is big enough
   typedef struct { char data[128]; } unw_interval_opaque_t;
#  define UNW_CURSOR_INTERVAL_t unw_interval_opaque_t

#else

#  include "splay-interval.h"
#  define UNW_CURSOR_INTERVAL_t splay_interval_t*

#endif

//***************************************************************************

typedef struct _unw_c_t {
  void *pc;
  void **bp;
  void **sp;
  void *ra;

  UNW_CURSOR_INTERVAL_t intvl;
  int trolling_used;

} unw_cursor_t;


//***************************************************************************

#endif
