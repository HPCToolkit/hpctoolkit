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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sample_sources_registered.h"
#include <sample-sources/sample_source_obj.h>

#include <messages/messages.h>

static sample_source_t* registered_sample_sources         = NULL;
static sample_source_t** registered_sample_sources_insert = &registered_sample_sources;

void
hpcrun_ss_register(sample_source_t* src)
{
  *registered_sample_sources_insert  = src;
  src->next_reg                      = NULL;
  registered_sample_sources_insert   = &(src->next_reg);
}


sample_source_t*
hpcrun_source_can_process(char *event)
{
  for (sample_source_t* ss=registered_sample_sources; ss; ss = ss->next_reg){
    if (METHOD_CALL(ss, supports_event, event)){
      return ss;
    }
  }
  return NULL;
}


// hpcrun_registered_sources_init(): This routine initializes all
// samples sources.  It is necessary because a sample source, even if
// unused, may need to initialized to list available events.
// Cf. SAMPLE_SOURCES(init), which only initializes used sample
// sources.
void
hpcrun_registered_sources_init(void)
{
  for (sample_source_t* ss=registered_sample_sources; ss; ss = ss->next_reg){
    METHOD_CALL(ss, init);
    TMSG(SS_COMMON, "sample source \"%s\": init", ss->name);
  }
}

void
hpcrun_display_avail_events(void)
{
  // Special hack to put WALLCLOCK first.
  for (sample_source_t* ss=registered_sample_sources; ss; ss = ss->next_reg){
    if (strcasecmp(ss->name, "itimer") == 0) {
      METHOD_CALL(ss, display_events);
    }
  }
  for (sample_source_t* ss=registered_sample_sources; ss; ss = ss->next_reg){
    if (strcasecmp(ss->name, "itimer") != 0) {
      METHOD_CALL(ss, display_events);
    }
  }
  exit(0);
}
