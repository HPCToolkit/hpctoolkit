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

// Process the event list (a string with substrings of form EVENT:PERIOD separated by spaces)
//   each EVENT:PERIOD substring specifies an event
//
// In particular, the sample source(s) are selected here

// System includes 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Local includes
#include "csprof_options.h"
#include "event_info.h"
#include "tokenize.h"

#include <messages/messages.h>

#define EVENT_PERIOD_SEPARATOR '@'

void
csprof_process_event_list(const char *evl,event_info *info)
{
  // for now, just look thru the list for wallclock, to set sample period
  // and check to make sure that both wallclock and papi events are not
  // both set
  // FIXME: error checking code needs to be more general
  
  int use_wallclock    = 0;
  int use_papi         = 0;
  unsigned long period = 0L;

  TMSG(EVENTS,"evl (before processing) = |%s|",evl);

  // Option: debug print -- not handled here !!
  for(char *event = start_tok(evl); more_tok(); event = next_tok()){
    use_wallclock = (strstr(event,"WALLCLOCK") != NULL);
    if (use_wallclock){
      char *_p = strchr(event, EVENT_PERIOD_SEPARATOR);
      if (! _p){
        csprof_abort("Syntax for WALLCLOCK event is: WALLCLOCK@_Your_Period_Here_");
      }
      period = strtol(_p+1,NULL,10);
      info->sample_period = period;
      TMSG(OPTIONS,"WALLCLOCK period set to %ld",period);
    }
    use_papi = use_papi || ! use_wallclock;
    if (use_wallclock && use_papi) {
      csprof_abort("Sampling with both WALLCLOCK and PAPI events in the same execution is forbidden");
    }
  }
  info->sample_source = use_wallclock ? ITIMER : PAPI;
  info->event_list = evl;
  TMSG(EVENTS,"options event list(AFTER processing) = |%s|",info->event_list);
}
