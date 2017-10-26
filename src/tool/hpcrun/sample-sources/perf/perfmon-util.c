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
// Copyright ((c)) 2002-2017, Rice University
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

// taken from perfmon/pfmlib_perf_event.h
// we don't want to include this file because it requires perfmon's perf_event.h
// which is not compatible with the official perf_event.h
/*
 * use with PFM_OS_PERF, PFM_OS_PERF_EXT for pfm_get_os_event_encoding()
 */
typedef struct {
  struct perf_event_attr *attr;   /* in/out: perf_event struct pointer */
  char **fstr;                    /* out/in: fully qualified event string */
  size_t size;                    /* sizeof struct */
  int idx;                        /* out: opaque event identifier */
  int cpu;                        /* out: cpu to program, -1 = not set */
  int flags;                      /* out: perf_event_open() flags */
  int pad0;                       /* explicit 64-bit mode padding */
} pfm_perf_encode_arg_t;



//******************************************************************************
// Constants
//******************************************************************************

#define MAX_EVENT_NAME_CHARS 	256


//******************************************************************************
// forward declarations
//******************************************************************************

int 
pfmu_getEventType(const char *eventname, u64 *code, u64 *type);

//******************************************************************************
// local operations
//******************************************************************************



static int
event_has_pname(char *s)
{
  char *p;
  return (p = strchr(s, ':')) && *(p+1) == ':';
}


/*
 * test if the kernel can create the given event
 * @param code: config, @type: type of event
 * @return: file descriptor if successful, -1 otherwise
 *  caller needs to check errno for the root cause
 */ 
static int
test_pmu(uint64_t code, uint64_t type) 
{
  struct perf_event_attr event_attr;
  memset(&event_attr, 0, sizeof(event_attr));
  event_attr.disabled = 1;

  event_attr.size = sizeof(struct perf_event_attr);
  event_attr.type = type;
  event_attr.config = code;

  // do not check if the event can run with kernel access
  event_attr.exclude_kernel = 1;
  event_attr.exclude_hv     = 1;
  event_attr.exclude_idle   = 1;

  int fd = perf_event_open(&event_attr, 0, -1, -1, 0);
  if (fd == -1) {
    return -1;
  }
  close(fd);
  return fd;
}

/******************
 * show the information of a given pmu event
 * the display will be formatted by display library
 ******************/
static void
show_event_info(pfm_event_info_t *info)
{
  pfm_event_attr_info_t ainfo;
  pfm_pmu_info_t pinfo;
  int i, ret;

  memset(&ainfo, 0, sizeof(ainfo));
  memset(&pinfo, 0, sizeof(pinfo));

  pinfo.size = sizeof(pinfo);
  ainfo.size = sizeof(ainfo);

  ret = pfm_get_pmu_info(info->pmu, &pinfo);
  if (ret) {
    EMSG( "cannot get pmu info: %s", pfm_strerror(ret));
    return;
  }
  u64 code, type;
  if (pfmu_getEventType(info->name, &code, &type) >0 ) {
    if (test_pmu(code, type)>=0) {

      char buffer[MAX_EVENT_NAME_CHARS];

      sprintf(buffer, "%s::%s", pinfo.name, info->name);
      display_line_single(stdout);
      display_event_info(stdout, buffer, info->desc);

      pfm_for_each_event_attr(i, info) {
        ret = pfm_get_event_attr_info(info->idx, i, PFM_OS_NONE, &ainfo);
        if (ret == PFM_SUCCESS)
        {
          memset(buffer, 0, MAX_EVENT_NAME_CHARS);
          sprintf(buffer, "%s::%s:%s", pinfo.name, info->name, ainfo.name);
          display_event_info(stdout, buffer, ainfo.desc);
        }
      }
    }
  }
}

/******************
 * show the list of supported events
 ******************/
static int
show_info(char *event )
{
  pfm_pmu_info_t pinfo;
  pfm_event_info_t info;
  int i, j, ret, match = 0, pname;

  memset(&pinfo, 0, sizeof(pinfo));
  memset(&info, 0, sizeof(info));

  pinfo.size = sizeof(pinfo);
  info.size = sizeof(info);

  pname = event_has_pname(event);

  /*
   * scan all supported events, incl. those
   * from undetected PMU models
   */
  pfm_for_all_pmus(j) {

    ret = pfm_get_pmu_info(j, &pinfo);
    if (ret != PFM_SUCCESS)
      continue;

    /* no pmu prefix, just look for detected PMU models */
    if (!pname && !pinfo.is_present)
      continue;

    for (i = pinfo.first_event; i != -1; i = pfm_get_event_next(i)) {
      ret = pfm_get_event_info(i, PFM_OS_NONE, &info);
      if (ret != PFM_SUCCESS)
        EMSG( "cannot get event info: %s", pfm_strerror(ret));
      else {
        show_event_info(&info);
        match++;
      }
    }
  }
  return match;
}


//******************************************************************************
// Supported operations
//******************************************************************************

// return 0 or positive if the event exists, -1 otherwise
// if the event exist, code and type are the code and type of the event
int 
pfmu_getEventType(const char *eventname, u64 *code, u64 *type)
{
  pfm_perf_encode_arg_t arg;
  char *fqstr = NULL;

  arg.fstr = &fqstr;
  arg.size = sizeof(pfm_perf_encode_arg_t);
  struct perf_event_attr attr;
  memset(&attr, 0, sizeof(struct perf_event_attr));

  arg.attr = &attr;
  int ret = pfm_get_os_event_encoding(eventname, PFM_PLM0|PFM_PLM3, PFM_OS_PERF_EVENT, &arg);

  if (ret == PFM_SUCCESS) {
    *type = arg.attr->type;
    *code = arg.attr->config;
    return 1;
  }
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
  u64 eventcode, eventtype;
  return pfmu_getEventType(eventname, &eventcode, &eventtype);
}


/**
 * Initializing perfmon
 * return 1 if the initialization passes successfully
 **/
int
pfmu_init()
{
  /* to allow encoding of events from non detected PMU models */
  int ret = setenv("LIBPFM_ENCODE_INACTIVE", "1", 1);
  if (ret != PFM_SUCCESS)
    EMSG( "cannot force inactive encoding");

  ret = pfm_initialize();
  if (ret != PFM_SUCCESS)
    EMSG( "cannot initialize libpfm: %s", pfm_strerror(ret));

  return 1;
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
  static char *argv_all =  ".*";

  int total_supported_events = 0;
  int total_available_events = 0;
  int i, ret;
  pfm_pmu_info_t pinfo;

  memset(&pinfo, 0, sizeof(pinfo));
  pinfo.size = sizeof(pinfo);

  static const char *pmu_types[]={
      "unknown type",
      "core",
      "uncore",
      "OS generic",
  };

  printf("Detected PMU models:\n");
  
  pfm_for_all_pmus(i) {
    ret = pfm_get_pmu_info(i, &pinfo);
    if (ret != PFM_SUCCESS)
      continue;

    if (pinfo.is_present) {
      if (pinfo.type >= PFM_PMU_TYPE_MAX)
        pinfo.type = PFM_PMU_TYPE_UNKNOWN;

      printf("\t[%d, %s, \"%s\", %d events, %d max encoding, %d counters, %s PMU]\n",
          i,
          pinfo.name,
          pinfo.desc,
          pinfo.nevents,
          pinfo.max_encoding,
          pinfo.num_cntrs + pinfo.num_fixed_cntrs,
          pmu_types[pinfo.type]);

      total_supported_events += pinfo.nevents;
    }
    total_available_events += pinfo.nevents;
  }
  printf("Total events: %d available, %d supported\n", total_available_events, total_supported_events);

  display_line_single(stdout);

  show_info(argv_all);

  pfm_terminate();

  return 0;
}

