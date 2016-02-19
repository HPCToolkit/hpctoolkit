// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
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

#ifndef CSPROF_OPTIONS_H
#define CSPROF_OPTIONS_H
#include <limits.h>

#include <sample-sources/sample_source_obj.h>
#include "event_info.h"

#define CSPROF_PATH_SZ (PATH_MAX+1) /* path size */

/* represents options for the library */
typedef struct hpcrun_options_s {
  char lush_agent_paths[CSPROF_PATH_SZ]; /* paths for LUSH agents */

  char out_path[CSPROF_PATH_SZ]; /* path for output */
  char addr_file[CSPROF_PATH_SZ]; /* path for "bad address" file */

  event_info evi;

  //  unsigned int max_metrics;

} hpcrun_options_t;

#define CSPROF_OUT_PATH          "."
#define CSPROF_EVENT       "microseconds"
#define CSPROF_SMPL_PERIOD 1000UL /* microseconds */
// #define CSPROF_MEM_SZ_INIT       32 * 1024 * 1024 /* FIXME: 1024 */
#define CSPROF_MEM_SZ_INIT       128 // small for mem stress test
#define CSPROF_MEM_SZ_DEFAULT  CSPROF_MEM_SZ_INIT

int hpcrun_options__init(hpcrun_options_t* x);
int hpcrun_options__fini(hpcrun_options_t* x);
int hpcrun_options__getopts(hpcrun_options_t* x);

#endif
