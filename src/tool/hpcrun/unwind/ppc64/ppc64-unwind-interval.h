// -*-Mode: C++;-*- // technically C99
// $Id$

//***************************************************************************
//
// HPCToolkit's PPC64 Unwinder
//
//***************************************************************************

#ifndef ppc64_unwind_interval_h
#define ppc64_unwind_interval_h

//************************* System Include Files ****************************

#include <stddef.h>
#include <stdbool.h>

//*************************** User Include Files ****************************

#include "splay-interval.h"


//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
// unw_interval_t
//***************************************************************************

typedef enum {
  SPTy_NULL = 0,
  SPTy_Reg,     // Parent's SP is in a register (R1) (unallocated frame)
  SPTy_SPRel,   // Parent's SP is relative to current SP (saved in frame)
} sp_ty_t;


typedef enum {
  RATy_NULL = 0,
  RATy_Reg,     // RA is in a register (either LR or R0)
  RATy_SPRel,   // RA is relative to SP
} ra_ty_t;


typedef struct {
  struct splay_interval_s common; // common splay tree fields

  // frame type
  sp_ty_t sp_ty : 16;
  ra_ty_t ra_ty : 16;

  // SPTy_Reg  : Parent SP's register
  // RATy_SPRel: Parent SP's offset from appropriate pointer
  int sp_arg;

  // RATy_Reg  : Parent RA's register
  // RATy_SPRel: Parent RA's offset from appropriate pointer
  int ra_arg;

} unw_interval_t;


unw_interval_t *
new_ui(char *startaddr, sp_ty_t sp_ty, ra_ty_t ra_ty, int sp_arg, int ra_arg,
       unw_interval_t *prev);

static inline bool 
ui_cmp(unw_interval_t* x, unw_interval_t* y)
{
  return ((x->sp_ty  == y->sp_ty) &&
	  (x->ra_ty  == y->ra_ty) && 
	  (x->sp_arg == y->sp_arg) &&
	  (x->ra_arg == y->ra_arg));
}

void 
ui_dump(unw_interval_t *u);

// FIXME: these should be part of the common interface
long ui_count();
long suspicious_count();
void suspicious_interval(void *pc);
void ui_link(unw_interval_t *current, unw_interval_t *next);


//***************************************************************************
// external interface
//***************************************************************************

interval_status 
build_intervals(char *ins, unsigned int len);


//***************************************************************************

#endif // ppc64_unwind_interval_h
