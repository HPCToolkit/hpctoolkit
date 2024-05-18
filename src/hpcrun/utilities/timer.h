// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __hpcrun_timer__
#define __hpcrun_timer__

//*****************************************************************************
// system includes
//*****************************************************************************

#include <time.h>



//*****************************************************************************
// interface operations
//*****************************************************************************

void timer_start
(
 struct timespec *start_time
);


double
timer_elapsed
(
 struct timespec *start_time
);

#endif
