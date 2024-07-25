// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef _EVENT_NAME_PARSER_H
#define _EVENT_NAME_PARSER_H

typedef struct perf_event_name_s
{
  // the number of events
  int num_events;

  // an array of the name of the events
  // if it's a group of event, the size of the array is `num_events`
  char** event_names;

  // text of frequency or period sampling
  char* how_often;

  // text of the skid precision
  char* skid;

} perf_event_name_t;


/// @brief Attempt to parse an event name into a `perf_event_name_t` struct object
/// structure of perf_event or hpcrun event name
/// hpcrun (perf) event is specified as:
///
/// """
///  hpcrun -e   event_name:skid\@how_often
///  hpcrun -e   event_name1,event_name2,event_name3
///  hpcrun -e   '{event_name1,event_name2,event_name3}'
///  hpcrun -e   '{event_name1,event_name2,event_name3}'\@10000
///  hpcrun -e   '{event_name1,event_name2,event_name3}'\@f100
///  hpcrun -e   '{event_name1,event_name2,event_name3}':skid@1000
/// """
///
/// @param event [in] the original event's name, it can't be NULL or empty
/// @param event_name [out] the perf's data of type [perf_event_name_t](perf_event_name_t)
///
/// @return int number of events in `event` parameter.
///
/// @warning the skid attribute is always empty at the moment
///
int
perf_util_parse_eventname(const char *event, perf_event_name_t* event_name);

///
/// Free string event names allocated by perf_util_extract_eventnames
///
void
perf_util_free_eventname(perf_event_name_t* event_name);

#endif
