// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef COMMON_SAMPLE_SOURCE_H
#define COMMON_SAMPLE_SOURCE_H

#include "common-method-def.h"

#define HPCRUN_PAPI_ERROR_UNAVAIL  1
#define HPCRUN_PAPI_ERROR_VERSION  2

// HPCRUN_CPU_FREQUENCY is used to add <no activity>
// to CPU traces where we do not any sample for a long period.
// The exact CPU frequency does not really matter.
#define HPCRUN_CPU_FREQUENCY 2000000000

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
