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
//   Define the interface for a function that returns the time of day in
//   microseconds as a long integer.
//
//******************************************************************************

#ifndef __usec_time_h__
#define __usec_time_h__

//******************************************************************************
// interface functions
//******************************************************************************

// return the time of day in microseconds as a long integer
unsigned long usec_time();

#endif
