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

#include <string.h>

#include "sample_source.h"

#include <messages/messages.h>

int
METHOD_FN(csprof_ss_started)
{
  NMSG(SS_COMMON,"check start for %s = %d",self->name,self->state);
  return (self->state == START);
}

void
METHOD_FN(csprof_ss_add_event,const char *ev)
{
  char *evl = self->evl.evl_spec;

  NMSG(SS_COMMON,"add event %s to evl |%s|",ev,evl);
  strcat(evl,ev);
  strcat(evl," ");
  NMSG(SS_COMMON,"evl after event added = |%s|",evl);
}

void
METHOD_FN(csprof_ss_store_event,int event_id,long thresh)
{
  evlist_t *_p       = &(self->evl);
  int *ev  = &(_p->nevents);
  if (*ev >= MAX_EVENTS) {
    EMSG("Too many events entered for sample source. Event code %d ignored",event_id);
    return;
  }
  _ev_t *current_event  = &(_p->events[*ev]);

  current_event->event  = event_id;
  current_event->thresh = thresh;

  (*ev)++;
}

char *
METHOD_FN(csprof_ss_get_event_str)
{
  return (self->evl).evl_spec;
}

void
METHOD_FN(csprof_ss_start)
{
  if (self->state != HARD_STOP){
    METHOD_CALL(self,_start);
  }
}

void
METHOD_FN(csprof_ss_hard_stop)
{
  METHOD_CALL(self,stop);
  self->state = HARD_STOP;
}
