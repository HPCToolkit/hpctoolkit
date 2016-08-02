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

// The stub for the implementation of caliper sample source.
// See implementation in caliper.cpp

#include "caliper-stub.h"

#include "simple_oo.h"
#include "sample_source_obj.h"
#include "common.h"

#include <hpcrun/sample_sources_registered.h>

static void
METHOD_FN(init) {
    caliper_init(self);
}

static void
METHOD_FN(thread_init) {
    caliper_thread_init(self);
}

static void
METHOD_FN(thread_init_action)
{
    caliper_thread_init_action(self);
}

static void
METHOD_FN(start)
{
	caliper_start(self);
}

static void
METHOD_FN(thread_fini_action)
{
	caliper_thread_fini_action(self);
}

static void
METHOD_FN(stop)
{
	caliper_stop(self);
}

static void
METHOD_FN(shutdown)
{
	caliper_shutdown(self);
}

static bool
METHOD_FN(supports_event, const char *ev_str)
{
	return caliper_supports_event(self, ev_str);
}
 
static void
METHOD_FN(process_event_list, int lush_metrics)
{
	caliper_process_event_list(self, lush_metrics);
}

static void
METHOD_FN(gen_event_set,int lush_metrics)
{
	caliper_gen_event_set(self, lush_metrics);
}

static void
METHOD_FN(display_events)
{
	caliper_display_events(self);
}

/***************************************************************************
 * object
 ***************************************************************************/

#define ss_name caliper
#define ss_cls SS_SOFTWARE

#include "ss_obj.h"

