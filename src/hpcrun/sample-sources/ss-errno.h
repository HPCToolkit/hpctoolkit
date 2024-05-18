// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//******************************************************************************
// File: ss-errno.h
//
// Purpose:
//   hpctoolkit sample sources MUST not disturb the application's value of
//   errno. define two macros to save and restore the application's errno
//   at entry and exit of signal handlers.
//
//******************************************************************************

#ifndef __hpctoolkit_ss_errno_h__
#define __hpctoolkit_ss_errno_h__

#include <errno.h>

#define HPCTOOLKIT_APPLICATION_ERRNO_SAVE() int application_errno = errno
#define HPCTOOLKIT_APPLICATION_ERRNO_RESTORE() errno = application_errno

#endif
