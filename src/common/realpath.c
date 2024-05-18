// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

/*************************** System Include Files ***************************/

#include <stdio.h>  // for FILENAME_MAX

#define __USE_XOPEN_EXTENDED // realpath()
#include <stdlib.h>

/**************************** User Include Files ****************************/

#include "realpath.h"

/**************************** Forward Declarations **************************/

/****************************************************************************/

/*
 * 'realpath' is a UNIX standard, but it is not standard in ANSI/ISO C++.
 * This is a C wrapper for the standard routine so that it can be available
 * for C++ programs.
 *
 */
const char*
RealPath(const char* nm)
{
  static __thread char _RealPathBuf[FILENAME_MAX]; // PATH_MAX

  if (realpath(nm, _RealPathBuf) == NULL) {
    return nm; /* error; return orig string */
  }
  else {
    return _RealPathBuf; /* resolved name has been copied here */
  }
}
