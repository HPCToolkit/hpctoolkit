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

#ifndef X86_INTERVAL_ARG_H
#define X86_INTERVAL_ARG_H

#include <stdbool.h>

#include "x86-interval-highwatermark.h"

#include <lib/isa-lean/x86/instruction-set.h>

// as extra arguments are added to the process inst routine, better
// to add fields to the structure, rather than keep changing the function signature

typedef struct interval_arg_t {
  // read only:
  void *beg;
  void *end;
  unwind_interval *first;

  // read/write:
  void *ins;
  unwind_interval *current;
  bool bp_just_pushed;
  highwatermark_t highwatermark;
  unwind_interval *canonical_interval;
  bool bp_frames_found;
  void *rax_rbp_equivalent_at;
  void *return_addr; // A place to store void * return values.
} interval_arg_t;

#endif // X86_INTERVAL_ARG_H
