// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// File: patchTokenSymbols.h
//
// Purpose:
//   interface to determine cubin symbol relocation values that will be used
//   by hpcstruct
//
//***************************************************************************

#ifndef patchTokenSymbols_h
#define patchTokenSymbols_h

//******************************************************************************
// local includes
//******************************************************************************

#include "symbolVector.h"



//******************************************************************************
// interface functions
//******************************************************************************

SymbolVector *
collectPatchTokenSymbols
(
 const char *patch_token_ptr,
 size_t patch_token_len
);



#endif
