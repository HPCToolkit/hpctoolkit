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
// Copyright ((c)) 2002-2010, Rice University 
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

void
csprof_undo_swizzled_data(csprof_state_t *state, void *ctx)
{
#ifdef CSPROF_TRAMPOLINE_BACKEND
    if(csprof_swizzle_patch_is_valid(state)) {
        csprof_remove_trampoline(state, ctx);
    }
#endif
}

void
csprof_swizzle_with_context(csprof_state_t *state, void *ctx)
{
#ifdef CSPROF_TRAMPOLINE_BACKEND
    struct lox l;
    int proceed = csprof_find_return_address_for_context(state, &l, ctx);

    if(proceed) {
        /* record */
        if(l.stored.type == REGISTER) {
            state->swizzle_patch = (void **) l.stored.location.reg;
        }
        else {
            state->swizzle_patch = l.stored.location.address;
        }
        /* recording the return is machine-dependent */

        csprof_insert_trampoline(state, &l, ctx);
    }
#endif
}
