// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// File: symbolVector.h
//
// Purpose:
//   type to represent symbol names and values in a binary
//
//***************************************************************************

//******************************************************************************
// global includes
//******************************************************************************

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "symbolVector.h"



//******************************************************************************
// interface operations
//******************************************************************************

SymbolVector *
symbolVectorNew
(
  int nsymbols
)
{
  SymbolVector *v = (SymbolVector *) malloc(sizeof(SymbolVector));
  v->nsymbols = 0;
  v->symbolValue = (unsigned long *) calloc(nsymbols, sizeof(unsigned long));
  v->symbolName = (char **) calloc(nsymbols, sizeof(const char *));
  return v;
}


void
symbolVectorAppend
(
  SymbolVector *v,
  const char *symbolName,
  unsigned long symbolValue
)
{
  unsigned int i = v->nsymbols;

  v->symbolValue[i] = symbolValue;
  v->symbolName[i] = strdup(symbolName);

  v->nsymbols++;
}


void
symbolVectorFree
(
  SymbolVector *v
)
{
  for (int i=0; i < v->nsymbols; i++) {
    free(v->symbolName[i]);
  }
  free(v->symbolName);
  free(v->symbolValue);
  free(v);
}


void
symbolVectorPrint
(
  SymbolVector *v,
  const char *kind
)
{
  fprintf(stderr, "%s\n", kind);
  for (int i=0; i < v->nsymbols; i++) {
    fprintf(stderr, "  0x%lx %s\n", v->symbolValue[i], v->symbolName[i]);
  }
  fprintf(stderr, "\n");
}
