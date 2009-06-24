// -*-Mode: C++;-*- // technically C99
// $Id$

#ifndef hpcrun_backtrace_h
#define hpcrun_backtrace_h

//***************************************************************************
// file: backtrace.h
//
// purpose:
//     an interface for performing stack unwinding 
//
//***************************************************************************


//***************************************************************************
// system include files 
//***************************************************************************

#include <stddef.h>
#include <stdint.h>

#include <ucontext.h>

//***************************************************************************
// local include files 
//***************************************************************************

#include "state.h"



//***************************************************************************
// forward declarations
//***************************************************************************

csprof_cct_node_t*
hpcrun_backtrace(csprof_state_t *state, ucontext_t* context, 
		 int metricId, uint64_t metricIncr,
		 int skipInner, int isSync);

csprof_frame_t*
hpcrun_skip_chords(csprof_frame_t* bt_outer, csprof_frame_t* bt_inner, 
		   int skip);

// FIXME: tallent: relocate when 'csprof state' trash is untangled
void 
dump_backtrace(csprof_state_t *state, csprof_frame_t *unwind);

//***************************************************************************

#endif // hpcrun_backtrace_h
