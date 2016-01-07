// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <messages/messages.h>
#include <lib/support-lean/OSUtil.h>
#include "sample_prob.h"

#define HPCRUN_SAMPLE_PROB  "HPCRUN_PROCESS_FRACTION"
#define DEFAULT_PROB  0.1

#define HASH_PRIME  2001001003
#define HASH_GEN    4011

static int is_init = 0;
static pid_t orig_pid = 0;
static int sample_prob_ans = 1;

static char *sample_prob_str = NULL;
static int prob_str_broken = 0;
static int prob_str_mesg = 0;


// -------------------------------------------------------------------
// This file implements probability-based sampling.  All processes
// continue to take samples, but if HPCRUN_SAMPLE_PROB is set in the
// environment, then only a fraction of the processes (based on a
// pseudo-random seed) open their .log and .hpcrun files and write out
// their results.
//
// HPCRUN_SAMPLE_PROB may be written as a a floating point number or
// as a fraction.  So, '0.10' and '1/10' are equivalent.
//
// The decision of which processes are active is process-wide, not
// per-thread (for now).
// -------------------------------------------------------------------


// Accept 0.ddd as floating point or x/y as fraction.
// Note: must delay printing any errors.
//
static float
string_to_prob(char *str)
{
  int x, y;
  float ans;

  if (strchr(str, '/') != NULL) {
    if (sscanf(str, "%d/%d", &x, &y) == 2 && y > 0) {
      ans = (float)x / (float)y;
    } else {
      prob_str_broken = 1;
      ans = DEFAULT_PROB;
    }
  }
  else {
    if (sscanf(str, "%f", &ans) < 1) {
      prob_str_broken = 1;
      ans = DEFAULT_PROB;
    }
  }

  return ans;
}


// Combine the hostid, the time of day in microseconds and
// /dev/urandom (if available), run it through a hash function and
// produce a pseudo-random value in the range [0.0, 1.0).
//
// This is a simple hash function based on the exponential mod
// function with good cryptographic properties.  MD5 or SHA-1 would
// make sense, but those require bringing in extra libraries.
//
// Anyway, the choice of seed is far more important than the hash
// function here.
//
static float
random_hash_prob(void)
{
  struct timeval tv;
  uint64_t a, b, x, rand;
  int fd;

  // Add /dev/urandom if available.
  rand = 0;
  fd = open("/dev/urandom", O_RDONLY);
  if (fd >= 0) {
    read(fd, &rand, sizeof(rand));
    close(fd);
  }

  gettimeofday(&tv, NULL);
  x = (((uint64_t) OSUtil_hostid()) << 24) + (tv.tv_usec << 4) + rand;
  x = (x & ~(((uint64_t) 15) << 60)) % HASH_PRIME;

  // Compute gen^x (mod prime).
  // Invariant: a * (b ^ x) = gen^(orig x) (mod prime).
  a = 1;
  b = HASH_GEN;
  while (x > 0) {
    if (x % 2 == 0) {
      b = (b * b) % HASH_PRIME;
      x = x/2;
    } else {
      a = (a * b) % HASH_PRIME;
      x = x - 1;
    }
  }

  return (float)a / (float)HASH_PRIME;
}


void
hpcrun_sample_prob_init(void)
{
  pid_t cur_pid;

  // For consistency, don't recompute the sample probability if the
  // pid hasn't changed.  But do recompute in the child after fork.
  cur_pid = getpid();
  if (is_init && cur_pid == orig_pid)
    return;
  orig_pid = cur_pid;

  // If HPCRUN_SAMPLE_PROB is not set in the environment, then the
  // answer is always on.
  sample_prob_str = getenv(HPCRUN_SAMPLE_PROB);
  if (sample_prob_str != NULL) {
    sample_prob_ans = (random_hash_prob() < string_to_prob(sample_prob_str));
  }
  else {
    sample_prob_ans = 1;
  }
  is_init = 1;
}


int
hpcrun_sample_prob_active(void)
{
  if (! is_init) {
    hpcrun_sample_prob_init();
  }
  return sample_prob_ans;
}


// We can't print messages while computing the sample probability
// because that would trigger opening the log files and recomputing
// the sample probability.  Instead, we have to record the failure and
// depend on the caller to call us again after the log files are
// opened.
//
void
hpcrun_sample_prob_mesg(void)
{
  if (prob_str_broken && (! prob_str_mesg) && sample_prob_ans) {
    EMSG("malformed probability in %s (%s), using default value of %f",
	 HPCRUN_SAMPLE_PROB, sample_prob_str, DEFAULT_PROB);
    prob_str_mesg = 1;
  }
}
