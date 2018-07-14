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
// Copyright ((c)) 2002-2018, Rice University
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

//
// Dummy interface for perfmon functions used in linux_perf.c, for the
// case that perfmon is not available.
//


//******************************************************************************
// include files
//******************************************************************************

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
