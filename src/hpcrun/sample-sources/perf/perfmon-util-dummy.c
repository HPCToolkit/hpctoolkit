// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//
// Dummy interface for perfmon functions used in linux_perf.c, for the
// case that perfmon is not available.
//


//******************************************************************************
// include files
//******************************************************************************

#define _GNU_SOURCE

#include <linux/perf_event.h>
#include "perfmon-util.h"
#include "perf-util.h"


//******************************************************************************
// Supported operations
//******************************************************************************

/**
 * get the complete perf event attribute for a given pmu
 * @return 1 if successful
 * -1 otherwise
 */
int
pfmu_getEventAttribute(const char *eventname, struct perf_event_attr *event_attr)
{
  return -1;
}


// return 0 or positive if the event exists, -1 otherwise
// if the event exist, code and type are the code and type of the event
int
pfmu_getEventType(const char *eventname, u64 *code, u64 *type)
{
  return -1;
}


/*
 * interface to check if an event is "supported"
 * "supported" here means, it matches with the perfmon PMU event
 *
 * return 0 or positive if the event exists, -1 otherwise
 */
int
pfmu_isSupported(const char *eventname)
{
  return -1;
}


/**
 * Initializing perfmon
 * return 1 if the initialization passes successfully
 **/
int
pfmu_init()
{
  return -1;
}


void
pfmu_fini()
{
  return;
}


/*
 * interface function to print the list of supported PMUs
 */
int
pfmu_showEventList()
{
  return -1;
}

/****
 * Try to retrieve the description of an event or a sub-event.
 * This routine depends on the result of libpfm4, and if the library doesn't
 * have the answer, it will return the event's name itself.
 *
 * @note This routine doesn't recognize events with umasks or modifiers.
 *  So if the event_name contains umasks like XXX:u=4, then it will return
 *  the description of event XXX.
 *
 * @param event_name
 *          The name of the event (or sub-event)
 * @return
 *          The description of the event (or sub-event)
 */
const char* pfmu_getEventDescription(const char *event_name)
{
  return "";
}
