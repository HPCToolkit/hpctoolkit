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


//***************************************************************************
// user include files
//***************************************************************************

#include <strings.h>       //  strcasecmp

#include "include/queue.h" // Singly-linkled list macros

#include "sample-sources/display.h"

#include "event_custom.h"

//*************************** type data structure **************************

// list of callbacks
typedef struct events_list_s {
  event_custom_t *event;
  SLIST_ENTRY(events_list_s) entries;
} events_list_t;


//*************************** Local variables **************************

static SLIST_HEAD(event_list_head, events_list_s) list_events_head =
	SLIST_HEAD_INITIALIZER(event_list_head);


//*************************** Interfaces **************************

event_custom_t*
event_custom_find(const char *name)
{
  events_list_t *item = NULL;

  // check if we already have the event
  SLIST_FOREACH(item, &list_events_head, entries) {
	if (item != NULL && strcasecmp(item->event->name, name)==0)
	  return item->event;
  }
  return NULL;
}

void
event_custom_display(FILE *std)
{
  if (SLIST_EMPTY(&list_events_head)) {
    return;
  }
  events_list_t *item = NULL;

  display_header(stdout, "Customized perf-event based events");
  fprintf(std, "Name\t\tDescription\n");
  display_line_single(stdout);

  // check if we already have the event
  SLIST_FOREACH(item, &list_events_head, entries) {
    if (item != NULL) {
       display_event_info(stdout, item->event->name, item->event->desc);
    }
  }
  fprintf(std, "\n");
}

// --------------------------------------------------------------
// Adding a new event on the head without checking if it already exists or not
// --------------------------------------------------------------
int
event_custom_register(event_custom_t *event)
{
  events_list_t item;
  item.event = event_custom_find(event->name);
  if (item.event != NULL)
    return 0;

  events_list_t *list_item = (events_list_t *) hpcrun_malloc(sizeof(events_list_t));
  if (list_item == NULL)
    return -1;

  list_item->event = event;

  SLIST_INSERT_HEAD(&list_events_head, list_item, entries);
  return 1;
}

// --------------------------------------------------------------
// Create a custom event
// --------------------------------------------------------------
/**
 * create and register an event if the name is part of customized event
 * returns the number of created events
 * returns 0 if the name is not part of custom events
 */
int
event_custom_create_event(sample_source_t *self, kind_info_t *kb_kind, char *name)
{
  event_custom_t *event = event_custom_find(name);

  if (event == NULL) {
    return 0;
  }

  struct event_threshold_s default_threshold;
  perf_util_get_default_threshold( &default_threshold );

  return event->register_fn(self, kb_kind, event, &default_threshold);
}

event_accept_type_t
event_custom_pre_handler(event_handler_arg_t *args)
{
  if (args == NULL || args->current == NULL)
    return NOT_MY_EVENT;

  events_list_t *item = NULL;

  SLIST_FOREACH(item, &list_events_head, entries) {
    if (item != NULL) {
      // if the custom event is inclusive, we call the handler
      //   even if this is not its event
      // if the custom event is exclusive, we call the handler
      //  iff the event is from the custom event

      if (item->event->handle_type == INCLUSIVE &&
          item->event->pre_handler_fn) {

          item->event->pre_handler_fn(args);

      } else if ( item->event->handle_type == EXCLUSIVE &&
                  item->event == args->current->metric_custom) {

        // exclusive event: make sure the event is the same is the current event
        if (item->event->pre_handler_fn)
          return item->event->pre_handler_fn(args);
        else
          return ACCEPT_EVENT;
      }
    }
  }
  return NOT_MY_EVENT;
}


event_accept_type_t
event_custom_post_handler(event_handler_arg_t *args)
{
  if (args == NULL || args->current == NULL)
    return NOT_MY_EVENT;

  events_list_t *item = NULL;

  SLIST_FOREACH(item, &list_events_head, entries) {
    if (item != NULL) {
      // if the custom event is inclusive, we call the handler
      //   even if this is not its event
      // if the custom event is exclusive, we call the handler
      //  iff the event is from the custom event

    	if (item->event->handle_type == INCLUSIVE) {
    		if (item->event->post_handler_fn)
    		  item->event->post_handler_fn(args);

    	} else if (item->event == args->current->metric_custom) {
    	  // exclusive event: make sure the event is the same is the current event
        if (item->event->post_handler_fn)
          return item->event->post_handler_fn(args);
    	}
    }
  }
  return NOT_MY_EVENT;
}


