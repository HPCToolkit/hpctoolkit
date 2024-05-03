// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef _HPCRUN_START_STOP_H_
#define _HPCRUN_START_STOP_H_

// Foil bases for the external API
int foilbase_hpctoolkit_sampling_is_active();
void foilbase_hpctoolkit_sampling_start();
void foilbase_hpctoolkit_sampling_stop();


// Internal functions
void hpcrun_start_stop_internal_init(void);

#endif
