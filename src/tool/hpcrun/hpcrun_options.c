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

//
//

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

#include <messages/messages.h>

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
