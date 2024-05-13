// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef thread_use_h
#define thread_use_h

//******************************************************************************
// system include files
//******************************************************************************

#include <stdbool.h>
#include <inttypes.h>


void hpcrun_set_using_threads(bool flag);
bool hpcrun_using_threads_p();

#endif // thread_use_h
