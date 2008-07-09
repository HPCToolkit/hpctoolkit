// -*-Mode: C;-*-
// $Id: env.h 1480 2008-06-23 15:33:41Z johnmc $

/*
  Copyright ((c)) 2002, Rice University 
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  * Neither the name of Rice University (RICE) nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  This software is provided by RICE and contributors "as is" and any
  express or implied warranties, including, but not limited to, the
  implied warranties of merchantability and fitness for a particular
  purpose are disclaimed. In no event shall RICE or contributors be
  liable for any direct, indirect, incidental, special, exemplary, or
  consequential damages (including, but not limited to, procurement of
  substitute goods or services; loss of use, data, or profits; or
  business interruption) however caused and on any theory of liability,
  whether in contract, strict liability, or tort (including negligence
  or otherwise) arising in any way out of the use of this software, even
  if advised of the possibility of such damage.
*/
#ifndef CSPROF_ENV_H
#define CSPROF_ENV_H

extern const char* CSPROF_FNM_PFX;
extern const char* CSPROF_PSTATE_FNM_SFX;

extern const char* CSPROF_PROFILE_FNM_SFX;
extern const char* CSPROF_TRACE_FNM_SFX;
extern const char* CSPROF_LOG_FNM_SFX;

extern const char* CSPROF_OPT_LUSH_AGENTS;

/* Names for option environment variables */
extern const char* CSPROF_OPT_OUT_PATH;
extern const char* CSPROF_OPT_SAMPLE_PERIOD;
extern const char* CSPROF_OPT_MEM_SZ;
extern const char* CSPROF_OPT_TRACE;

extern const char* CSPROF_OPT_VERBOSITY;
extern const char* CSPROF_OPT_DEBUG;

extern const char* CSPROF_OPT_MAX_METRICS;

extern const char* SWITCH_TO_PAPI;

#ifdef CSPROF_PAPI
extern const char* CSPROF_OPT_EVENT;
#endif

#endif /* CSPROF_ENV_H */
