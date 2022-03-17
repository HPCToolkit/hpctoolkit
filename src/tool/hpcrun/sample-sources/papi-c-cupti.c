// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
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

//***************************************************************************
//
// File:
//   cupti-api.c
//
// Purpose:
//   implementation of wrapper around NVIDIA's CUPTI performance tools API
//
//***************************************************************************

//***************************************************************************
// system includes
//***************************************************************************

#include <papi.h>
#include <monitor.h>



//***************************************************************************
// local includes
//***************************************************************************

#include <messages/messages.h>
#include <sample-sources/ss-obj-name.h>
#include "papi-c.h"
#include "papi-c-extended-info.h"



//******************************************************************************
// static data
//******************************************************************************

static __thread bool event_set_created = false;
static __thread bool event_set_finalized = false;
static __thread int my_event_set = PAPI_NULL;



//******************************************************************************
// private operations
//******************************************************************************

static void
papi_c_no_action(void)
{
  ;
}


static bool
is_papi_c_cuda(const char* name)
{
  return strstr(name, "cuda") == name;
}


// Get or create a cupti event set
static void
papi_c_cupti_get_event_set(int* event_set)
{
  TMSG(CUDA, "Get event set");
  if (! event_set_created) {
    TMSG(CUDA, "No event set created, so create one");
    int ret = PAPI_create_eventset(&my_event_set);
    if (ret != PAPI_OK) {
      hpcrun_abort("Failure: PAPI_create_eventset.Return code = %d ==> %s",
                   ret, PAPI_strerror(ret));
    }
    *event_set = my_event_set;
    event_set_created = true;
    TMSG(CUDA, "Event set %d created", my_event_set);
  }
}


// Add event to my_event_set
void
papi_c_cupti_add_event(int event_set, int evcode)
{
  assert(event_set == my_event_set);

  int rv = PAPI_OK;
  if (! event_set_finalized) {
    TMSG(CUDA, "Adding event %x to cupti event set", evcode);
    rv = PAPI_add_event(my_event_set, evcode);
    if (rv != PAPI_OK) {
      hpcrun_abort("failure in PAPI gen_event_set(): PAPI_add_event() returned: %s (%d)",
                   PAPI_strerror(rv), rv);
    }
    TMSG(CUDA, "Added event %d, to cuda event set %d", evcode, my_event_set);
  }
}

// No adding new events after this point
void
papi_c_cupti_finalize_event_set(void)
{
  event_set_finalized = true;
}


void
papi_c_cupti_start()
{
  int ret = PAPI_start(my_event_set);
  if (ret != PAPI_OK) {
    hpcrun_abort("PAPI_start of event set %d failed with %s (%d)",
         my_event_set, PAPI_strerror(ret), ret);
  }
}


void
papi_c_cupti_read(long long *values)
{
  int ret = PAPI_read(my_event_set, values);
  if (ret != PAPI_OK) {
    hpcrun_abort("PAPI_read of event set %d failed with %s (%d)",
         my_event_set, PAPI_strerror(ret), ret);
  }
}


void
papi_c_cupti_stop(long long *values)
{
  int ret = PAPI_stop(my_event_set, values);
  if (ret != PAPI_OK) {
    hpcrun_abort("PAPI_stop of event set %d failed with %s (%d)",
         my_event_set, PAPI_strerror(ret), ret);
  }
}


static sync_info_list_t cuda_component = {
  .pred = is_papi_c_cuda,
  .get_event_set = papi_c_cupti_get_event_set,
  .add_event = papi_c_cupti_add_event,
  .finalize_event_set = papi_c_cupti_finalize_event_set,
  .is_gpu_sync = true,
  .setup = papi_c_no_action,
  .teardown = papi_c_no_action,
  .start = papi_c_cupti_start,
  .read = papi_c_cupti_read,
  .stop = papi_c_cupti_stop,
  .process_only = true,
  .next = NULL,
};


void
SS_OBJ_CONSTRUCTOR(papi_c_cupti)(void)
{
  papi_c_sync_register(&cuda_component);
}