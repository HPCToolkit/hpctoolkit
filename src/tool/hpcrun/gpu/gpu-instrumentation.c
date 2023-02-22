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
// Copyright ((c)) 2002-2023, Rice University
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

//******************************************************************************
// system includes
//******************************************************************************

#include <string.h>
#include <stdlib.h>



//******************************************************************************
// local includes
//******************************************************************************

#include <hpcrun/messages/messages.h>

#include "gpu-instrumentation.h"



//******************************************************************************
// macros
//******************************************************************************

#define DEBUG 0

#define INST_PREFIX   "inst"
#define INST_COUNT    "count"
#define INST_LATENCY  "latency"
#define INST_SIMD     "simd"

#define ENABLE_SIMD_ANALYSIS 0
#define ENABLE_LATENCY_ANALYSIS 0



//******************************************************************************
// local data
//******************************************************************************

#ifdef ENABLE_GTPIN
static const char *delimiter = ",";
#endif


//******************************************************************************
// interface functions
//******************************************************************************

void
gpu_instrumentation_options_set
(
 const char *str,
 const char *prefix,
 gpu_instrumentation_t *options
)
{
  memset(options, 0, sizeof(gpu_instrumentation_t));

  char *opt = strdup(str); // writable copy of str
  char *ostr = 0;

  int len = strlen(opt);
  int match_len = strlen(prefix);

  if (len > match_len) {
    ostr = opt + match_len;

#ifdef ENABLE_GTPIN
    // match comma separator
    if (*ostr != ',') {
      fprintf(stderr, "hpcrun ERROR: while parsing GPU instrumentation knobs, expected ',' separator but found '%s'\n", ostr);
      exit(-1);
    }
    ostr++;

    // match instrumentation prefix
    match_len = strlen(INST_PREFIX);
    if (strncmp(ostr, INST_PREFIX, match_len) != 0) {
      fprintf(stderr, "hpcrun ERROR: while parsing GPU instrumentation knobs, expected 'inst' but found '%s'\n", ostr);
      exit(-1);
    }
    ostr += match_len;

    switch(*ostr) {
    case 0:
      // default instrumentation
      options->count_instructions = true;
      break;

    case '=':
      ostr++;
      char *token = strtok(ostr, delimiter);

      // analyze options
      while(token) {
	if (strcmp(token, INST_COUNT) == 0) {
	  options->count_instructions = true;
  #if ENABLE_LATENCY_ANALYSIS
	} else if (strcmp(token, INST_LATENCY) == 0) {
	  options->attribute_latency = true;
  #endif
  #if ENABLE_SIMD_ANALYSIS
	} else if (strcmp(token, INST_SIMD) == 0) {
	  options->analyze_simd = true;
  #endif
	} else {
	  fprintf(stderr, "hpcrun ERROR: while parsing GPU instrumentation knobs, unrecognized knob '%s'\n", token);
	  exit(-1);
	}
	token = strtok(NULL, delimiter);
      }
      break;

    default:
      fprintf(stderr, "hpcrun ERROR: unexpected text encountered parsing GPU instrumentation knobs '%s'\n", ostr);
      exit(-1);
    }
#else
    if (*ostr) {
      fprintf(stderr, "hpcrun ERROR: unexpected text encountered parsing GPU"
	      " setting '%s'\n", ostr);
      exit(-1);
    }
#endif
  }

#if DEBUG
printf("gpu instrumentation options  : %s\n", opt);
  printf("\toptions.count_instructions : %d\n", options->count_instructions);
  printf("\toptions.attribute_latency  : %d\n", options->attribute_latency);
  printf("\toptions.analyze_simd       : %d\n", options->analyze_simd);
#endif

  free(opt);

  // consistency check
  if (options->attribute_latency) {
    if (options->analyze_simd) {
      fprintf(stderr, "hpcrun WARNING: unwise to analyze GPU SIMD instructions while attributing GPU instruction latency\n");
    }
  }
}


bool
gpu_instrumentation_enabled
(
 gpu_instrumentation_t *o
)
{
  return o->count_instructions | o->analyze_simd | o-> attribute_latency;
}
