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
// Copyright ((c)) 2002-2020, Rice University
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

#include <stdbool.h>
#include <stdlib.h>

// Names for option environment variables
const char* HPCRUN_OPT_LUSH_AGENTS = "HPCRUN_OPT_LUSH_AGENTS";

const char* HPCRUN_OUT_PATH        = "HPCRUN_OUT_PATH";
const char* HPCRUN_TRACE           = "HPCRUN_TRACE";

const char* PAPI_EVENT_LIST        = "PAPI_EVENT_LIST";

const char* HPCRUN_EVENT_LIST      = "HPCRUN_EVENT_LIST";
const char* HPCRUN_MEMSIZE         = "HPCRUN_MEMSIZE";
const char* HPCRUN_LOW_MEMSIZE     = "HPCRUN_LOW_MEMSIZE";

//
// Returns: true if 'name' is in the environment and set to a true
// (non-zero) value.
//
bool
hpcrun_get_env_bool(const char *name)
{
  if (name == NULL) { return false; }

  char * str = getenv(name);

  // not in environment
  if (str == NULL) { return false; }

  return (atoi(str) != 0);
}
