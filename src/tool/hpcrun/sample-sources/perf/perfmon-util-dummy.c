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
// Utility interface for perfmon
//



/******************************************************************************
 * system includes
 *****************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>

/******************************************************************************
 * linux specific headers
 *****************************************************************************/
#include <linux/perf_event.h>

/******************************************************************************
 * hpcrun includes
 *****************************************************************************/

#include <hpcrun/messages/messages.h>
#include "perf-util.h"    // u64, u32 and perf_mmap_data_t
#include "sample-sources/display.h"

/******************************************************************************
 * perfmon
 *****************************************************************************/
#include <perfmon/pfmlib.h>


//******************************************************************************
// data structure
//******************************************************************************




//******************************************************************************
// Constants
//******************************************************************************

#define MAX_EVENT_NAME_CHARS 	256
#define MAX_EVENT_DESC_CHARS 	4096

#define EVENT_IS_PROFILABLE	 0
#define EVENT_MAY_NOT_PROFILABLE 1
#define EVENT_FATAL_ERROR	-1

//******************************************************************************
// forward declarations
//******************************************************************************


//******************************************************************************
// local operations
//******************************************************************************



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
  pfm_terminate();
}

/*
 * interface function to print the list of supported PMUs
 */
int
pfmu_showEventList()
{
  return -1;
}

