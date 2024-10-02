// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#include <stdlib.h>
#include <string.h>

#include "event_name_parser.h"

#define EVENT_DELIMITER_STRING     ","
#define EVENT_DELIMITER_CHR        ','


int
perf_util_parse_eventname(const char *event, perf_event_name_t* event_name)
{
  // check if it's a single event or a group of events
  // a group of events is specified with curly brackets {e1,e2,..}
  char *start = strchr(event, '{');
  char *end;
  const char *event_property;
  char* pure_event_name = malloc(strlen(event)+1);

  if (start != NULL) {
      start++; // Skip the opening brace
      end = strchr(start, '}');

      if (end == NULL)
        return -1;

      strncpy(pure_event_name, start, end - start);
      pure_event_name[end - start] = '\0';

      // to find the property string
      event_property = end+1;
  } else {
    // we don't know yet how to calculate the pure name here
    // we need to remove the skid and the how_often
    event_property = event;
  }

  // check if a period or frequency is specified
  //  event@how_often
  //
  event_name->how_often = NULL;
  event_name->skid = NULL;

  char *at_sign = strchr(event_property, '@');
  if (at_sign != NULL) {
    event_name->how_often = strdup(++at_sign);

    if (event_property == event) {
      // case for no group with at_sign
      int len = at_sign-event-1;
      strncpy(pure_event_name, event, len);
      pure_event_name[len] = '\0';
    }

  } else if (event_property == event) {
    // case for no group, no at_sign
    strcpy(pure_event_name, event);
  }

  // count the number of events so that we can allocate an array
  // this is an ugly hack but I don't know how to make it elegant
  char* ptr_event = pure_event_name;
  int num_events = 1;
  for(int i=0; ptr_event[i] != '\0'; i++) {
    if (ptr_event[i] == EVENT_DELIMITER_CHR)
      num_events++;
  }
  event_name->num_events = num_events;
  event_name->event_names = (char**) malloc(sizeof(char*) * num_events );

  // Split the string into keywords separated by a comma
  int count = 0;
  char *token = strtok(pure_event_name, EVENT_DELIMITER_STRING);

  while (token != NULL && count < num_events) {
      event_name->event_names[count] = strdup(token);

      if (event_name->event_names[count] == NULL) {
        // not enough memory. Perhaps we should exit the program instead?
        return count;
      }
      count++;
      token = strtok(NULL, EVENT_DELIMITER_STRING);
  }
  free(pure_event_name);

  return count;
}


/****
 * Free string event names allocated by perf_util_extract_eventnames
*/
void
perf_util_free_eventname(perf_event_name_t* event_name)
{
  if (event_name == NULL)
    return;

  if (event_name->event_names != NULL) {
    for(int i=0; i<event_name->num_events; i++) {
      free(event_name->event_names[i]);
    }
    free(event_name->event_names);
  }
  if (event_name->how_often != NULL)
    free(event_name->how_often);
}
