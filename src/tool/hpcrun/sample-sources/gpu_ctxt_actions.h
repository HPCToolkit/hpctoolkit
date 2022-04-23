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
// Copyright ((c)) 2002-2022, Rice University
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

#ifndef GPU_CTXT_ACTIONS
#define GPU_CTXT_ACTIONS

#include <stdint.h>

// opaque data type -- so that clients do not have to include entire gpu
// API
typedef struct cuda_ctxt_t cuda_ctxt_t;

// Opaque, atomic context counter ops
// reset context counter to 0
extern void cuda_ncontexts_reset(void);
extern uint64_t cuda_ncontexts_incr(void);
extern uint64_t cuda_ncontexts_decr(void);

// NOTE: These functions have hidden side effect !!!
extern cuda_ctxt_t* cuda_capture_ctxt(void);
extern void cuda_set_ctxt(cuda_ctxt_t* ctxt);
extern void* cuda_get_handle(cuda_ctxt_t* ctxt);
extern void cuda_ncontexts_reset(void);
#endif  // GPU_CTXT_ACTIONS
