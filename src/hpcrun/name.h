// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef CSPROF_NAME_H
#define CSPROF_NAME_H

/******************************************************************************
 * File: name.h
 *
 * Purpose: maintain the name of the executable
 *
 * Modification history:
 *   28 October 2007 - created - John Mellor-Crummey
 *****************************************************************************/

/******************************************************************************
 * macros
 *****************************************************************************/

#define UNKNOWN_MPI_RANK (-1)

/******************************************************************************
 * interface functions
 *****************************************************************************/

extern void hpcrun_set_executable_name(char *argv0);
extern const char *hpcrun_get_executable_name();

#endif
