// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// File: cubin-symbols.h
//
// Purpose:
//   interface to determine cubin symbol relocation values that will be used
//   by hpcstruct
//
//***************************************************************************

#ifndef cubin_symbols_h
#define cubin_symbols_h



//******************************************************************************
// type definitions
//******************************************************************************

typedef struct Elf_SymbolVector {
  int nsymbols;
  unsigned long *symbols;
} Elf_SymbolVector;



//******************************************************************************
// interface functions
//******************************************************************************

Elf_SymbolVector *
computeCubinFunctionOffsets
(
 const char *cubin_ptr,
 size_t cubin_len
);



#endif
