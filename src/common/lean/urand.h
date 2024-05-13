// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//******************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   Define an API for generating thread-local streams of uniform random
//   numbers.
//
//******************************************************************************

//******************************************************************************
// File urand.h
//******************************************************************************

#ifndef __urand_h__
#define __urand_h__

//******************************************************************************
// interface functions
//******************************************************************************

// generate a pseudo-random number between [0 .. RAND_MAX]. a thread-local
// seed is initialized using the time of day in microseconds when a thread
// first calls the generator.
int urand();

// generate a pseudo-random number [0 .. n], where n <= RAND_MAX
int urand_bounded(int n);

#endif
