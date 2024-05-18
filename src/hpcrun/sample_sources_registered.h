// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef REGISTERED_SAMPLE_SOURCES_H
#define REGISTERED_SAMPLE_SOURCES_H

#include "sample-sources/sample_source_obj.h"

void hpcrun_ss_register(sample_source_t *src);
sample_source_t *hpcrun_source_can_process(char *event);
void hpcrun_registered_sources_init(void);
void hpcrun_display_avail_events(void);

#endif // REGISTERED_SAMPLE_SOURCES_H
