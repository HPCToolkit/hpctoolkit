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
// Copyright ((c)) 2002-2014, Rice University
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


//******************************************************************************
// local includes  
//******************************************************************************

#ifndef OMPT_THREAD_H
#define OMPT_THREAD_H

#include <ompt.h>

#define MAX_NESTING_LEVELS 128

//******************************************************************************
// interface operations 
//******************************************************************************

void ompt_thread_type_set(ompt_thread_type_t ttype);

ompt_thread_type_t ompt_thread_type_get();

extern __thread ompt_thread_regions_list_t* registered_regions;
extern __thread ompt_threads_queue_t threads_queue;

// freelists
extern __thread ompt_notification_t* notification_freelist_head;
extern __thread ompt_thread_regions_list_t* thread_region_freelist_head;
extern __thread ompt_thread_region_freelist_t public_region_freelist;
extern __thread ompt_region_data_t* private_region_freelist_head;

// stack that contais all nested parallel region
extern __thread uint64_t region_stack[];
extern  __thread int top_index;

uint64_t top_region_stack();
uint64_t pop_region_stack();
void push_region_stack(uint64_t region_id);
void clear_region_stack();
int is_empty_region_stack();

#endif