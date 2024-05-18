// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// File: symbol-vector.h
//
// Purpose:
//   type to represent symbol names and values in a binary
//
//***************************************************************************

#ifndef symbolVector_h
#define symbolVector_h

//******************************************************************************
// type definitions
//******************************************************************************

typedef struct SymbolVector {
  int nsymbols;
  unsigned long *symbolValue;
  char **symbolName;
} SymbolVector;



//******************************************************************************
// interface functions
//******************************************************************************

SymbolVector *
symbolVectorNew
(
  int nsymbols
);


void
symbolVectorAppend
(
  SymbolVector *v,
  const char *symbolName,
  unsigned long symbolValue
);


void
symbolVectorFree
(
  SymbolVector *v
);


void
symbolVectorPrint
(
  SymbolVector *symbols,
  const char *kind
);



#endif
