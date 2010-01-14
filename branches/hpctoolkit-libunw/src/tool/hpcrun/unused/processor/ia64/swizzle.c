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

#include <sys/types.h>
#include <ucontext.h>

#include "interface.h"
#include "general.h"

extern void *__libc_start_main;

void *
context_pc(void *context) 
{
  return (void *) ((ucontext_t *) context)->uc_mcontext->sc_ip;
}

int
csprof_find_return_address_for_context(csprof_state_t *state,
                                       struct lox *l, mcontext_t *ctx)
{
    return 0;
}

int
csprof_find_return_address_for_function(csprof_state_t *state,
                                        struct lox *l, void **stackptr)
{

#if 0
    l->current.location.address = l->stored.location.address = rbp;

    /* we have to beware of swizzling the return from libc_start_main;
       we take the evil measure of guessing how long the procedure is
       going to be to figure out when the instruction pointer lies
       within libc_start_main.  this should be reasonably safe. */
    {
        void *libc_start_main_addr = (void **)&__libc_start_main;
        void *libc_start_main_end = VPTR_ADD(libc_start_main_addr, 240);
        void *ret = *rbp;

        return !((libc_start_main_addr <= ret) && (ret < libc_start_main_end));
    }
#endif

    return 0;
}

void
csprof_remove_trampoline(csprof_state_t *state, mcontext_t *context)
{
#if 0
    if(csprof_swizzle_patch_is_register(state)) {
        DIE("x86-64 backend doesn't support register saves!", __FILE__, __LINE__);
    }
    else {
        void **addr = state->swizzle_patch;

        *addr = state->swizzle_return;
    }
#else
    DIE("IA64 backend doesn't support trampolines", __FILE__, __LINE__);
#endif
}

void
csprof_insert_trampoline(csprof_state_t *state, struct lox *l, mcontext_t *context)
{
#if 0
    if(l->current.type == REGISTER) {
        DIE("x86-64 backend doesn't support register saves!", __FILE__, __LINE__);
    }
    else {
        void **addr = l->current.location.address;

	DBGMSG_PUB(8, "Inserting trampoline at %p = %p",
		   addr, *addr);

        state->swizzle_return = *addr;
        *addr = CSPROF_TRAMPOLINE_ADDRESS;
    }
#else
    DIE("IA64 backend doesn't support trampolines", __FILE__, __LINE__);
#endif
}
