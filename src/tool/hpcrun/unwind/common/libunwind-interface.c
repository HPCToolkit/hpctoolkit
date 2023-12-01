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
// Copyright ((c)) 2002-2023, Rice University
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

#include "libunwind-interface.h"

#include "../../audit/binding.h"
#include "hpctoolkit-config.h"

#include <stddef.h>

pfn_unw_getcontext_t libunwind_getcontext = NULL;
pfn_unw_init_local_t libunwind_init_local = NULL;
pfn_unw_init_local2_t libunwind_init_local2 = NULL;
pfn_unw_step_t libunwind_step = NULL;
pfn_unw_reg_states_iterate_t libunwind_reg_states_iterate = NULL;
pfn_unw_apply_reg_state_t libunwind_apply_reg_state = NULL;
pfn_unw_get_reg_t libunwind_get_reg = NULL;
pfn_set_reg_t libunwind_set_reg = NULL;
pfn_unw_get_save_loc_t libunwind_get_save_loc = NULL;

void libunwind_bind() {
  hpcrun_bind_private(HPCTOOLKIT_INSTALL_PREFIX "/lib/hpctoolkit/libhpcrun_unwind.so",
    "libunwind_getcontext", &libunwind_getcontext,
    "libunwind_init_local", &libunwind_init_local,
    "libunwind_init_local2", &libunwind_init_local2,
    "libunwind_step", &libunwind_step,
    "libunwind_reg_states_iterate", &libunwind_reg_states_iterate,
    "libunwind_apply_reg_state", &libunwind_apply_reg_state,
    "libunwind_get_reg", &libunwind_get_reg,
    "libunwind_set_reg", &libunwind_set_reg,
    "libunwind_get_save_loc", &libunwind_get_save_loc,
    NULL);
}
