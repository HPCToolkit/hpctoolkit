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
