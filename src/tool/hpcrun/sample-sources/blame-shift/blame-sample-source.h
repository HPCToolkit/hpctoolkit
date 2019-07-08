// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: $
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2012, Rice University
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

#include <hpcrun/sample-sources/simple_oo.h>
#include <hpcrun/sample-sources/sample_source_obj.h>
#include <hpcrun/sample-sources/common.h>



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
  BLAME_REQUEST();
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

#include <hpcrun/sample-sources/ss_obj.h>
