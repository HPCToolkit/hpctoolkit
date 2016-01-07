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

#ifndef FRAME_H
#define FRAME_H

#include <unwind/common/unw-datatypes.h>
#include <utilities/ip-normalized.h>
#include <lib/prof-lean/lush/lush-support.h>

// --------------------------------------------------------------------------
// frame_t: similar to cct_node_t, but specialized for the backtrace buffer
// --------------------------------------------------------------------------

typedef struct frame_t {
  hpcrun_unw_cursor_t cursor;       // hold a copy of the cursor for this frame
  lush_assoc_info_t as_info;
  ip_normalized_t ip_norm;
  ip_normalized_t the_function;     // enclosing function of ip_norm
  void* ra_loc;
  lush_lip_t* lip;
} frame_t;

static inline
void*
hpcrun_frame_get_unnorm(frame_t* frame)
{
  return frame->cursor.pc_unnorm;
}

#endif // FRAME_H
