// -*-Mode: C++;-*- // technically C99
// $Id: backtrace.h 1445 2008-06-14 18:58:36Z johnmc $


#ifndef CSPROF_BACKTRACE_H
#define CSPROF_BACKTRACE_H

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


#endif
