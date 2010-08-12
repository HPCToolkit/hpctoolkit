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
// Copyright ((c)) 2002-2010, Rice University 
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
// The sample sources data structure
//

//*******************************************************************
// System includes
//*******************************************************************

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

//*******************************************************************
// Local Includes
//*******************************************************************

#include "sample_sources_all.h"
#include "sample_sources_registered.h"

#include "thread_data.h"
#include <sample-sources/simple_oo.h>
#include <sample-sources/sample_source_obj.h>
#include <sample-sources/common.h>
#include <utilities/tokenize.h>
#include <messages/messages.h>

//*******************************************************************
// Function Defining Macros
//*******************************************************************

#define _AS0(n) \
void                                                            \
hpcrun_all_sources_ ##n(void)                                   \
{								\
  for(int i=0;i < n_sources;i++) {				\
    TMSG(AS_MAP,"sample source %d (%s) method call: %s",i,	\
	 sample_sources[i]->name,#n);				\
    METHOD_CALL(sample_sources[i],n);				\
  }								\
}

#define _AS1(n,t,arg) \
void                                                            \
hpcrun_all_sources_ ##n(t arg)                                  \
{								\
  for(int i=0;i < n_sources;i++) {				\
    METHOD_CALL(sample_sources[i],n,arg);			\
  }								\
}

#define _ASB(n)							\
bool								\
hpcrun_all_sources_ ##n(void)					\
{								\
  NMSG(AS_ ##n,"checking %d sources",n_sources);		\
  for(int i=0;i < n_sources;i++) {				\
    if (! METHOD_CALL(sample_sources[i],n)) {			\
      NMSG(AS_ ##n,"%s not started",sample_sources[i]->name);	\
      return false;						\
    }								\
  }								\
  return true;							\
}

//
// END Function Defining Macros
//

//*******************************************************************
// Local variables
//*******************************************************************

static sample_source_t* sample_sources[MAX_SAMPLE_SOURCES];
static int n_sources = 0;
static int n_hardware_sources;

//*******************************************************************
// Interface functions
//*******************************************************************


sample_source_t *
hpcrun_fetch_source_by_name(const char *src)
{
  for(int i=0; i < n_sources; i++){
    if (strcmp(sample_sources[i]->name, src) == 0) {
      return sample_sources[i];
    }
  }
  return NULL;
}

bool
hpcrun_check_named_source(const char *src)
{
  for(int i=0; i < n_sources; i++){
    if (strcmp(sample_sources[i]->name, src) == 0) {
      return true;
    }
  }
  return false;
}

static int
in_sources(sample_source_t* ss)
{
  for(int i=0; i < n_sources; i++){
    if (sample_sources[i] == ss)
      return 1;
  }
  return 0;
}


static void
add_source(sample_source_t* ss)
{
  NMSG(AS_add_source,"%s",ss->name);
  if (n_sources == MAX_SAMPLE_SOURCES){
    // check to see is ss already present
    if (! in_sources(ss)){
      hpcrun_abort("Too many total (hardware + software) sample sources");
    }
    return;
  }
  if (ss->cls == SS_HARDWARE && n_hardware_sources == MAX_HARDWARE_SAMPLE_SOURCES) {
    if (! in_sources(ss)) {
      hpcrun_abort("Too many hardware sample sources");
    }
    return;
  }
  sample_sources[n_sources] = ss;
  n_sources++;
  if (ss->cls == SS_HARDWARE) {
    n_hardware_sources++;
  }
  NMSG(AS_add_source,"# sources now = %d",n_sources);
}


void
hpcrun_sample_sources_from_eventlist(char* evl)
{
  if (evl == NULL) {
    hpcrun_ssfail_none();
  }

  TMSG(EVENTS,"evl (before processing) = |%s|",evl);

  for(char *event = start_tok(evl); more_tok(); event = next_tok()){
    sample_source_t *s;
    if (strcasecmp(event, "LIST") == 0) {
      hpcrun_display_avail_events();
    }
    else if ( (s = hpcrun_source_can_process(event)) ){
      add_source(s);
      METHOD_CALL(s,add_event,event);
    }
    else {
      hpcrun_ssfail_unknown(event);
    }
  }
}

// The mapped operations

_AS1(process_event_list,int,lush_metrics)
_AS0(init)
_AS1(gen_event_set,int,lush_metrics)
_AS0(start)
_AS0(stop)
_AS0(shutdown)
_ASB(started)
