// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __PERFMON_UTIL_H__
#define __PERFMON_UTIL_H__

#include <linux/perf_event.h>
#include "perf-util.h"    // u64, u32 and perf_mmap_data_t


/// Initializing perfmon (performance monitor tool)
///
/// @return 1 if the initialization passes successfully
///
/// @note need to call pfmu_fini to finalize
int pfmu_init();

/*
 * interface function to print the list of supported PMUs
 */
void pfmu_showEventList();

/// interface to check if an event is "supported"
/// "supported" here means, it matches with the perfmon PMU event
///
/// @param eventname
/// (input) the name of the event
///
/// @return 0 or positive if the event exists, -1 otherwise
int pfmu_isSupported(const char *eventname);

/// Get the perf_event's code and type of an event.
///
/// @param eventname
/// (input) the name of the perf event
///
/// @param code
/// (output) the perf_event's code (aka config)
///
/// @param type
/// (output) the perf_event's type
///
/// @return 0 or positive if the event exists, -1 otherwise
///
/// @note if the event exists, code and type are the code and type of the event.
/// Otherwise, the output is undefined.
int pfmu_getEventType(const char *eventname, u64 *code, u64 *type);

/// get the complete perf event attribute for a given pmu
///
/// @param eventname
/// (input) The name of the event.
/// @param event_attr
/// (output) the attribute for perf_event
///
/// @return 1 if successful
/// -1 otherwise
int pfmu_getEventAttribute(const char *eventname, struct perf_event_attr *event_attr);

/// Try to retrieve the description of an event or a sub-event.
/// This routine depends on the result of libpfm4, and if the library doesn't
/// have the answer, it will return the event's name itself.
///
/// @note This routine doesn't recognize events with umasks or modifiers.
/// So if the event_name contains umasks like XXX:u=4, then it will return
/// the description of event XXX.
///
/// @param event_name
/// The name of the event (or sub-event)
/// @return
/// The description of the event (or sub-event)
const char* pfmu_getEventDescription(const char *event_name);

/// Finalize the use of perf_event monitor utility
void pfmu_fini();

#endif
