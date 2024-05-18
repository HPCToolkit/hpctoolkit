// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// Purpose:
//   Common types and values across multiple HPCToolkit formats
//
//   See doc/FORMATS.md.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef FORMATS_COMMON_H
#define FORMATS_COMMON_H

#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/// Success/error values returned by version-checking functions
/// Negative values are hard errors, positive values are soft failures
enum fmt_version_t {
  /// Version is fully compatible with the implementation, good to go
  fmt_version_exact = 0,
  /// Format identifier failed to match, not a file of the correct type
  fmt_version_invalid = -1,
  /// Incompatible version, below the current version (no backward-compatibility)
  fmt_version_backward = -2,
  /// Incompatible version, different major version
  fmt_version_major = -3,
  /// Forward-compatible version, fields added later will be missing
  fmt_version_forward = 1,
};

/// Major version of all the database formats
enum { FMT_DB_MajorVersion = 4 };

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // FORMATS_COMMON_H
