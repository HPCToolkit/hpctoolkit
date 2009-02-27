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

//*************************** User Include Files ****************************

#include "splay-interval.h"


//*************************** Forward Declarations **************************

//***************************************************************************

//***************************************************************************
// unw_interval_t
//***************************************************************************

typedef enum {
  RA_SP_RELATIVE,
  RA_STD_FRAME,
  RA_BP_FRAME,
  RA_REGISTER, // RA is in a register (either LR or R0)
  RA_POISON
} ra_loc;


typedef enum {
  BP_UNCHANGED,
  BP_SAVED,     // Parent's BP is saved in frame
  BP_HOSED
} bp_loc;


typedef struct {
  struct splay_interval_s common; // common splay tree fields

  ra_loc ra_status; // how to find the return address
  bp_loc bp_status; // how to find the bp register

  // RA_REGISTER: the register that contains ra
  // otherwise:   ra offset from bp
  int ra_arg;

} unw_interval_t;


unw_interval_t *
new_ui(char *startaddr, ra_loc ra_status, int ra_arg, 
       bp_loc bp_status, unw_interval_t *prev);

void 
ui_dump(unw_interval_t *u, int dump_to_stdout);

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
