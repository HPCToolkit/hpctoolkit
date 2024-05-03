// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef HANDLING_SAMPLE_H
#define HANDLING_SAMPLE_H

#include "thread_data.h"

extern void hpcrun_init_handling_sample(thread_data_t *td, int in, int id);
extern void hpcrun_set_handling_sample(thread_data_t *td);
extern void hpcrun_clear_handling_sample(thread_data_t *td);
extern int  hpcrun_is_handling_sample(void);


#endif
