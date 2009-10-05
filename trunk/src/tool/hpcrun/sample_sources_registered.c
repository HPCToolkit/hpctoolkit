// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sample_sources_registered.h"
#include "sample_source.h"

#include <messages/messages.h>

static sample_source_t *registered_sample_sources[MAX_POSSIBLE_SAMPLE_SOURCES];

static int nregs = 0;

void
csprof_ss_register(sample_source_t *src)
{
  if (nregs >= MAX_POSSIBLE_SAMPLE_SOURCES){
    EMSG("Sample source named %s NOT registered due to # sample sources exceeded",src->name);
    return;
  }
  registered_sample_sources[nregs++] = src;
}


sample_source_t*
csprof_source_can_process(char *event)
{
  for (int i=0;i < nregs;i++){
    if (METHOD_CALL(registered_sample_sources[i],supports_event,event)){
      return registered_sample_sources[i];
    }
  }
  return NULL;
}

void
csprof_registered_sources_init(void)
{
  for (int i=0;i<nregs;i++){
    METHOD_CALL(registered_sample_sources[i],init);
    NMSG(SS_COMMON,"sample source \"%s\": init",registered_sample_sources[i]->name);
  }
}

void
hpcrun_display_avail_events(void)
{
  // Special hack to put WALLCLOCK first.
  for (int i = 0; i < nregs; i++) {
    if (strcasecmp(registered_sample_sources[i]->name, "itimer") == 0) {
      METHOD_CALL(registered_sample_sources[i], display_events);
    }
  }
  for (int i = 0; i < nregs; i++) {
    if (strcasecmp(registered_sample_sources[i]->name, "itimer") != 0) {
      METHOD_CALL(registered_sample_sources[i], display_events);
    }
  }
  exit(0);
}
