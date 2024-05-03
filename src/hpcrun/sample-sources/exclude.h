// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef EXCLUDE_SAMPLE_SOURCE_H
#define EXCLUDE_SAMPLE_SOURCE_H

#include <stdbool.h>

/***
 * Check if an event needs to be excluded or not.
 * @param event
 * @return true if the event has to be excluded, false otherwise
 */
bool is_event_to_exclude(char *event);

#endif
