// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//
// generic blame shifting sample source
//
// to use this code, define
//    BLAME_LAYER     -- the name of the software layer in which the blame
//                       shifting occurs, e.g. OpenMP
//    BLAME_NAME      -- the name of the event on the command line,
//                       e.g., OMP_MUTEX
//    BLAME_DIRECTED  -- define this if using directed blame shifting
//    BLAME_REQUEST   -- name of the function to be invoked to initialize
//                       the blame shifting

/******************************************************************************
 * macros
 *****************************************************************************/

#ifdef BLAME_DIRECTED
#define BLAME_TYPE directed
#else
#define BLAME_TYPE undirected
#endif



/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdio.h>
#include <string.h>



/******************************************************************************
 * local includes
 *****************************************************************************/

#include "../simple_oo.h"
#include "../sample_source_obj.h"
#include "../common.h"



/******************************************************************************
 * macros
 *****************************************************************************/

#define stringify_helper(n) #n
#define stringify(n) stringify_helper(n)



/*--------------------------------------------------------------------------
 | sample source methods
 --------------------------------------------------------------------------*/

static void
METHOD_FN(init)
{
  self->state = INIT;
}


static void
METHOD_FN(thread_init)
{
}


static void
METHOD_FN(thread_init_action)
{
}


static void
METHOD_FN(start)
{
}


static void
METHOD_FN(thread_fini_action)
{
}


static void
METHOD_FN(stop)
{
}


static void
METHOD_FN(shutdown)
{
  self->state = UNINIT;
}


static bool
METHOD_FN(supports_event,const char *ev_str)
{
  return (strstr(ev_str, stringify(BLAME_NAME)) != NULL);
}


static void
METHOD_FN(process_event_list, int lush_metrics)
{
}

static void
METHOD_FN(finalize_event_list)
{
}

static void
METHOD_FN(gen_event_set,int lush_metrics)
{
}


static void
METHOD_FN(display_events)
{
  printf("===========================================================================\n");
  printf("Available %s %s blame shifting preset events\n",
         stringify(BLAME_LAYER), stringify(BLAME_TYPE));
  printf("===========================================================================\n");
  printf("Name\t\tDescription\n");
  printf("---------------------------------------------------------------------------\n");
#ifdef BLAME_DIRECTED
  printf("%s\tWhen waiting for an %s mutex (e.g., a lock), shift\n"
         "\t\tblame for waiting to code holding the mutex.\n",
#else
  printf("%s\tWhen idle in the %s layer, shift blame to code being\n"
         "\t\texecuted at present by working threads.\n",
#endif
         stringify(BLAME_NAME), stringify(BLAME_LAYER));

  printf("\n");
}


/*--------------------------------------------------------------------------
 | sample source object
 --------------------------------------------------------------------------*/

#define ss_name BLAME_NAME
#define ss_cls SS_SOFTWARE

#include "../ss_obj.h"
