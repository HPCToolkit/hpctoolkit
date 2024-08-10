// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef EXCLUDE_SAMPLE_SOURCE_H
#define EXCLUDE_SAMPLE_SOURCE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


/***
 * Check if an event needs to be excluded or not.
 * @param event
 * @return true if the event has to be excluded, false otherwise
 */
bool is_event_to_exclude(const char *event);


#ifdef __cplusplus
} // extern "C"
#endif

#endif
