// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
// user include files
//***************************************************************************

#define _GNU_SOURCE

#include <strings.h>       //  strcasecmp

#include "../../../common/lean/queue.h" // Singly-linkled list macros

#include "../display.h"

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
        if (item != NULL &&     strcasecmp(item->event->name, name)==0)
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

int
event_custom_register(event_custom_t *event)
{
  events_list_t item;
  item.event = event_custom_find(event->name);
  if (item.event != NULL) return 0;

  events_list_t *list_item = (events_list_t *) hpcrun_malloc(sizeof(events_list_t));
  if (list_item == NULL)
        return -1;

  list_item->event = event;

  SLIST_INSERT_HEAD(&list_events_head, list_item, entries);
  return 1;
}
