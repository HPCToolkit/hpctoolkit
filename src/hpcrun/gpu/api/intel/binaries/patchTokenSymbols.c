// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************

//******************************************************************************
// system includes
//******************************************************************************

#define _GNU_SOURCE

#include <stddef.h>
#include <stdint.h>



//******************************************************************************
// local includes
//******************************************************************************

#include "symbolVector.h"



//******************************************************************************
// macros
//******************************************************************************

#define DEBUG_PATCH_TOKEN_SYMBOLS 0



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

    if (DEBUG_PATCH_TOKEN_SYMBOLS) {
      symbolVectorPrint(symbols, "Patch Token Binary Symbols");
    }

    return symbols;
  }

  return 0;
}
