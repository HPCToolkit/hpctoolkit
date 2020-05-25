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

#ifndef __OMPT_TASK_H__
#define __OMPT_TASK_H__


//*****************************************************************************
// local includes
//*****************************************************************************

#include "omp-tools.h"
#include <lib/prof-lean/stacks.h>

// ompt_task_data_t stack declaration
typedef struct ompt_task_data_t typed_stack_elem(task);
typed_stack_declare_type(task);
typed_stack_declare(task, sstack);

//*****************************************************************************
// interface operations
//*****************************************************************************

typed_stack_elem_ptr(task)
ompt_task_acquire
(
 cct_node_t *callpath,
 ompt_task_flag_t task_type
);


void
ompt_task_release
(
 ompt_data_t *t
);


cct_node_t *
ompt_task_callpath
(
 ompt_data_t *task_data
);


void 
ompt_task_register_callbacks
(
 ompt_set_callback_t ompt_set_callback_fn
);

#endif
