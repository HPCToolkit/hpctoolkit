// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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
