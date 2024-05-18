// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef linuxtimer_h
#define linuxtimer_h

//*****************************************************************************
// system includes
//*****************************************************************************

#include <signal.h>
#include <time.h>
#include <stdbool.h>



//*****************************************************************************
// type declarations
//*****************************************************************************

typedef struct {
  struct sigevent sigev;
  timer_t timerid;
  clockid_t clock;
} linuxtimer_t;



//*****************************************************************************
// interface operations
//*****************************************************************************

int
linuxtimer_create
(
 linuxtimer_t *td,
 clockid_t clock,
 int signal
);


int
linuxtimer_set
(
 linuxtimer_t *td,
 time_t sec,
 long nsec,
 bool repeat
);


int
linuxtimer_newsignal
(
 void
);


int
linuxtimer_getsignal
(
 linuxtimer_t *td
);


int
linuxtimer_delete
(
 linuxtimer_t *td
);



#endif
