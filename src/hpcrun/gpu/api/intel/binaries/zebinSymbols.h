// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// File: zebin-symbols.h
//
// Purpose:
//   interface to determine cubin symbol relocation values that will be used
//   by hpcstruct
//
//***************************************************************************

#ifndef zebinSymbols_h
#define zebinSymbols_h

//******************************************************************************
// local includes
//******************************************************************************

#include "symbolVector.h"



//******************************************************************************
// interface functions
//******************************************************************************

SymbolVector *
collectZebinSymbols
(
 const char *zebin_ptr,
 size_t zebin_len
);



#endif
