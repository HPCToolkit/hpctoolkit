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

#ifndef X86_INTERVAL_HIGHWATERMARK_H
#define X86_INTERVAL_HIGHWATERMARK_H

#include "x86-unwind-interval.h"

typedef struct highwatermark_t {
  unwind_interval *uwi;
  void *succ_inst_ptr; // pointer to successor (support for pathscale idiom)
  int state;
} highwatermark_t;

/******************************************************************************
 * macros 
 *****************************************************************************/

#define HW_INITIALIZED      0x8
#define HW_BP_SAVED         0x4
#define HW_BP_OVERWRITTEN   0x2
#define HW_SP_DECREMENTED   0x1
#define HW_UNINITIALIZED    0x0

#define HW_TEST_STATE(state, is_set, is_clear) \
  ((((state) & (is_set)) == (is_set))  && (((state) & (is_clear)) == 0x0))

#define HW_NEW_STATE(state, set) ((state) | HW_INITIALIZED | (set))

#endif  // X86_INTERVAL_HIGHWATERMARK_H

extern highwatermark_t _hw_uninit;
