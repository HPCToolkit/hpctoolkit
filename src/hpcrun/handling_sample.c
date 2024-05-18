// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include <stdlib.h>
#include "thread_data.h"

#include "handling_sample.h"

#include "messages/messages.h"

//
// id specifically passed in, since id has not been set yet!!
//
void
hpcrun_init_handling_sample(thread_data_t *td, int in, int id)
{
  TMSG(HANDLING_SAMPLE,"INIT called f thread %d", id);
  td->handling_sample = in;
}


void
hpcrun_set_handling_sample(thread_data_t *td)
{
  if (td->handling_sample != 0)
    hpcrun_terminate();

  td->handling_sample = 0xDEADBEEF;
}


void
hpcrun_clear_handling_sample(thread_data_t *td)
{
  if (td->handling_sample != 0xDEADBEEF)
    hpcrun_terminate();

  td->handling_sample = 0;
}


int
hpcrun_is_handling_sample(void)
{
  return (TD_GET(handling_sample) == 0xDEADBEEF);
}
