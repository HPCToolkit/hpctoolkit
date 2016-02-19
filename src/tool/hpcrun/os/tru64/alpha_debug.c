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

/* alpha_debug.c -- printing various debugging information */

void
hpcrun_print_context(CONTEXT *ctx)
{
    unsigned int i;

    ERRMSG("PC: %#lx", __FILE__, __LINE__, ctx->sc_pc);

    for(i=0; i<31; ++i) {
        ERRMSG("$r%d: %lx", __FILE__, __LINE__, i, ctx->sc_regs[i]);
    }
}

/* meant for those cases where we're about to die, but we need a little
   useful information before we bail.  `ctx' may be null, depending on
   where we die. */
void
hpcrun_print_state(hpcrun_state_t *state, void *context)
{
    CONTEXT *ctx = (CONTEXT *) context;

    /* try to print out a little information; debugger backtraces
       tend to be unhelpful because the trampoline interferes. */
    hpcrun_print_backtrace(state);
    ERRMSG("Trampoline was located at: %#lx -> %#lx", __FILE__, __LINE__,
           state->swizzle_patch, state->swizzle_return);
    if(ctx != NULL) {
        hpcrun_print_context(ctx);
    }

    ERRMSG("Extra data: %#lx", __FILE__, __LINE__, state->extra_state);
    ERRMSG("we %s been through a trampoline since the last signal",
           __FILE__, __LINE__,
           hpcrun_state_flag_isset(state, CSPROF_THRU_TRAMP) ? "have" : "have not");
}

