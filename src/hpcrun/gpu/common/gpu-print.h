// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_print_h
#define gpu_print_h



//******************************************************************************
// system includes
//******************************************************************************

#if DEBUG
#include <stdio.h>
#endif



//******************************************************************************
// macros
//******************************************************************************

#if DEBUG
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif



#endif
