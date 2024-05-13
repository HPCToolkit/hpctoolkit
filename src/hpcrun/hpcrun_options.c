// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//
//

#define _GNU_SOURCE

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hpcrun_options.h"
#include "files.h"
#include "hpcrun_return_codes.h"
#include "env.h"
#include "sample_sources_all.h"
#include "sample_sources_registered.h"

#include "messages/messages.h"

/* option handling */
/* FIXME: this needs to be split up a little bit for different backends */

int
hpcrun_options__init(hpcrun_options_t *x)
{
  TMSG(OPTIONS,"__init");
  memset(x, 0, sizeof(*x));
  return HPCRUN_OK;
}

int
hpcrun_options__fini(hpcrun_options_t* x)
{
  return HPCRUN_OK;
}

/* assumes no private 'heap' memory is available yet */
int
hpcrun_options__getopts(hpcrun_options_t* x)
{
  /* Option: HPCRUN_OPT_LUSH_AGENTS */
  char *s = getenv(HPCRUN_OPT_LUSH_AGENTS);
  if (s) {
    strcpy(x->lush_agent_paths, s);
  }
  else {
    x->lush_agent_paths[0] = '\0';
  }

  TMSG(OPTIONS,"--at end of getopts");

  return HPCRUN_OK;
}
