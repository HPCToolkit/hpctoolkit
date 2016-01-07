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

#ifndef COMMON_SAMPLE_SOURCE_H
#define COMMON_SAMPLE_SOURCE_H

#include "common-method-def.h"

#define HPCRUN_PAPI_ERROR_UNAVAIL  1
#define HPCRUN_PAPI_ERROR_VERSION  2

void  METHOD_FN(hpcrun_ss_add_event, const char* ev);
void  METHOD_FN(hpcrun_ss_store_event, int event_id, long thresh);
void  METHOD_FN(hpcrun_ss_store_metric_id, int event_id, int metric_id);
char* METHOD_FN(hpcrun_ss_get_event_str);
bool  METHOD_FN(hpcrun_ss_started);

// Interface (NON method) functions

int hpcrun_event2metric(sample_source_t* ss, int event_idx);

void hpcrun_ssfail_none(void);
void hpcrun_ssfail_unknown(char *event);
void hpcrun_ssfail_unsupported(char *source, char *event);
void hpcrun_ssfail_derived(char *source, char *event);
void hpcrun_ssfail_all_derived(char *source);
void hpcrun_ssfail_conflict(char *source, char *event);
void hpcrun_ssfail_start(char *source);
void hpcrun_save_papi_error(int error);

#endif // COMMON_SAMPLE_SOURCE_H
