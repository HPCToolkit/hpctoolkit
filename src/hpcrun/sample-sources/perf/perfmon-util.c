// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//
// Utility interface for perfmon
//



/******************************************************************************
 * system includes
 *****************************************************************************/

#define _GNU_SOURCE

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

#include "../../messages/messages.h"
#include "perf-util.h"    // u64, u32 and perf_mmap_data_t
#include "perf_event_open.h"
#include "../display.h"

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

#define MAX_EVENT_NAME_CHARS    256
#define MAX_EVENT_DESC_CHARS    4096

#define EVENT_IS_PROFILABLE      0
#define EVENT_MAY_NOT_PROFILABLE 1
#define EVENT_FATAL_ERROR       -1

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
show_event_info(pfm_event_info_t *event_info)
{
  pfm_pmu_info_t pmu_info;

  memset(&pmu_info, 0, sizeof(pmu_info));
  pmu_info.size = sizeof(pmu_info);

  int ret = pfm_get_pmu_info(event_info->pmu, &pmu_info);
  if (ret) {
    EMSG( "cannot get pmu info: %s", pfm_strerror(ret));
    return EVENT_FATAL_ERROR;
  }

  char buffer[MAX_EVENT_NAME_CHARS];
  char buffer_desc[MAX_EVENT_DESC_CHARS];

  sprintf(buffer, "%s::%s", pmu_info.name, event_info->name);
  int profilable = test_pmu(buffer);

  display_line_single(stdout);

  if (profilable == EVENT_IS_PROFILABLE) {
    display_event_info(stdout, buffer, event_info->desc);
  } else {
    sprintf(buffer_desc, "%s (*)", event_info->desc);
    display_event_info(stdout, buffer, buffer_desc);
  }

  int i;

  // print all the sub-events, attributes, modifiers, umasks of this event.
  // also check if the attribute is "profileable" or not.

  pfm_for_each_event_attr(i, event_info) {
    pfm_event_attr_info_t attr_info;
    memset(&attr_info, 0, sizeof(attr_info));
    attr_info.size = sizeof(attr_info);

    ret = pfm_get_event_attr_info(event_info->idx, i, PFM_OS_NONE, &attr_info);
    if (ret == PFM_SUCCESS)
    {
      memset(buffer, 0, MAX_EVENT_NAME_CHARS);
      sprintf(buffer, "%s::%s:%s", pmu_info.name, event_info->name, attr_info.name);

      if (test_pmu(buffer) == 0) {
        display_event_info(stdout, buffer, attr_info.desc);
        continue;
      }

      // the counter may not be profilable. Perhaps requires more attributes/masks
      // or higher user privilege (like super user)
      // add a sign to users so they know the event may not be profilable
      sprintf(buffer_desc, "%s (*)", attr_info.desc);
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

  memset(&pinfo, 0, sizeof(pinfo));
  memset(&info, 0, sizeof(info));

  pinfo.size = sizeof(pinfo);
  info.size = sizeof(info);

  pname = event_has_pname(event);

  printf("(*) Denotes the counter may not be profilable.\n\n");

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
const char *
pfmu_getEventDescription(const char *event_name)
{
  if (event_name == NULL) return NULL;

  pfm_perf_encode_arg_t arg;
  char *fqstr = NULL;

  arg.fstr = &fqstr;
  arg.size = sizeof(pfm_perf_encode_arg_t);
  struct perf_event_attr attr;
  memset(&attr, 0, sizeof(struct perf_event_attr));

  arg.attr = &attr;
  int ret = pfm_get_os_event_encoding(event_name, PFM_PLM0|PFM_PLM3, PFM_OS_PERF_EVENT_EXT, &arg);

  if (ret != PFM_SUCCESS)
    return event_name;

  pfm_event_info_t info;
  memset(&info, 0, sizeof(info));
  info.size = sizeof(info);

  ret = pfm_get_event_info(arg.idx, PFM_OS_NONE, &info);
  if (ret != PFM_SUCCESS)
    return event_name;

  int i;

  // fix hpctoolkit issue #850: iterate all attributes to get the correct description
  // if the PMU has no attribute, it skips the loop and return the parent's description.
  pfm_for_each_event_attr(i, &info) {
    pfm_event_attr_info_t attr_info;
    memset(&attr_info, 0, sizeof(attr_info));

    attr_info.size = sizeof(attr_info);

    ret = pfm_get_event_attr_info(arg.idx, i, PFM_OS_NONE, &attr_info);

    // Only consider sub-event (PFM_ATTR_UMASK) in the event description.
    // For other attributes (such as modifier), it returns the description of the event
    if (ret == PFM_SUCCESS &&
        attr_info.type == PFM_ATTR_UMASK &&
        strstr(event_name, attr_info.name) != NULL)
      return attr_info.desc;
  }
  return info.desc;
}


/**
 * get the complete perf event attribute for a given pmu
 *
 * @param eventname
 *        (input) The name of the event.
 * @param event_attr
 *        (output) the attribute for perf_event
 *
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
  int ret = pfm_get_os_event_encoding(eventname, PFM_PLM0|PFM_PLM3, PFM_OS_PERF_EVENT_EXT, &arg);

  if (ret == PFM_SUCCESS) {
    memcpy(event_attr, arg.attr, sizeof(struct perf_event_attr));
    return 1;
  }
  return -1;
}

/**
 * Get the perf_event's code and type of an event.
 *
 * @param eventname
 *        (input) the name of the perf event
 *
 * @param code
 *        (output) the perf_event's code (aka config)
 *
 * @param type
 *        (output) the perf_event's type
 *
 * @return 0 or positive if the event exists, -1 otherwise
 *
 * @note if the event exists, code and type are the code and type of the event.
 * Otherwise, the output is undefined.
 */
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


/**
 * interface to check if an event is "supported"
 * "supported" here means, it matches with the perfmon PMU event
 *
 * @param eventname
 *        (input) the name of the event
 *
 * @return 0 or positive if the event exists, -1 otherwise
 */
int
pfmu_isSupported(const char *eventname)
{
  u64 eventcode, eventtype;
  return pfmu_getEventType(eventname, &eventcode, &eventtype);
}


/**
 * Initializing perfmon
 * @return 1 if the initialization passes successfully
 **/
int
pfmu_init()
{
  int ret;

  // pfm_initialize is idempotent, so it is not a problem if
  // another library (e.g., PAPI) also calls this.
  ret = pfm_initialize();

  if (ret != PFM_SUCCESS) {
    EMSG( "libpfm: cannot initialize: %s", pfm_strerror(ret));
    return -1;
  }

  return 1;
}

void
pfmu_fini()
{
  pfm_terminate();
}

/*
 * Print to the standard output ALL supported events.
 * This means we use ".*" filter to include all events with no restriction.
 */
void
pfmu_showEventList()
{
  static char *argv_all =  ".*";

  show_info(argv_all);
}
