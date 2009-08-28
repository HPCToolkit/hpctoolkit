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

#include <setjmp.h>

#include "interface.h"

void
arch_libc_init()
{
    /* nothing to do ATM; exception handling stuff goes here */
}

void
csprof_nlx_to_context(csprof_state_t *state, jmp_buf env)
{
    /* have to figure out what to do about IA-64's insistence that
       user code know nothing about the layout of the jmpbuf */
#if 0
    struct __jmp_buf_tag *xenv = &env[0];
    __jmp_buf *zenv = &xenv->__jmpbuf;
    void *pc;
    void *sp;
    void *return_address;

    pc = (void *)zenv[0][JB_PC];
    sp = (void *)zenv[0][JB_RSP];

    if(pc == CSPROF_TRAMPOLINE_ADDRESS) {
	pc = csprof_bt_top_ip(state);
        zenv[0][JB_PC] = (long int) csprof_bt_top_ip(state);
        *(state->swizzle_patch) = state->swizzle_return;
    }

    if(!csprof_state_has_empty_backtrace(state)) {
        if(sp < csprof_bt_top_sp(state)) {
            /* nothing to do here */
        }
        else if(sp == csprof_bt_top_sp(state)) {
            /* nothing to do here */
        }
        else {
            /* undo the current swizzled state */
            *(state->swizzle_patch) = state->swizzle_return;
            state->swizzle_patch = state->swizzle_return = 0;

            /* used to be '!=', but I think '>' is OK.  using '!=' means
	       we have to resort to unwinding to figure out the "proper"
	       $sp and that's a real pain. */
            while((csprof_bt_top_sp(state) > sp)
                  && !csprof_state_has_empty_backtrace(state)) {
                csprof_bt_pop(state);
            }

            csprof_swizzle_with_jmpbuf(state, env);
        }
    }
#endif
}
