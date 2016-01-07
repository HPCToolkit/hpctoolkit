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

#ifndef ALL_SAMPLE_SOURCES_H
#define ALL_SAMPLE_SOURCES_H

#define MAX_SAMPLE_SOURCES 2
#define MAX_HARDWARE_SAMPLE_SOURCES 1

#include <stdbool.h>
#include <stddef.h>

#include <sample-sources/sample_source_obj.h>

extern void hpcrun_sample_sources_from_eventlist(char *evl);
extern sample_source_t* hpcrun_fetch_source_by_name(const char *src);
extern bool hpcrun_check_named_source(const char *src);
extern void hpcrun_all_sources_init(void);
extern void hpcrun_all_sources_thread_init(void);
extern void hpcrun_all_sources_thread_init_action(void);
extern void hpcrun_all_sources_process_event_list(int lush_metrics);
extern void hpcrun_all_sources_gen_event_set(int lush_metrics);
extern void hpcrun_all_sources_start(void);
extern void hpcrun_all_sources_thread_fini_action(void);
extern void hpcrun_all_sources_stop(void);
extern void hpcrun_all_sources_shutdown(void);
extern bool  hpcrun_all_sources_started(void);
extern size_t hpcrun_get_num_sample_sources(void);

#define SAMPLE_SOURCES(op,...) hpcrun_all_sources_ ##op(__VA_ARGS__)

#endif // ALL_SAMPLE_SOURCES_H
