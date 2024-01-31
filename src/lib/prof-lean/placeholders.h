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
// Copyright ((c)) 2002-2024, Rice University
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

#include <stdint.h>

#ifndef _HPCTOOLKIT_PLACEHOLDERS_H_
#define _HPCTOOLKIT_PLACEHOLDERS_H_

#ifdef __cplusplus
extern "C" {
#endif

//*****************************************************************************
// macros
//*****************************************************************************

#define PLACEHOLDER_VALUE(A,B,C,D,E,F,G,H) \
  ( (uint64_t)(((A) & 0xffULL) << 56) \
  | (uint64_t)(((B) & 0xffULL) << 48) \
  | (uint64_t)(((C) & 0xffULL) << 40) \
  | (uint64_t)(((D) & 0xffULL) << 32) \
  | (uint64_t)(((E) & 0xffULL) << 24) \
  | (uint64_t)(((F) & 0xffULL) << 16) \
  | (uint64_t)(((G) & 0xffULL) << 8) \
  | (uint64_t)(((H) & 0xffULL)) )

//*****************************************************************************
// types
//*****************************************************************************

// Enumeration of all offsets in load module 0, which have special meanings.
// For debuggability, all values are encoded "names" (or "shortcodes").
enum hpcrun_placeholder {
  // Placeholders for special cases that should never be presented literally
  hpcrun_placeholder_unnormalized_ip = PLACEHOLDER_VALUE('*','U','n','n','o','r','m',' '),
  hpcrun_placeholder_root_primary    = PLACEHOLDER_VALUE('^','P','r','i','m','a','r','y'),
  hpcrun_placeholder_root_partial    = PLACEHOLDER_VALUE('^','P','a','r','t','i','a','l'),

  // Fences to indicate the source of all contexts
  hpcrun_placeholder_fence_main   = PLACEHOLDER_VALUE('|',' ','M','a','i','n',' ',' '),
  hpcrun_placeholder_fence_thread = PLACEHOLDER_VALUE('|',' ','T','h','r','e','a','d'),

  // Marker for regions of idleness within a thread
  hpcrun_placeholder_no_activity = PLACEHOLDER_VALUE('N','o','A','c','t','v','t','y'),

  // OpenMP abstracted operations
  hpcrun_placeholder_ompt_idle_state         = PLACEHOLDER_VALUE('O','M','P',' ','I','d','l','e'),
  hpcrun_placeholder_ompt_overhead_state     = PLACEHOLDER_VALUE('O','M','P','O','v','r','H','d'),
  hpcrun_placeholder_ompt_barrier_wait_state = PLACEHOLDER_VALUE('O','M','P','B','a','r','r','W'),
  hpcrun_placeholder_ompt_task_wait_state    = PLACEHOLDER_VALUE('O','M','P','T','a','s','k','W'),
  hpcrun_placeholder_ompt_mutex_wait_state   = PLACEHOLDER_VALUE('O','M','P','M','t','e','x','W'),
  // OpenMP target abstracted operations
  hpcrun_placeholder_ompt_tgt_alloc   = PLACEHOLDER_VALUE('O','M','T','A','l','l','o','c'),
  hpcrun_placeholder_ompt_tgt_delete  = PLACEHOLDER_VALUE('O','M','T','D','e','l','t','e'),
  hpcrun_placeholder_ompt_tgt_copyin  = PLACEHOLDER_VALUE('O','M','T','C','p','I','n',' '),
  hpcrun_placeholder_ompt_tgt_copyout = PLACEHOLDER_VALUE('O','M','T','C','p','O','u','t'),
  hpcrun_placeholder_ompt_tgt_kernel  = PLACEHOLDER_VALUE('O','M','T','K','e','r','n','l'),
  hpcrun_placeholder_ompt_tgt_none    = PLACEHOLDER_VALUE('O','M','T',' ','N','o','n','e'),
  // Placeholder to indicate when an OpenMP region never resolved (e.g. crash)
  hpcrun_placeholder_ompt_region_unresolved = PLACEHOLDER_VALUE('O','M','P','U','r','e','s','v'),

  // GPU abstract operations
  hpcrun_placeholder_gpu_copy    = PLACEHOLDER_VALUE('G','P','U','C','p','?','2','?'),
  hpcrun_placeholder_gpu_copyin  = PLACEHOLDER_VALUE('G','P','U','C','p','H','2','D'),
  hpcrun_placeholder_gpu_copyout = PLACEHOLDER_VALUE('G','P','U','C','p','D','2','H'),
  hpcrun_placeholder_gpu_alloc   = PLACEHOLDER_VALUE('G','P','U','A','l','l','o','c'),
  hpcrun_placeholder_gpu_delete  = PLACEHOLDER_VALUE('G','P','U','D','e','l','t','e'),
  hpcrun_placeholder_gpu_kernel  = PLACEHOLDER_VALUE('G','P','U','K','e','r','n','l'),
  hpcrun_placeholder_gpu_memset  = PLACEHOLDER_VALUE('G','P','U','M','e','m','s','t'),
  hpcrun_placeholder_gpu_sync    = PLACEHOLDER_VALUE('G','P','U',' ','S','y','n','c'),
  hpcrun_placeholder_gpu_trace   = PLACEHOLDER_VALUE('G','P','U','T','r','a','c','e'),
};

//*****************************************************************************
// interface operations
//*****************************************************************************

// Get the "pretty" string name of the given placeholder, or NULL if unknown.
const char *get_placeholder_name(uint64_t placeholder);

// Load module of all the placeholders
#define HPCRUN_PLACEHOLDER_LM ((uint16_t)0)

// In hpcrun, get the normalized ip for a placeholder
#define get_placeholder_norm(PH) ((ip_normalized_t){HPCRUN_PLACEHOLDER_LM, PH})

#define is_placeholder(ip) ((ip).lm_id == HPCRUN_PLACEHOLDER_LM)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif
