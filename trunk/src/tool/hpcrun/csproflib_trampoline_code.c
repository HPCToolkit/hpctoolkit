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

/* the C trampoline */
#ifdef CSPROF_TRAMPOLINE_BACKEND

/* the !defined(CSPROF_PERF) are intended to help reduce the code footprint
   of the trampoline.  every little bit counts */
void *
csprof_trampoline2(void **stackptr)
{
    state_t *state;
    void *return_ip;
    struct lox l;
    int should_insert;

    state = csprof_get_state();
    csprof_state_flag_set(state, CSPROF_EXC_HANDLING);
    return_ip = state->swizzle_return;

    DBGMSG_PUB(1, "%%trampoline returning to %p", return_ip);
    DBGMSG_PUB(1, "%%stackptr = %p => %p", stackptr, *stackptr);
    {
        csprof_cct_node_t *treenode = (csprof_cct_node_t *)state->treenode;

        assert(treenode != NULL);

#if defined(CSPROF_LAST_EDGE_ONLY)
        if(!csprof_state_flag_isset(state, CSPROF_THRU_TRAMP)) {
#endif
            /* count this; FIXME HACK */
            treenode->metrics[1]++;
#if defined(CSPROF_LAST_EDGE_ONLY)
        }
#endif

        csprof_bt_pop(state);
        state->treenode = treenode->parent;

        csprof_state_verify_backtrace_invariants(state);
    }

    if(return_ip != csprof_bt_top_ip(state)) {
#if !defined(CSPROF_PERF)
      hpcrun_frame_t *x = state->bufstk - 1;

#if 0
      for( ; x != state->bufend; ++x) {
	printf("ip %#lx | sp %#lx\n", x->ip, x->sp);
      }
#else
      dump_backtraces(state, 0);
#endif


        ERRMSG("Thought we were returning to %#lx, but %#lx",
               __FILE__, __LINE__, return_ip, csprof_bt_top_ip(state));
        ERRMSG("csprof_trampoline2 %#lx",
               __FILE__, __LINE__, csprof_trampoline2);
	csprof_dump_loaded_modules();
#endif
        DIE("Returning to an unexpected location.",
            __FILE__, __LINE__);
    }

    should_insert = csprof_find_return_address_for_function(state, &l, stackptr);

    if(should_insert) {
        if(l.stored.type == REGISTER) {
            int reg = l.stored.location.reg;

            DBGMSG_PUB(1, "%%trampoline patching register %d", reg);
            state->swizzle_patch = reg;
            state->swizzle_return =
                csprof_replace_return_address_onstack(stackptr, reg);
        }
        else {
            void **address = l.stored.location.address;

            DBGMSG_PUB(1, "%%trampoline patching address %p = %p",
		       address, *address);

            state->swizzle_patch = address;
            if(*address != CSPROF_TRAMPOLINE_ADDRESS) {
                state->swizzle_return = *address;
                *address = CSPROF_TRAMPOLINE_ADDRESS;
            }
            else {
                DIE("Trampoline not patched out at %#lx", __FILE__, __LINE__,
                    address);
            }
        }
    }
    else {
        /* clear out swizzle info */
        state->swizzle_patch = 0;
        state->swizzle_return = 0;
    }

    csprof_state_flag_set(state, CSPROF_THRU_TRAMP);
    csprof_state_flag_clear(state, CSPROF_EXC_HANDLING | CSPROF_SIGNALED_DURING_TRAMPOLINE);

    
    return return_ip;
}

void
csprof_trampoline2_end()
{
}
#endif
