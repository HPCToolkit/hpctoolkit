// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

// Simple utility for iterating the tokens out of a given string
// FIXME: don't use strtok(), don't use single static buffer,
// factor out delimiters, etc.

#define _GNU_SOURCE

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "tokenize.h"

#define MIN(a,b)  (((a)<=(b))?(a):(b))

#define EVENT_DELIMITER '@'
#define PREFIX_FREQUENCY 'f'

static char *tmp;
static char *tk;
static char *last;

static char *sep1 = " ;";

char *
start_tok(char *lst)
{
  tmp = strdup(lst);
  tk  = strtok_r(tmp,sep1,&last);
  return tk;
}

int
more_tok(void)
{
  if (! tk){
    free(tmp);
  }
  return (tk != NULL);
}

char *
next_tok(void)
{
  tk = strtok_r(NULL,sep1,&last);
  return tk;
}


/**
 * extract the threshold
 */
// Returns:
//   THRESH_FREQ     if event has explicit frequency
//   THRESH_VALUE    if event has explicit threshold,
//   THRESH_DEFAULT  if using default.
int
hpcrun_extract_threshold
(
 const char *input_string,
 long *threshold,
 long default_value
)
{
  int type = THRESH_VALUE;

  if (input_string == NULL) {
    *threshold = default_value;
    type = THRESH_DEFAULT;
  } else {
    if (*input_string == PREFIX_FREQUENCY) {
      input_string++; // skip the PREFIX_FREQUENCY character
      type = THRESH_FREQ;
    } else {
      type = THRESH_VALUE;
    }

    char *endptr;
    long value = strtol(input_string, &endptr, 10);
    if (value == 0) {
      //  If there were no digits at all, strtol() stores the original
      //  value of nptr in *endptr. if there were no digits at all, use
      //  the default threshold; otherwise, the value 0 was supplied and
      //  intended.
      if (endptr == input_string) {
        value = default_value;
        if (type == THRESH_FREQ) {
          // FIXME: Laksono: we have some choices
          //
          //   Should the type be changed to THRESH_VALUE here?
          //
          //   Should we just signal an error and quit? Perhaps we are here
          //   because of a typo.
        }
      }
    }

    *threshold = value;
  }

  return type;
}

// event option syntax is event_name @ [f] threshold
//   if the f indicator exist, the number is the frequency, otherwise
//   it's a period number
// Returns:
//   THRESH_FREQ     if event has explicit frequency
//   THRESH_VALUE    if event has explicit threshold,
//   THRESH_DEFAULT  if using default.
//
int
hpcrun_extract_ev_thresh(const char *in, int evlen, char *ev, long *th, long def)
{
  unsigned int threshold_pos = 0;
  unsigned int len = strlen(in);

  char *dlm = strrchr(in, EVENT_DELIMITER);
  if (dlm) {
    if (isdigit(dlm[1]) || dlm[1] == PREFIX_FREQUENCY) {
      // make sure the number if the threshold number,
      // don't accept like xxxx:123b which is a named event

      // we probably have threshold number
      len = MIN(dlm - in, evlen);
      strncpy(ev, in, len);
      ev[len] = '\0';

      return hpcrun_extract_threshold(dlm+1+threshold_pos, th, def);
    }
    if (dlm[0] == EVENT_DELIMITER) {
      len = MIN(dlm - in, evlen);
    }
  }
  // no threshold or the threshold is not a number
  strncpy(ev, in, len);
  ev[len] = '\0';
  *th = def;

  return THRESH_DEFAULT;
}

//
// Check  "event" part of candidate string
// for an **exact** match event_name
//
// candidate strings may be either a single event name, or EVENT_NAME@SOMETHING or EVENT_NAME:SOMETHING
//
bool
hpcrun_ev_is(const char* candidate, const char* event_name)
{
  return (strstr(candidate, event_name) == candidate) && strchr("@:",candidate[strlen(event_name)]);
}

#if TEST_TOKENIZE

int
main (int argc, char *argv[])
{
  const char *tokens[]={ "one", "two@", "three@3", "four@f4", "five@f", "six@s", "seven::perf",
      "perf::eight@", "perf::nine@f200", "perf:ten@10", "eleven@123b", "twelve:123b@10000",
      "thirteen:123b@f100" };
  const int elem = 13;
  int i, res;
  long th;
  char ev[100];

  for(i=0; i<elem; i++) {
    res = hpcrun_extract_ev_thresh(tokens[i], 100, ev, &th, -1);
    printf("%d: %s --> ev: %s, t: %ld, r: %d\n", i, tokens[i], ev, th, res);
  }
}
#endif
