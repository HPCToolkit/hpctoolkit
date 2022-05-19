// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
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

#ifndef cupti_range_thread_list_h
#define cupti_range_thread_list_h

#include <stdbool.h>

#include <hpcrun/thread_data.h>

#include "cupti-cct-trie.h"

#define CUPTI_RANGE_THREAD_NULL -1

// A thread invokes the callback function when it enters a range
// and there's a pending notification
typedef void (*cupti_range_thread_list_fn_t)
(
 int thread_id,
 void *entry,
 void *args
);

// Adds the current thread to cupti range thread list
void cupti_range_thread_list_add();

// Applies the callback function
void cupti_range_thread_list_apply(cupti_range_thread_list_fn_t fn, void *args);

// Updates a thread's cct trie based on the notification
void cupti_range_thread_list_notification_update(void *entry, uint32_t context_id, uint32_t range_id, bool active, bool logic);

// Clears the notification for the current thread
void cupti_range_thread_list_notification_clear();

// Gets the number of threads in the list
int cupti_range_thread_list_num_threads();

// Gets the id of the current thread
int cupti_range_thread_list_id_get();

// Gets context id of the notification thread
uint32_t cupti_range_thread_list_notification_context_id_get();

// Gets range id of the notification thread
uint32_t cupti_range_thread_list_notification_range_id_get();

// Gets active status of the notification thread
bool cupti_range_thread_list_notification_active_get();

// Gets logic status of the notification thread
bool cupti_range_thread_list_notification_logic_get();

// Removes all thread entries in the list
void cupti_range_thread_list_clear();

#endif 
