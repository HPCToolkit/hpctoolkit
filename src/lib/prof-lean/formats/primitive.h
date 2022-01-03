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

//***************************************************************************
//
// Purpose:
//   Low-level functions for standard byte-conversions
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef FORMATS_PRIMITIVE_H
#define FORMATS_PRIMITIVE_H

#include "common.h"

#include <string.h>
#include <endian.h>

#if defined(__cplusplus)
extern "C" {
#endif

// NOTE: Using memcpy in the functions dodges strict aliasing issues that might
// come up with just type-punning. It optimizes away completely at -O1.

// Operations for u16 (uint16_t)
static_assert(sizeof(uint16_t) == 2, "uint16_t isn't 2 bytes?");
inline uint16_t fmt_u16_read(const char v[sizeof(uint16_t)]) {
  uint16_t o;
  memcpy(&o, v, sizeof(uint16_t));
  return le16toh(o);
}
inline void fmt_u16_write(char o[sizeof(uint16_t)], uint16_t v) {
  v = htole16(v);
  memcpy(o, &v, sizeof(uint16_t));
}

// Operations for u32 (uint32_t)
static_assert(sizeof(uint32_t) == 4, "uint32_t isn't 4 bytes?");
inline uint32_t fmt_u32_read(const char v[sizeof(uint32_t)]) {
  uint32_t o;
  memcpy(&o, v, sizeof(uint32_t));
  return le32toh(o);
}
inline void fmt_u32_write(char o[sizeof(uint32_t)], uint32_t v) {
  v = htole32(v);
  memcpy(o, &v, sizeof(uint32_t));
}

// Operations for u64 (uint64_t)
static_assert(sizeof(uint64_t) == 8, "uint64_t isn't 8 bytes?");
inline uint64_t fmt_u64_read(const char v[sizeof(uint64_t)]) {
  uint64_t o;
  memcpy(&o, v, sizeof(uint64_t));
  return le64toh(o);
}
inline void fmt_u64_write(char o[sizeof(uint64_t)], uint64_t v) {
  v = htole64(v);
  memcpy(o, &v, sizeof(uint64_t));
}

// Operations for f64 (double)
static_assert(sizeof(double) == 8, "doubles aren't 8 bytes?");
inline double fmt_f64_read(const char v[sizeof(double)]) {
  uint64_t vv = fmt_u64_read(v);
  double o;
  memcpy(&o, &vv, sizeof(double));
  return o;
}
inline void fmt_f64_write(char o[sizeof(double)], const double v) {
  uint64_t vv;
  memcpy(&vv, &v, sizeof(double));
  fmt_u64_write(o, vv);
}

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif  // FORMATS_PRIMITIVE_H
