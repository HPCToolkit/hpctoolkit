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
// Copyright ((c)) 2002-2019, Rice University
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

#ifdef _PERF_SKID_DEBUG_
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#endif

#include <string.h>

#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

#include <linux/perf_event.h>

#include "perf_constants.h"
#include "perf_skid.h"
#include "perf_event_open.h"

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

#define PRECISE_IP_CHAR_MODIFIER 'p'

#define PRECISE_IP_SUFFIX   	 ":p"
#define PRECISE_IP_MAX_SUFFIX    ":P"

#define DELIMITER_HOWOFTEN    '@'

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


static char* 
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


static const char *
find_precise_suffix(const char *s, const char *suffix, char allowed)
{
  char *ptr_att = strlaststr(s, suffix);
  if (ptr_att) {
    const char *end =  ptr_att + strlen(suffix);

    // skip allowable characters after suffix
    if (allowed) {
      if (*end == allowed) end++; 
      if (*end == allowed) end++; 
    }

    // check that either 
    // (1) there is nothing left, or 
    // (2) it is followed by DELIMITER_HOWOFTEN
    if (*end != 0 && *end != DELIMITER_HOWOFTEN) ptr_att = 0;
  }
  return ptr_att;
}



//===================================================================
// INTERFACES
//===================================================================

/**
 * set precise_ip attribute to the max value
 *
 * TODO: this method only works on some platforms, and not
 *       general enough on all the platforms.
 */
int
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
	  return attr->precise_ip;
	}
  }
  return 0;
}


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
      close(ret);
      return val;
    }
  }
  // no variable is set or the value is not valid
  // set to the most arbitrary skid to ensure it works
  //
  attr->precise_ip = PERF_EVENT_SKID_ARBITRARY;
  
  return attr->precise_ip;
}

// parse the event into event_name and the type of precise_ip
//  the name of the event excludes the precise ip suffix
// returns:
//  4 PERF_EVENT_AUTODETECT_SKID       
//  3 PERF_EVENT_SKID_ZERO_REQUIRED    
//  2 PERF_EVENT_SKID_ZERO_REQUESTED  
//  1 PERF_EVENT_SKID_CONSTANT         
//  0 PERF_EVENT_SKID_ARBITRARY        
//  -1 PERF_EVENT_SKID_ERROR   
int
perf_skid_parse_event(const char *event_string, char **event_string_without_skidmarks)
{
  int len_suf = strlen(PRECISE_IP_SUFFIX);
  int len_evt = strlen(event_string);
  int precise = 0;

  if (len_evt <= len_suf) {
    // some events consist only of two letters (e.g,: cs)
    // Using this event (which has two letters) is not an error,
    // we just doesn't need to parse it.
    *event_string_without_skidmarks = strdup(event_string);
    return PERF_EVENT_SKID_ARBITRARY;
  }

  const char *ptr_att = find_precise_suffix(event_string, PRECISE_IP_MAX_SUFFIX, 0);
  if (ptr_att) {
    char buffer[1024];
#ifdef _PERF_SKID_DEBUG_
    memset(buffer,1, sizeof(buffer));
#endif
    memcpy(buffer, event_string, ptr_att - event_string);
    buffer[ptr_att - event_string] = 0;
    strcat(buffer, ptr_att + strlen(PRECISE_IP_MAX_SUFFIX));
    *event_string_without_skidmarks = strdup(buffer);
    return PERF_EVENT_AUTODETECT_SKID;
  }

  // continuously looking for ":p" pattern inside the event_name
  // if an event has foo::par:peer:p it should return the last ":p"
  ptr_att = find_precise_suffix(event_string, PRECISE_IP_SUFFIX, PRECISE_IP_CHAR_MODIFIER);

  if (!ptr_att) {
    *event_string_without_skidmarks = strdup(event_string);
  } else {
    char buffer[1024];
#ifdef _PERF_SKID_DEBUG_
    memset(buffer,1, sizeof(buffer));
#endif
    memcpy(buffer, event_string, ptr_att - event_string);
    buffer[ptr_att - event_string] = 0;
    precise++;
    
    const char *ptr_next = ptr_att + len_suf;
    if (!ptr_next)
      // shouldn't happen here
      return precise;

    // count the number of p in :ppp
    while (ptr_next && *ptr_next == PRECISE_IP_CHAR_MODIFIER) {
      ptr_next++;
      precise++;
    }

    if (*ptr_next == DELIMITER_HOWOFTEN) {
      // next char is period threshold or frequency
      strcat(buffer, ptr_next);
    } else if (*ptr_next != '\0') {
      // the next char is not recognized
      precise--;
    }

    *event_string_without_skidmarks = strdup(buffer);
  }

  return precise;
}

#ifdef _PERF_SKID_DEBUG_
#include <stdio.h>

#define MAX_NAME_EVENT 128
int 
main (int argc, char *argv[])
{
  char *ev[] = {"cycles:p", "cycles:pp", "cycles", "cycles::popo:peer", 
	  	"cycles::popo:oeer:ppp", "cycles::popo:peer:p", "cycles@100", 
	  	"cycles::popo:oeer:p@100", "cycles::popo:peer:p@f10",
 		"cycles::popo:peer:ppp@5050", "cycles::popo:peer:pp",
 		"cs:P", "cs:Pp", "cs:pppp", "cs:P@10000" };
  int num_events = sizeof(ev)/sizeof(ev[9]);
  int i;

  for (i=0; i<num_events; i++) {
    char *name_event;
    int r = perf_skid_parse_event(ev[i], &name_event);
    printf("event: %s  name: %s   -> %d\n", ev[i], name_event,  r);
  }
}

#endif
