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

// -----------------------------------------------------
// includes
// -----------------------------------------------------

#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

#define _PERF_UTIL_DEBUG_  0
#if _PERF_UTIL_DEBUG_
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#endif

#include <string.h>

#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>

#include "perf_constants.h"
#include "perf_skid.h"

// -----------------------------------------------------
// precise ip / skid options
// -----------------------------------------------------

// Possible value of precise ip:
//  0  SAMPLE_IP can have arbitrary skid.
//  1  SAMPLE_IP must have constant skid.
//  2  SAMPLE_IP requested to have 0 skid.
//  3  SAMPLE_IP must have 0 skid.
//  4  Detect automatically to have the most precise possible (default)
#define HPCRUN_OPTION_PRECISE_IP "HPCRUN_PRECISE_IP"

// default option for precise_ip: autodetect skid
#define PERF_EVENT_AUTODETECT_SKID    4

// constants of precise_ip (see the man page)
#define PERF_EVENT_SKID_ZERO_REQUIRED    3
#define PERF_EVENT_SKID_ZERO_REQUESTED   2
#define PERF_EVENT_SKID_CONSTANT         1
#define PERF_EVENT_SKID_ARBITRARY        0

#define PRECISE_IP_SUFFIX   ":p"

#define SIGN_EQUAL          '='

#define DELIMITER_PERIOD    '@'

// -----------------------------------------------------
// constants
// -----------------------------------------------------

// ordered in increasing precision
const int perf_skid_precision[] = {
  PERF_EVENT_SKID_ARBITRARY,
  PERF_EVENT_SKID_CONSTANT,
  PERF_EVENT_SKID_ZERO_REQUESTED,
  PERF_EVENT_SKID_ZERO_REQUIRED
};

const int perf_skid_flavors = sizeof(perf_skid_precision)/sizeof(int);

// -----------------------------------------------------
// private methods
// -----------------------------------------------------
static long
perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
         int cpu, int group_fd, unsigned long flags)
{
   int ret;

   ret = syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
   return ret;
}


// -----------------------------------------------------
// API
// -----------------------------------------------------

/**
 * set precise_ip attribute to the max value
 *
 * TODO: this method only works on some platforms, and not
 *       general enough on all the platforms.
 */
static void
perf_skid_set_max_precise_ip(struct perf_event_attr *attr)
{
  // start with the most restrict skid (3) then 2, 1 and 0
  // this is specified in perf_event_open man page
  // if there's a change in the specification, we need to change
  // this one too (unfortunately)
  for(int i=perf_skid_flavors-1; i>=0; i--) {
	attr->precise_ip = perf_skid_precision[i];

	// ask sys to "create" the event
	// it returns -1 if it fails.
	int ret = perf_event_open(attr,
			THREAD_SELF, CPU_ANY,
			GROUP_FD, PERF_FLAGS);
	if (ret >= 0) {
	  close(ret);
	  // just quit when the returned value is correct
	  return;
	}
  }
}


/*
 * get int long value of variable environment.
 * If the variable is not set, return the default value 
 */
static long
getEnvLong(const char *env_var, long default_value)
{
  const char *str_val= getenv(env_var);

  if (str_val) {
    char *end_ptr;
    long val = strtol( str_val, &end_ptr, 10 );
    if ( end_ptr != env_var && (val < LONG_MAX && val > LONG_MIN) ) {
      return val;
    }
  }
  // invalid value
  return default_value;
}


char* 
strlaststr(const char* haystack, const char* needle)
{
   char*  loc = 0;
   char*  found = 0;
   size_t pos = 0;

   while ((found = strstr(haystack + pos, needle)) != 0)
   {
      loc = found;
      pos = (found - haystack) + 1;
   }

   return loc;
}

//===================================================================
// INTERFACES
//===================================================================

//----------------------------------------------------------
// find the best precise ip value in this platform
// @param current perf event attribute. This attribute can be
//    updated for the default precise ip.
// @return the assigned precise ip 
//----------------------------------------------------------
u64
perf_skid_get_precise_ip(struct perf_event_attr *attr)
{
  // check if user wants a specific ip-precision
  int val = getEnvLong(HPCRUN_OPTION_PRECISE_IP, PERF_EVENT_AUTODETECT_SKID);
  if (val >= PERF_EVENT_SKID_ARBITRARY && val <= PERF_EVENT_SKID_ZERO_REQUIRED)
  {
    attr->precise_ip = val;

    // check the validity of the requested precision
    // if it returns -1 we need to use our own auto-detect precision
    int ret = perf_event_open(attr,
            THREAD_SELF, CPU_ANY,
            GROUP_FD, PERF_FLAGS);
    if (ret >= 0) {
      return val;
    }
  }
  perf_skid_set_max_precise_ip(attr);

  return attr->precise_ip;
}


// parse the event into event_name and the type of precise_ip
//  the name of the event excludes the precise ip suffix
// returns:
//   PRECISE_IP_NONE     : if there is no precise ip
//   PRECISE_IP_DEFAULT  : if precise ip is not specified (use the default)
//   x                   : if a precise_ip value is specified
int
perf_skid_parse_event(const char *event, char *event_name, size_t event_name_size)
{
  int len_suf = strlen(PRECISE_IP_SUFFIX);
  int len_evt = strlen(event);

  if (len_evt <= len_suf) 
    return PRECISE_IP_NONE;

  // continuously looking for ":p" pattern inside the event_name
  // if an event has foo::par:peer:p it should return the last ":p"
  char *ptr_att = strlaststr(event, PRECISE_IP_SUFFIX);

  if (!ptr_att) {
    strcpy(event_name, (const char*)event);
    return PRECISE_IP_NONE;
  }
  memcpy(event_name, event, ptr_att-event);

  char *ptr_next = ptr_att + len_suf;
  if (!ptr_next)
    // shouldn't happen here
    return PRECISE_IP_NONE;

  if (*ptr_next == '\0') {
    return PRECISE_IP_DEFAULT;
  }

  if (ptr_next && *ptr_next == '=') {
    ptr_next++;

    if (ptr_next && isdigit(*ptr_next)) {
      u64 precise_ip = *ptr_next - '0';

      ptr_next++;
      if (*ptr_next == DELIMITER_PERIOD) {
        strcat(event_name, ptr_next);
      }
      return precise_ip;
    }

    // precise_ip suffix is present but incorrect value
    // should we fail or return to default ?
    return PRECISE_IP_DEFAULT;
  }

  if (*ptr_next == DELIMITER_PERIOD) {
    strcat(event_name, ptr_next);
  }

  return PRECISE_IP_NONE;
}

#if     _PERF_UTIL_DEBUG_
#include <stdio.h>

#define MAX_NAME_EVENT 128
int 
main (int argc, char *argv[])
{
  char *ev[] = {"cycles:p", "cycles:p=3", "cycles", "cycles::popo:peer", 
	  	"cycles::popo:oeer:p", "cycles::popo:peer:p=f", "cycles@100", 
	  	"cycles::popo:oeer:p@100", "cycles::popo:peer:p=f@f10",
 		"cycles::popo:peer:p=1@5050", "cycles::popo:peer:p=2" };
  char name_event[MAX_NAME_EVENT];
  int num_events = sizeof(ev)/sizeof(ev[9]);
  int i;

  for (i=0; i<num_events; i++) {
    memset(name_event, 0, MAX_NAME_EVENT);
    int r = perf_skid_parse_event(ev[i], name_event, MAX_NAME_EVENT);
    printf("event: %s  name: %s   -> %d\n", ev[i], name_event,  r);
  }
}

#endif
