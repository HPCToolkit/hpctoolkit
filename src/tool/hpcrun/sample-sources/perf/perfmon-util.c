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
#define MAX_EVENT_DESC_CHARS 	4096

#define EVENT_IS_PROFILABLE	 0
#define EVENT_MAY_NOT_PROFILABLE 1
#define EVENT_FATAL_ERROR	-1

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
create_event(uint64_t code, uint64_t type) 
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
 * test a pmu if it's profilable or not
 * @return 0 if it's fine. 1 otherwise
 ******************/ 
static int
test_pmu(char *evname)
{
  u64  code, type;
  
  if (pfmu_getEventType(evname, &code, &type) >0 ) {
    if (create_event(code, type)>=0) {
      return EVENT_IS_PROFILABLE;
    }
  }
  return EVENT_MAY_NOT_PROFILABLE;
}

/******************
 * show the information of a given pmu event
 * the display will be formatted by display library
 *
 * @param: perf event information
 * @return: 
 *   0 if profilable, 
 *  -1 if there's a fatal error
 *   1 otherwise
 ******************/
static int
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
    return EVENT_FATAL_ERROR;
  }

  char buffer[MAX_EVENT_NAME_CHARS];
  char buffer_desc[MAX_EVENT_DESC_CHARS];

  sprintf(buffer, "%s::%s", pinfo.name, info->name);
  int profilable = test_pmu(buffer);

  display_line_single(stdout);
  
  if (profilable == EVENT_IS_PROFILABLE) {
    display_event_info(stdout, buffer, info->desc);
  } else {
    sprintf(buffer_desc, "%s (*)", info->desc);
    display_event_info(stdout, buffer, buffer_desc);
  }

  pfm_for_each_event_attr(i, info) {
    ret = pfm_get_event_attr_info(info->idx, i, PFM_OS_NONE, &ainfo);
    if (ret == PFM_SUCCESS)
    {
      memset(buffer, 0, MAX_EVENT_NAME_CHARS);
      sprintf(buffer, "%s::%s:%s", pinfo.name, info->name, ainfo.name);

      if (test_pmu(buffer) == 0) {
        display_event_info(stdout, buffer, ainfo.desc);
	continue;
      }

      // the counter may not be profilable. Perhaps requires more attributes/masks 
      // or higher user privilege (like super user)
      // add a sign to users so they know the event may not be profilable
      sprintf(buffer_desc, "%s (*)", ainfo.desc);
      display_event_info(stdout, buffer, buffer_desc);

      profilable = EVENT_MAY_NOT_PROFILABLE;
    }
  }
  return profilable;
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
  int profilable;

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
        profilable |= show_event_info(&info);
        match++;
      }
    }
  }
  if (profilable != EVENT_IS_PROFILABLE) 
    printf("(*) The counter may not be profilable.\n\n");
  
  return match;
}


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
  pfm_perf_encode_arg_t arg;
  char *fqstr = NULL;

  arg.fstr = &fqstr;
  arg.size = sizeof(pfm_perf_encode_arg_t);
  struct perf_event_attr attr;
  memset(&attr, 0, sizeof(struct perf_event_attr));

  arg.attr = &attr;
  int ret = pfm_get_os_event_encoding(eventname, PFM_PLM0|PFM_PLM3, PFM_OS_PERF_EVENT, &arg);

  if (ret == PFM_SUCCESS) {
    memcpy(event_attr, arg.attr, sizeof(struct perf_event_attr));
    return 1;
  }
  return -1;
}

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
    EMSG( "libpfm: cannot force inactive encoding");

  // pfm_initialize is idempotent, so it is not a problem if
  // another library (e.g., PAPI) also calls this.
  ret = pfm_initialize();
  if (ret != PFM_SUCCESS)
    EMSG( "libpfm: cannot initialize: %s", pfm_strerror(ret));

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

  return 0;
}

