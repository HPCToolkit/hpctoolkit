#ifndef __cct_backtrace_finalize_h__
#define __cct_backtrace_finalize_h__

//******************************************************************************
// local includes 
//******************************************************************************

#include "unwind/common/backtrace_info.h"
#include "cct/cct.h"
#include "cct/cct_bundle.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef void (*cct_backtrace_finalize_fn)(
  backtrace_info_t *bt, 
  int isSync
);


typedef struct cct_backtrace_finalize_entry_s {
  cct_backtrace_finalize_fn fn;
  struct cct_backtrace_finalize_entry_s *next;
} cct_backtrace_finalize_entry_t;


typedef cct_node_t * (*cct_cursor_finalize_fn)(
  cct_bundle_t *cct,
  backtrace_info_t *bt,
  cct_node_t *cursor
);

//******************************************************************************
// forward declarations
//******************************************************************************

extern void cct_backtrace_finalize_register(
  cct_backtrace_finalize_entry_t *e
);


extern void cct_backtrace_finalize(
  backtrace_info_t *bt, 
  int isSync
);


extern void cct_cursor_finalize_register(
  cct_cursor_finalize_fn fn
);


extern cct_node_t *cct_cursor_finalize(
  cct_bundle_t *cct,
  backtrace_info_t *bt,
  cct_node_t *cursor
);

#endif
