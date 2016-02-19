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

/******************************************************************************
 * this is a draft of an interface that we can use to separate sampling 
 * sources from the profiler core. 
 *
 * each sampling source will define a method table.
 * sampling sources will register themselves to process arguments using
 * an init constructor. (that part of the interface isn't worked out yet.) 
 *
 * any sampling source selected with the command line arguments will 
 * register a method table with this interface. all active sampling sources 
 * registered will be invoked when an action on sampling sources 
 * (init, start, stop) is initiated.
 *****************************************************************************/

/******************************************************************************
 * type declarations 
 *****************************************************************************/
typedef enum {
  SAMPLING_INIT=0,
  SAMPLING_START=1,
  SAMPLING_STOP=2,
  SAMPLING_NMETHODS=3
} sampling_method;

typedef (*sampling_method_fn)(thread_data_t *td);

typedef struct{
  methods[SAMPLING_NMETHODS];
} sample_source_globals_t;



/******************************************************************************
 * local variables
 *****************************************************************************/
static sample_source_globals_t *ssg = NULL;



/******************************************************************************
 * forward declarations 
 *****************************************************************************/

static void call_sampling_methods(sampling_method method, thread_data_t *td);
static void call_sampling_method(sampling_source_globals_t *g, 
				 sampling_method method, thread_data_t *td);



/******************************************************************************
 * interface functions 
 *****************************************************************************/

void
hpcrun_add_sample_source(sample_source_globals_t *_ssgtd)
{
  ssgd->next = ssgd;
  ssgd = _ssgd;
}

//
// START/STOP inversion
void
hpcrun_start_sampling(thread_data_t* td)
{
  call_sampling_methods(SAMPLING_START, td);
}


void
hpcrun_stop_sampling(thread_data_t* td)
{
  call_sampling_methods(SAMPLING_STOP, td);
}


void
hpcrun_init_sampling(void)
{
  call_sampling_methods(SAMPLING_INIT, td);
}


/******************************************************************************
 * private operations
 *****************************************************************************/

static void
call_sampling_methods(sampling_method mid, thread_data_t* td)
{
  sampling_source_globals_t *g = ssg;
  for(; g; g = g->next) {
    call_sampling_method(g, mid, td);
  }
}


static void
call_sampling_method(sampling_source_globals_t* g, 
		     sampling_method mid, thread_data_t* td)
{
  (g->methods[mid])(td);
}
