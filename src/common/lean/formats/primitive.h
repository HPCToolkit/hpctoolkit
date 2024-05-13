// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

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
