// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2023, Rice University
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

//******************************************************************************
// system includes
//******************************************************************************

#include <stdint.h>
#include <stdio.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "../gpu-binary.h"
#include "symbolVector.h"



//******************************************************************************
// macros
//******************************************************************************

#define DEBUG_PATCH_TOKEN 0



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct PatchTokenBinaryHeader {
  uint32_t magicNumber;
  uint32_t binaryVersion;
  uint32_t binaryLength;
  uint32_t deviceId;
  uint32_t steppingId;
  uint32_t gpuPtrBytes;
  uint32_t kernelCount;
} IntelPatchTokenBinaryHeader;


typedef struct PatchTokenKernelHeader {
  uint32_t kernelNameLength;
  uint32_t elf1Length;
  uint32_t elf2Length;
} IntelPatchTokenKernelHeader;



//******************************************************************************
// private operations
//******************************************************************************

// round val to nearest multiple of x
static uint32_t
roundToMultiple
(
  uint32_t val,
  uint32_t x
)
{
  return x * ((val + (x - 1)) / x);
}


static uint32_t
computeKernelNameSize
(
 uint32_t kernelNameLength
)
{
  return roundToMultiple(kernelNameLength, sizeof(uint32_t));
}



//******************************************************************************
// interface operations
//******************************************************************************

SymbolVector *
collectPatchTokenSymbols
(
 const char *patchTokenPtr,
 size_t patchTokenLength
)
{
  const char *cursor = patchTokenPtr;

  const IntelPatchTokenBinaryHeader *header =
    (const IntelPatchTokenBinaryHeader *) cursor;

  // skip past binary header
  cursor += sizeof(IntelPatchTokenBinaryHeader);

  int nsymbols = header->kernelCount;

  if (nsymbols != 0) {
    SymbolVector *symbols = symbolVectorNew(nsymbols);

    for (uint32_t i = 0; i < nsymbols; ++i) {
      const IntelPatchTokenKernelHeader *kernel_header =
        (const IntelPatchTokenKernelHeader *) cursor;

      // skip past kernel header
      cursor += sizeof(IntelPatchTokenKernelHeader);

      // add kernel name to symbol table
      symbolVectorAppend(symbols, cursor, 0);

      // skip past kernel name
      cursor += computeKernelNameSize(kernel_header->kernelNameLength);

      // skip past kernel binaries
      cursor += kernel_header->elf1Length + kernel_header->elf2Length;
    }

    if (DEBUG_PATCH_TOKEN) symbolVectorPrint(symbols, "Patch Token Binary Symbols");

    return symbols;
  }

  return 0;
}
