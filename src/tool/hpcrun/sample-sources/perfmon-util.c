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

/******************************************************************************
 * libmonitor
 *****************************************************************************/
#include <perfmon/err.h>
#include <perfmon/pfmlib.h>


//******************************************************************************
// local variables
//******************************************************************************

static const char *srcs[PFM_ATTR_CTRL_MAX]={
	[PFM_ATTR_CTRL_UNKNOWN] = "???",
	[PFM_ATTR_CTRL_PMU] = "PMU",
	[PFM_ATTR_CTRL_PERF_EVENT] = "perf_event",
};


//******************************************************************************
// local operations
//******************************************************************************

static void
print_attr_flags(pfm_event_attr_info_t *info)
{
	int n = 0;

	if (info->is_dfl) {
		printf("[default] ");
		n++;
	}

	if (info->is_precise) {
		printf("[precise] ");
		n++;
	}

	if (!n)
		printf("None ");
}


static int
event_has_pname(char *s)
{
	char *p;
	return (p = strchr(s, ':')) && *(p+1) == ':';
}

static void
print_event_flags(pfm_event_info_t *info)
{
	int n = 0;

	if (info->is_precise) {
		printf("[precise] ");
		n++;
	}
	if (!n)
		printf("None");
}

const int MAXBUF = 1024;

static void
show_event_info(pfm_event_info_t *info)
{
	pfm_event_attr_info_t ainfo;
	pfm_pmu_info_t pinfo;
	int mod = 0, um = 0;
	int i, ret;
	const char *src;
	char buffer[MAXBUF];	

	memset(&ainfo, 0, sizeof(ainfo));
	memset(&pinfo, 0, sizeof(pinfo));

	pinfo.size = sizeof(pinfo);
	ainfo.size = sizeof(ainfo);

	ret = pfm_get_pmu_info(info->pmu, &pinfo);
	if (ret)
		errx(1, "cannot get pmu info: %s", pfm_strerror(ret));

	sprintf(buffer, "%s::%s", pinfo.name, info->name);
	int lpname = strlen(buffer);
	if (lpname >50)
		lpname = 80-lpname;
	else
		lpname = 50-lpname;

  	printf("%s %*s %s\n", buffer, lpname, " ", info->desc ? info->desc : "");

	pfm_for_each_event_attr(i, info) {
		ret = pfm_get_event_attr_info(info->idx, i, PFM_OS_NONE, &ainfo);
		if (ret != PFM_SUCCESS)
			errx(1, "cannot retrieve event %s attribute info: %s", info->name, pfm_strerror(ret));
	  	printf("%s:%s  %s %s\n", buffer, ainfo.name, info->desc ? info->desc : "", ainfo.desc);
	}
#if 0
	printf("#-----------------------------\n"
	       "IDX	 : %d\n"
	       "PMU name : %s (%s)\n"
	       "Name     : %s\n"
	       "Equiv	 : %s\n",
		info->idx,
		pinfo.name,
		pinfo.desc,
		info->name,
		info->equiv ? info->equiv : "None");

	printf("Flags    : ");
	print_event_flags(info);
	putchar('\n');

	printf("Desc     : %s\n", info->desc ? info->desc : "no description available");
	printf("Code     : 0x%"PRIx64"\n", info->code);

	pfm_for_each_event_attr(i, info) {
		ret = pfm_get_event_attr_info(info->idx, i, PFM_OS_NONE, &ainfo);
		if (ret != PFM_SUCCESS)
			errx(1, "cannot retrieve event %s attribute info: %s", info->name, pfm_strerror(ret));

		if (ainfo.ctrl >= PFM_ATTR_CTRL_MAX) {
			warnx("event: %s has unsupported attribute source %d", info->name, ainfo.ctrl);
			ainfo.ctrl = PFM_ATTR_CTRL_UNKNOWN;
		}
		src = srcs[ainfo.ctrl];
		switch(ainfo.type) {
		case PFM_ATTR_UMASK:

			printf("Umask-%02u : 0x%02"PRIx64" : %s : [%s] : ",
				um,
				ainfo.code,
				src,
				ainfo.name);

			print_attr_flags(&ainfo);

			putchar(':');

			if (ainfo.equiv)
				printf(" Alias to %s", ainfo.equiv);
			else
				printf(" %s", ainfo.desc);

			putchar('\n');
			um++;
			break;
		case PFM_ATTR_MOD_BOOL:
			printf("Modif-%02u : 0x%02"PRIx64" : %s : [%s] : %s (boolean)\n", mod, ainfo.code, src, ainfo.name, ainfo.desc);
			mod++;
			break;
		case PFM_ATTR_MOD_INTEGER:
			printf("Modif-%02u : 0x%02"PRIx64" : %s : [%s] : %s (integer)\n", mod, ainfo.code, src, ainfo.name, ainfo.desc);
			mod++;
			break;
		default:
			printf("Attr-%02u  : 0x%02"PRIx64" : %s : [%s] : %s\n", i, ainfo.code, ainfo.name, src, ainfo.desc);
		}
	}
#endif
}

static int
show_info(char *event )
{
   pfm_pmu_info_t pinfo;
   pfm_event_info_t info;
   int i, j, ret, match = 0, pname;
   size_t len, l = 0;

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

/*
 * interface to get the info of an event (if matched)
 */


/*
 * interface function to print the list of supported PMUs
 */
int
getEvent()
{
   static char *argv_all =  ".*";
   /* to allow encoding of events from non detected PMU models */
   int ret = setenv("LIBPFM_ENCODE_INACTIVE", "1", 1);
   if (ret != PFM_SUCCESS)
      errx(1, "cannot force inactive encoding");


   ret = pfm_initialize();
   if (ret != PFM_SUCCESS)
      errx(1, "cannot initialize libpfm: %s", pfm_strerror(ret));
             
   int total_supported_events = 0;
   int total_available_events = 0;
   int i;
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

   show_info(argv_all);

   return 0;
}

