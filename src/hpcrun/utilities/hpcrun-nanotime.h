// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __hpcrun_nanotime_h__
#define __hpcrun_nanotime_h__

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>



//*****************************************************************************
// interface operations
//*****************************************************************************

uint64_t
hpcrun_nanotime
(
  void
);

int32_t
hpcrun_nanosleep
(
  uint32_t nsec
);

#endif
