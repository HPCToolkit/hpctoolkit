// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// File:
// $HeadURL$
//
// Purpose:
// Provide weak symbols of static overrides to handle the case where
// these don't exist.
//
// Note: the weak symbols need to be in a separate file so the
// compiler doesn't inline them.
//
//***************************************************************************

#define _GNU_SOURCE

#include <stdint.h>


int __attribute__ ((weak))
dmapp_get_jobinfo(void *ptr)
{
  return -1;
}


// Technically, gasnet_node_t is unsigned 32-bit, but we need to allow
// -1 to indicate unknown.

int32_t __attribute__ ((weak)) gasneti_mynode = -1;
