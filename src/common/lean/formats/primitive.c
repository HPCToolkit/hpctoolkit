// SPDX-FileCopyrightText: 2002-2024 Rice University
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

#include "primitive.h"

// Instantiate symbols for the inline functions
uint16_t fmt_u16_read(const char[sizeof(uint16_t)]);
void fmt_u16_write(char[sizeof(uint16_t)], uint16_t);
uint32_t fmt_u32_read(const char[sizeof(uint32_t)]);
void fmt_u32_write(char[sizeof(uint32_t)], uint32_t);
uint64_t fmt_u64_read(const char[sizeof(uint64_t)]);
void fmt_u64_write(char[sizeof(uint64_t)], uint64_t);
double fmt_f64_read(const char[sizeof(double)]);
void fmt_f64_write(char[sizeof(double)], const double);
