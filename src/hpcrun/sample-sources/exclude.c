// SPDX-FileCopyrightText: 2002-2024 Rice University
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
bool is_event_to_exclude(char *event)
{
  for(int i=0; i<numEventsToExclude ; i++) {
    if (strcmp(event, eventsToExclude[i]) == 0) {
      return true;
    }
  }
  return false;
}

#ifdef UNIT_TEST_THIS
#include <stdio.h>

int main() {
  char *events[] = { "re", "realtime", "ret", "retcnt", "", "//" };

  for(int i=0; i<6; i++) {
    printf("check %s : %d\n", events[i], is_event_to_exclude(events[i]));
  }
}
#endif
