// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include <string.h>
#include <stdbool.h>



static char *eventsToExclude[] =  { "realtime", "retcnt" };
static const int numEventsToExclude = 2;

/***
 * Check if an event needs to be excluded or not.
 * @param event
 * @return true if the event has to be excluded, false otherwise
 */
bool is_event_to_exclude(const char *event)
{
  for(int i=0; i<numEventsToExclude ; i++) {
    if (strcmp(event, eventsToExclude[i]) == 0) {
      return true;
    }
  }
  return false;
}
