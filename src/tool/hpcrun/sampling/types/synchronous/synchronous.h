// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
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

#ifndef CSPROF_SYNCHRONOUS_H
#define CSPROF_SYNCHRONOUS_H

/* hpcrun_fetch_stack_pointer takes a `void **' to force procedures in
   which it is used to require a stack frame.  this gets around several
   problems, notably the problem of return addresses being in registers. */

#if defined(OSF) && defined(__DECC)

static inline void
hpcrun_fetch_stack_pointer(void **stack_pointer_loc)
{
    asm("stq sp, 0(v0)", stack_pointer_loc);
}

/* is this procedure general enough to be moved outside #if? */
static inline void
hpcrun_take_synchronous_sample(unsigned int sample_count)
{
    void *ra_loc;
    hpcrun_state_t *state = hpcrun_get_state();

    hpcrun_undo_swizzled_data(state, NULL);
    hpcrun_fetch_stack_pointer(&ra_loc);
    hpcrun_take_sample(sample_count);
    hpcrun_swizzle_location(state, ra_loc);
}

#endif

#endif
