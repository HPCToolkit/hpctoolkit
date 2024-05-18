// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// File: Fatbin.hpp
//
// Purpose:
//   Interface for a routine that inspects and Elf module and collects
//   nested Elf modules.
//
//***************************************************************************

#ifndef __Fatbin_hpp__
#define __Fatbin_hpp__

//******************************************************************************
// local includes
//******************************************************************************

#include "ElfHelper.hpp"



//******************************************************************************
// interface functions
//******************************************************************************

bool
findCubins
(
 ElfFile *elfFile,
 ElfFileVector *elfFileVector
);


bool
isCubin(Elf *elf);


void
writeElfFile
(
 ElfFile *elfFile,
 const char *suffix
);

#endif
