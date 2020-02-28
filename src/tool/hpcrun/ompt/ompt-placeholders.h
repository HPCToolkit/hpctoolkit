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
// Copyright ((c)) 2002-2020, Rice University
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

#ifndef __OMPT_PLACEHOLDERS_H__
#define __OMPT_PLACEHOLDERS_H__

//***************************************************************************
// local include files
//***************************************************************************

#include <hpcrun/utilities/ip-normalized.h>

#include "ompt-types.h"



//***************************************************************************
// macros
//***************************************************************************

#define FOREACH_OMPT_PLACEHOLDER_FN(macro)		\
  /**** OpenMP state placeholders ****/			\
  macro (ompt_idle_state)				\
  macro (ompt_overhead_state)				\
  macro (ompt_barrier_wait_state)			\
  macro (ompt_task_wait_state)				\
  macro (ompt_mutex_wait_state)				\
							\
  /**** OpenMP target operation placeholders ****/	\
  macro (ompt_tgt_alloc)				\
  macro (ompt_tgt_delete)				\
  macro (ompt_tgt_copyin)				\
  macro (ompt_tgt_copyout)				\
  macro (ompt_tgt_kernel)				\
  macro (ompt_tgt_none)					\
  							\
  /**** OpenMP unresolved parallel region ****/		\
  /* The placeholder below is used when the context */	\
  /* for a parallel region remains unresolved at    */	\
  /* the end of a program execution. This can       */	\
  /* happen if an execution is interrupted before   */	\
  /* the region is resolved.                        */	\
  macro (ompt_region_unresolved)


//***************************************************************************
// types
//***************************************************************************

typedef struct {
  void           *pc;
  ip_normalized_t pc_norm; 
} ompt_placeholder_t;


typedef struct {
#define declare_ompt_ph(f) ompt_placeholder_t f;
  FOREACH_OMPT_PLACEHOLDER_FN(declare_ompt_ph);
#undef declare_ompt_ph 
} ompt_placeholders_t; 



//***************************************************************************
// forward declarations
//***************************************************************************

extern ompt_placeholders_t ompt_placeholders;



//***************************************************************************
// interface operations 
//***************************************************************************

void
ompt_init_placeholders
(
 void
);

#endif
