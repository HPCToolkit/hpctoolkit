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
// Copyright ((c)) 2002-2016, Rice University
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

#include <linux/perf_event.h>

/******************************************************************************
 * libmonitor
 *****************************************************************************/
#include <perfmon/err.h>
#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>


//******************************************************************************
// Constants
//******************************************************************************

const int MAXBUF   		     = 1024;
const int MAX_CHARS_PER_LINE 	     = 74;
static const char * dashes_separator = 
  "---------------------------------------------------------------------------\n";

//******************************************************************************
// local variables
//******************************************************************************

#include "line_wrapping.h"


//******************************************************************************
// local operations
//******************************************************************************



static int
event_has_pname(char *s)
{
	char *p;
	return (p = strchr(s, ':')) && *(p+1) == ':';
}

static void printw(const char *desc)
{
  char **line;
  int *len;
  char sdesc[MAX_CHARS_PER_LINE];

  int lines = strwrap(desc, MAX_CHARS_PER_LINE, &line, &len);
  for (int i=0; i<lines; i++) {
    strncpy(sdesc, line[i], len[i]);
    sdesc[len[i]] = '\0';
    printf("\t%s\n", sdesc);
  }
  free (line);
  free (len);
}


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
  if (ret)
	errx(1, "cannot get pmu info: %s", pfm_strerror(ret));

  printf(dashes_separator );
  printf("%s::%s\n", pinfo.name, info->name);

  printw(info->desc); 

  pfm_for_each_event_attr(i, info) {
    ret = pfm_get_event_attr_info(info->idx, i, PFM_OS_NONE, &ainfo);
    if (ret != PFM_SUCCESS)
	errx(1, "cannot retrieve event %s attribute info: %s", info->name, pfm_strerror(ret));
    printf("%s::%s:%s\n", pinfo.name, info->name, ainfo.name); 
    //printw(info->desc);
    printw(ainfo.desc);
  }
}

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
			errx(1, "cannot get event info: %s", pfm_strerror(ret));

		show_event_info(&info);
		match++;
	}
   }
   return match;
}


//******************************************************************************
// Supported operations
//******************************************************************************

int 
pfmu_getEventType(const char *eventname)
{
  pfm_perf_encode_arg_t arg;
  char *fqstr = NULL;

  memset(&arg.attr, 0, sizeof(struct perf_event_attr));

  arg.fstr = &fqstr;
  arg.size = sizeof(pfm_perf_encode_arg_t);
  struct perf_event_attr attr;
  arg.attr = &attr;
  int ret = pfm_get_os_event_encoding(eventname, PFM_PLM0|PFM_PLM3, PFM_OS_PERF_EVENT, &arg);

  if (ret == PFM_SUCCESS) {
    return arg.attr->type;
  }

  free(arg.fstr);
  return -1;
}

/**
 * interface to convert from event name into event code
 *
 * return 1 (true) if the event is supported, 0 (false) otherwise
 **/
int
pfmu_getEventCode(const char *eventname, unsigned int *eventcode)
{

  pfm_pmu_encode_arg_t raw;
  memset(&raw, 0, sizeof(raw));

  int ret = pfm_get_os_event_encoding(eventname, PFM_PLM0|PFM_PLM3, PFM_OS_NONE, &raw);
  int result = (ret == PFM_SUCCESS);
  if (result) {
     if (raw.count>0) {
	// TODO: by default we use the first event code 
       *eventcode = raw.codes[0];
     }
  }
  return result;
}

/*
 * interface to check if an event is "supported"
 * "supported" here means, it matches with the perfmon PMU event 
 *
 * return 1 (true) if the event is supported, 0 (false) otherwise
 */
int
pfmu_isSupported(const char *eventname)
{
  unsigned int eventcode;
  return pfmu_getEventCode(eventname, &eventcode);
}

int
pfmu_getEventAttribute(const char *event_str, struct perf_event_attr *pea)
{
  char *fstr = NULL;
  int ret = 0;

  if (pea) {
    pea->size = sizeof(struct perf_event_attr);
    ret = pfm_get_perf_event_encoding(event_str, PFM_PLM0|PFM_PLM3|PFM_PLMH, pea,
          &fstr, NULL);
  }
  return ret == PFM_SUCCESS;
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
      errx(1, "cannot force inactive encoding");

   ret = pfm_initialize();
   if (ret != PFM_SUCCESS)
      errx(1, "cannot initialize libpfm: %s", pfm_strerror(ret));

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

   printf("Supported PMU models:\n");
   pfm_for_all_pmus(i) {
	ret = pfm_get_pmu_info(i, &pinfo);
	if (ret != PFM_SUCCESS)
		continue;

	printf("\t[%d, %s, \"%s\"]\n", i, pinfo.name,  pinfo.desc);
   }  

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

   printf(dashes_separator);

   show_info(argv_all);

   pfm_terminate();

   return 0;
}

