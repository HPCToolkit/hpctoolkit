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
// Copyright ((c)) 2002-2017, Rice University
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


//***************************************************************************
// user include files
//***************************************************************************

#include <stdlib.h>

#include "perf-util.h" // event_info_t

#include "include/queue.h" // Singly-linkled list macros

//******************************************************************************
// type declarations
//******************************************************************************

// list of event descriptions
typedef struct event_desc_list_s {
  event_info_t *event_info;
  SLIST_ENTRY(event_desc_list_s) entries;
} event_desc_list_t;


//******************************************************************************
// local variables
//******************************************************************************

// a list of main description of events, shared between threads
// once initialize, this list doesn't change (but event description can change)

static SLIST_HEAD(event_desc_list_head_s, event_desc_list_s) list_event_desc_head = 
                                SLIST_HEAD_INITIALIZER(event_desc_list_head_s);



//******************************************************************************
// Public functions
//******************************************************************************

/**
 * add an event descriptor into the list
 * return 1 if successful
 * return -1 if fails
 */
int
event_desc_add(event_info_t *event)
{
  event_desc_list_t *item = (event_desc_list_t*) hpcrun_malloc(sizeof(event_desc_list_t));
  if (item == NULL)
    return -1;

  item->event_info = event;

  SLIST_INSERT_HEAD(&list_event_desc_head, item, entries);
  return 1;
}

event_info_t*
event_desc_find(int metric)
{
  event_desc_list_t *item = NULL;

  SLIST_FOREACH(item, &list_event_desc_head, entries) {
    if (item != NULL && item->event_info->metric == metric)
      return item->event_info;
  }
  return NULL;
}

int
event_desc_get_num()
{
  event_desc_list_t *item = NULL;
  int num_event_desc = 0;

  SLIST_FOREACH(item, &list_event_desc_head, entries) {
    num_event_desc++;
  }
  return num_event_desc;
}

event_desc_list_t*
event_desc_get_first()
{
  return SLIST_FIRST(&list_event_desc_head);
}

event_desc_list_t*
event_desc_get_next(event_desc_list_t *item)
{
  if (item == NULL)
    return SLIST_FIRST(&list_event_desc_head);

  return SLIST_NEXT(item, entries);
}

event_info_t*
event_desc_get_event_info(event_desc_list_t *item)
{
  return item->event_info;
}

