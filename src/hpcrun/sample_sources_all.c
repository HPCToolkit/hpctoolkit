// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//
// The sample sources data structure
//

//*******************************************************************
// System includes
//*******************************************************************

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

//*******************************************************************
// Local Includes
//*******************************************************************

#include "libmonitor/monitor.h"

#include "sample_sources_all.h"
#include "sample_sources_registered.h"

#include "thread_data.h"
#include "sample-sources/simple_oo.h"
#include "sample-sources/sample_source_obj.h"
#include "sample-sources/common.h"
#include "utilities/tokenize.h"
#include "messages/messages.h"



//*******************************************************************
// Macros
//*******************************************************************

#define THREAD_DOINIT           0
#define THREAD_NOSAMPLING       1
#define THREAD_SAMPLING         2

//*******************************************************************
// Function Defining Macros
//*******************************************************************

#define _AS0(n, necessary)                                           \
void                                                                 \
hpcrun_all_sources_ ##n(void)                                        \
{                                                                    \
 if (necessary) { if (ignore_this_thread()) return; }                \
  TMSG(SS_ALL, "calling function %s", __func__);                     \
  for(sample_source_t* ss = sample_sources; ss; ss = ss->next_sel) { \
    TMSG(SS_ALL,"sample source (%s) method call: %s",                \
         ss->name, #n);                                              \
    METHOD_CALL(ss, n);                                              \
  }                                                                  \
}

#define _AS1(n,t,arg) \
void                                                                 \
hpcrun_all_sources_ ##n(t arg)                                       \
{                                                                    \
  for(sample_source_t* ss = sample_sources; ss; ss = ss->next_sel) { \
    METHOD_CALL(ss, n, arg);                                         \
  }                                                                  \
}

#define _ASB(n)                                                      \
bool                                                                 \
hpcrun_all_sources_ ##n(void)                                        \
{                                                                    \
  TMSG(SS_ALL, "checking %d sources",n_sources);                     \
  for(sample_source_t* ss = sample_sources; ss; ss = ss->next_sel) { \
    if (! METHOD_CALL(ss, n)) {                                      \
      TMSG(SS_ALL, "%s not started",ss->name);                       \
      return false;                                                  \
    }                                                                \
  }                                                                  \
  return true;                                                       \
}

//
// END Function Defining Macros
//



//*******************************************************************
// Local variables
//*******************************************************************

static sample_source_t* sample_sources = NULL;
static sample_source_t** ss_insert     = &sample_sources;
static size_t n_sources = 0;

static __thread int ignore_thread = THREAD_DOINIT;



//*******************************************************************
// private functions
//*******************************************************************

static int
ignore_this_thread()
{
  if (ignore_thread == THREAD_DOINIT) {
    ignore_thread = THREAD_SAMPLING;

    char *string = getenv("HPCRUN_IGNORE_THREAD");
    if (string) {

      // eliminate special cases by adding comma delimiters at front and back
      char all_str[1024];
      sprintf(all_str, ",%s,", string);

      int myid = monitor_get_thread_num();
      char myid_str[20];
      sprintf(myid_str, ",%d,", myid);

      if (strstr(all_str, myid_str)) {
        ignore_thread = THREAD_NOSAMPLING;
        TMSG(IGNORE, "Thread %d ignore sampling", myid);
      }
    }
  }
  return ignore_thread == THREAD_NOSAMPLING;
}

//*******************************************************************
// Interface functions
//*******************************************************************


sample_source_t*
hpcrun_fetch_source_by_name(const char* src)
{
  for (sample_source_t* ss = sample_sources; ss; ss = ss->next_sel){
    if (strcmp(ss->name, src) == 0) {
      return ss;
    }
  }
  return NULL;
}

bool
hpcrun_check_named_source(const char* src)
{
  for (sample_source_t* ss = sample_sources; ss; ss = ss->next_sel){
    if (strcmp(ss->name, src) == 0) {
      return true;
    }
  }
  return false;
}

static bool
in_sources(sample_source_t* ss_in)
{
  for (sample_source_t* ss = sample_sources; ss; ss = ss->next_sel){
    if (ss == ss_in)
      return true;
  }
  return false;
}


static void
add_source(sample_source_t* ss)
{
  TMSG(SS_ALL, "%s", ss->name);
  if (in_sources(ss)) {
    return;
  }
  *ss_insert = ss;
  ss->next_sel = NULL;
  ss_insert    = &(ss->next_sel);
  ss->sel_idx  = n_sources++;
  TMSG(SS_ALL, "Sample source %s has selection index %d", ss->name, ss->sel_idx);
  TMSG(SS_ALL, "# sources now = %d", n_sources);
}


//
// Return the number of -selected- sample sources
//
size_t
hpcrun_get_num_sample_sources(void)
{
  return n_sources;
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
      METHOD_CALL(s, add_event, event);
    }
    else {
      hpcrun_ssfail_unknown(event);
    }
  }
}

// The mapped operations

_AS1(process_event_list, int, lush_metrics)
_AS0(finalize_event_list, 0);
_AS0(init, 0)
_AS0(thread_init, 0)
_AS0(thread_init_action, 0)
_AS1(gen_event_set, int, lush_metrics)
_AS0(start, 1)
_AS0(thread_fini_action, 0)
_AS0(stop, 1)
_AS0(shutdown, 0)
_ASB(started)
