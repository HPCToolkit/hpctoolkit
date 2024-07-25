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

// uncomment the line below for unit test
//#define UNIT_TEST_EVENT_NAME_PARSER

#ifdef UNIT_TEST_EVENT_NAME_PARSER
#include <stdio.h>
int
main(int argc, char *argv[])
{
    const char *eventnames[] = {
        "event_name",
        "event1,event2",
        "event_name@100",
        "event_name@f100",
        "'{event1,event2}'",
        "'{event1,event2}'@100",
        "'{event1,event2}'@f10",
        "event_name:p",
        "event_name:pp@100",
        "event_name:P@f2000",
        "event_name:ppp@f100",
        "'{event1,event2}':ppp",
        "'{event1,event2}':ppp@100",
        "'{event1,event2}':ppp@f100",
        "module::event:p_modifier=1:p_umask=2:123_bytes",
        "module::event:p_modifier=1:p_umask=2:123_bytes:p",
        "module::event:p_modifier=1:p_umask=2:123_bytes:pp",
        "module::event:p_modifier=1:p_umask=2:123_bytes:ppp",
        "module::event:p_modifier=1:p_umask=2:123_bytes@100000",
        "module::event:p_modifier=1:p_umask=2:123_bytes:p@100000",
        "module::event:p_modifier=1:p_umask=2:123_bytes:pp@100000",
        "module::event:p_modifier=1:p_umask=2:123_bytes:ppp@100000",
        "module::event:p_modifier=1:p_umask=2:123_bytes@f100000"
    };
    int size = sizeof(eventnames) / sizeof(eventnames[0]);

    for(int i=0; i<size; i++) {
        perf_event_name_t obj;

        int result = perf_util_parse_eventname(eventnames[i], &obj);

        printf("[%d]  n=%s,   t: %s   s: %s  \n   e:", i, eventnames[i], obj.how_often, obj.skid);

        for(int j=0; j<obj.num_events; j++) {
            printf(" '%s'    ", obj.event_names[j]);
        }
        printf("\n");
    }
}
#endif
