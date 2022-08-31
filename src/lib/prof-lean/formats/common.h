// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2020, Rice University
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
