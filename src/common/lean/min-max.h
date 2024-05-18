// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   unsigned int types
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   Nathan Tallent, Rice University.
//
//***************************************************************************

#ifndef include_min_max_h
#define include_min_max_h

//****************************************************************************

//****************************************************************************
// MIN/MAX for C (use std::min/max for C++)
//****************************************************************************

#if !defined(__cplusplus)

# undef MIN
# undef MAX
# define MIN(a,b) (((a) < (b)) ? (a) : (b))
# define MAX(a,b) (((a) > (b)) ? (a) : (b))

#endif


//****************************************************************************

#endif /* include_min_max_h */
