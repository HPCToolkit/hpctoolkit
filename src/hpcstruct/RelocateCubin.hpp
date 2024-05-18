// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// File: relocate_cubin.hpp
//
// Purpose:
//   Interface definition for in-memory cubin relocation.
//
// Description:
//   The associated implementation performs in-memory relocation of a cubin so
//   that all text segments and functions are non-overlapping. Following
//   relocation
//     - each text segment begins at its offset in the segment
//     - each function, which is in a unique text segment, has its symbol
//       adjusted to point to the new position of the function in its relocated
//       text segment
//     - addresses in line map entries are adjusted to account for the new
//       offsets of the instructions to which they refer
//
//***************************************************************************

#ifndef __RelocateCubin_hpp__
#define __RelocateCubin_hpp__

#include "ElfHelper.hpp"

bool
relocateCubin
(
 char *cubin_ptr,
 size_t cubin_size,
 Elf *cubin_elf
);

#endif
