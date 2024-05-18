// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef __HPCRUN_TOKENIZE_H__
#define __HPCRUN_TOKENIZE_H__

#include <stdbool.h>

// macros:
// return value of hpcrun_extract_ev_thresh()
//
#define THRESH_DEFAULT 0
#define THRESH_VALUE   1
#define THRESH_FREQ    2



extern char *start_tok(char *l);
extern int   more_tok(void);
extern char *next_tok(void);
extern int hpcrun_extract_threshold(const char *in, long *th, long def);
extern int hpcrun_extract_ev_thresh(const char*, int, char*, long*, long);
extern bool hpcrun_ev_is(const char* candidate, const char* event_name);

#endif
