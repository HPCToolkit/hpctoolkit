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
// Copyright ((c)) 2002-2022, Rice University
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

#ifndef BACKTRACE_INFO_H
#define BACKTRACE_INFO_H

//
// Information about backtrace
// NOTE:
//    Minor data structure for transitional use
//    There are no operations, so type is concrete
//
//    Likely to be eventually replaced by an opaque backtrace datatype
//
#include "fence_enum.h"

#include "hpcrun/frame.h"

#include "../../frame.h"

#include <stdbool.h>
#include <stdint.h>
#include <unwind/common/fence_enum.h>

typedef struct {
  frame_t* begin;                // beginning frame of backtrace
  frame_t* last;                 // ending frame of backtrace (inclusive)
  size_t n_trolls;               // # of frames that resulted from trolling
  fence_enum_t fence       : 3;  // Type of stop -- thread or main *only meaninful when good unwind
  bool has_tramp           : 1;  // true when a trampoline short-circuited the unwind
  bool bottom_frame_elided : 1;  // true if bottom frame has been elided
  bool partial_unwind      : 1;  // true if not a full unwind
  bool collapsed           : 1;  // callstack collapsed by hpctoolkit, e.g. OpenMP placeholders
  void* trace_pc;                // in/out value: modified to adjust trace when modifying backtrace
} backtrace_info_t;

#endif  // BACKTRACE_INFO_H
