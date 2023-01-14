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
// Copyright ((c)) 2021-2023, Rice University
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

#ifndef LOGICAL_PYTHON_H
#define LOGICAL_PYTHON_H

#include <stdint.h>

/// Python-specific data to store in each #logical_frame_t.
typedef struct logical_python_frame_t {
  /// Cached fid for this frame.
  uint32_t fid;

  /// PyCodeObject used to generate the above fid. May be needed when bits go
  /// out of sync sometimes.
  void* code;
} logical_python_frame_t;

/// Python-specific data to store in each #logical_region_t.
typedef struct logical_python_region_t {
  /// Load module id for the Python infrastructure itself (libpython.so)
  int16_t lm;

  /// Topmost PyFrameObject that should not be reported as part of this region,
  /// or `NULL` if the bottommost Python region.
  void* caller;

  /// Current topmost PyFrameObject, as determined by tracer callbacks.
  void* frame;

  /// If not `NULL`, the PyObject that was called to exit Python (`C_CALL`).
  void* cfunc;
} logical_python_region_t;

/// Initialize the Python logical attribution sub-system.
extern void hpcrun_logical_python_init();

#endif  // LOGICAL_PYTHON_H
