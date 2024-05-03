// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

// ******************************************************************************
// System Includes
// ******************************************************************************

#define _GNU_SOURCE

#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <ucontext.h>

// ******************************************************************************
// Local Includes
// ******************************************************************************

#include "../mcontext.h"
#include "../context-pc.h"

//***************************************************************************
// interface functions
//***************************************************************************


void *
hpcrun_context_pc_async(void *context)
{
  ucontext_t* ctxt = (ucontext_t*)context;
  return ucontext_pc(ctxt);
}
