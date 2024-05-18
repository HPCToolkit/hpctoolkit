// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef CSPROF_OPTIONS_H
#define CSPROF_OPTIONS_H
#include <limits.h>

#include "sample-sources/sample_source_obj.h"
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
