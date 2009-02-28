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
#include <ucontext.h>



//***************************************************************************
// local include files 
//***************************************************************************

#include "state.h"



//***************************************************************************
// forward declarations
//***************************************************************************

csprof_cct_node_t*
csprof_sample_callstack(csprof_state_t *state, ucontext_t* context, 
			int metric_id, size_t sample_count);

#if (HPC_UNW_LITE)
int
hpcrun_backtrace_lite(void** buffer, int size, ucontext_t* context);
#endif

// FIXME: tallent: relocate when 'csprof state' trash is untangled
void 
dump_backtrace(csprof_state_t *state, csprof_frame_t *unwind);

//***************************************************************************

#endif // hpcrun_backtrace_h
