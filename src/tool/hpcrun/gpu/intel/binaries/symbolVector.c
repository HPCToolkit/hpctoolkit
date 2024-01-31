// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2024, Rice University
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
// File: symbolVector.h
//
// Purpose:
//   type to represent symbol names and values in a binary
//
//***************************************************************************

//******************************************************************************
// global includes
//******************************************************************************

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
  v->symbolName = (const char **) calloc(nsymbols, sizeof(const char *));
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
