// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// 
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
// 
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage. 
// 
// ******************************************************* EndRiceCopyright *

/* a driver for programs that are going to manually call into the library */

#include <libunwind.h>

#include "state.h"
#include "structs.h"

void
csprof_record_sample(unsigned long amount)
{
  csprof_state_t *state = csprof_get_state();
  unw_context_t ctx;
  unw_cursor_t frame;

  if(state != NULL) {
    /* force insertion from the root */
    state->treenode = NULL;
    state->bufstk = state->bufend;
    state = csprof_check_for_new_epoch(state);

    /* FIXME: error checking */
    unw_get_context(&ctx);
    unw_init_local(&frame, &ctx);
    unw_step(&frame);		/* step out into our caller */

    csprof_sample_callstack_from_frame(state, amount, &frame);
  }
}

void
csprof_driver_init(csprof_state_t *state, csprof_options_t *options)
{
}

void
csprof_driver_fini(csprof_state_t *state, csprof_options_t *options)
{
}

#ifdef CSPROF_THREADS
void
csprof_driver_thread_init(csprof_state_t *state)
{
    /* no support required */
}

void
csprof_driver_thread_fini(csprof_state_t *state)
{
    /* no support required */
}
#endif

void
csprof_driver_suspend(csprof_state_t *state)
{
    /* no support required */
}

void
csprof_driver_resume(csprof_state_t *state)
{
    /* no support required */
}

