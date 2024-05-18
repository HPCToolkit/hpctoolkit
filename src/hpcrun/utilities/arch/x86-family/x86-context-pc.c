// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
// system include files
//***************************************************************************

#define _GNU_SOURCE

#include <stdio.h>
#include <setjmp.h>
#include <stdbool.h>

#include <sys/types.h>
#include <ucontext.h>

//***************************************************************************
// local include files
//***************************************************************************

#include "../context-pc.h"
#include "../mcontext.h"

//****************************************************************************
// forward declarations
//****************************************************************************

//****************************************************************************
// interface functions
//****************************************************************************

void*
hpcrun_context_pc_async(void* context)
{
  mcontext_t *mc = GET_MCONTEXT(context);
  return MCONTEXT_PC(mc);
}
