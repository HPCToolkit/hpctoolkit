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
// Copyright ((c)) 2002-2017, Rice University
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
// user include files
//***************************************************************************

#include <stdlib.h>

#include "perf-util.h" // event_info_t

#ifndef SRC_TOOL_HPCRUN_SAMPLE_SOURCES_PERF_EVENT_DESC_H_
#define SRC_TOOL_HPCRUN_SAMPLE_SOURCES_PERF_EVENT_DESC_H_


typedef struct event_desc_list_s event_desc_list_t;

int event_desc_add(event_info_t *event);

event_info_t* event_desc_find(int metric);

int event_desc_get_num();

event_desc_list_t* event_desc_get_first();

event_desc_list_t* event_desc_get_next(event_desc_list_t *item);

event_info_t* event_desc_get_event_info(event_desc_list_t *item);

#endif /* SRC_TOOL_HPCRUN_SAMPLE_SOURCES_PERF_EVENT_DESC_H_ */
