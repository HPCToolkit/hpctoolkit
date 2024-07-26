// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

// This file contains the routines that scan instructions to identify functions

#include "scan.h"

#include <endian.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dwarf.h>
#include <string.h>

// HACK
#define ENABLED(x) (1)
#define TMSG(f, ...) do { fprintf(stderr, #f ": " __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#define ETMSG(f, ...) do { fprintf(stderr, #f ": " __VA_ARGS__); fprintf(stderr, "\n"); } while (0)
#define hpcrun_abort(...) do { fprintf(stderr, __VA_ARGS__); exit(1); } while(0)

// Defines
#define EHF_MAX_CIE       (64)
#define EHF_CIE_TRUE      (1)
#define EHF_CIE_FALSE     (0)
#define EHF_CIE_EXTREC    (0xfffffffful)
#define EHF_FDE_DMASK     (0x0f)
#define EHF_FDE_OMASK     (0xf0)
#define EHF_CF_DONE           (0)
#define EHF_CF_CONT           (1)
#define EHF_WO_TYPE           (1)
#define EHF_WO_FS               (2)
#define EHF_TP_CIE            (0)
#define EHF_SLEB128_ERROR (-1)
#define EHF_ULEB128_ERROR (0xffffffffffffffffull)
#define EHF_DECDWRF_ERROR (0xffffffffffffffffull)

#define EHF_CIE_BO_VER    (8)
#define EHF_CIE_BO_AUSTR  (9)

#define EHF_CIE_VER_1     (1)
#define EHF_CIE_VER_3     (3)

// some dwarf.h may not have DW_EH_PE_indirect, needed for personality fn
// there is also a combo type
//
#ifndef DW_EH_PE_indirect
#define DW_EH_PE_indirect (0x80)
#endif
// this might also be a personality decode op
#define DW_EH_PE_indpcrel (DW_EH_PE_indirect | DW_EH_PE_pcrel)
// sign values for unalignedEndianAddr, which returns unsigned
#define EHF_UER_SIGNED    (1)
#define EHF_UER_UNSIGNED  (0)

// in case someone produces a eh_frame stream that is in big endian order, change the define below
// this is only supported on a big endian order core.
#define EHF_STREAM_LE     (1)
#define EHF_STREAM_BE     (0)
#define EHF_STREAM_ORDER  (EHF_STREAM_LE)
// #define EHF_STREAM_ORDER  (EHF_STREAM_BE)

// determine the stack offset for CIE's with 'S'.  In practice
// this seems independent of instruction word size, but might not be
// on some architectures.  if funnies are produced, this is where to fix
// it.  If there is a macro that gives the minimum instruction word size, this is
// where to put it.
#define EHF_FOLLOW_IWS    (0)
#if EHF_FOLLOW_IWS
#if defined ( __x86_64__ ) || defined ( __i386__ )
#define EHF_FORWARD_OFF   (1)
#else
#define EHF_FORWARD_OFF   (4)
#endif
#else
#define EHF_FORWARD_OFF   (1)
#endif

typedef struct __ehFDEAddrType {
  uint8_t R;
  uint8_t P;
  uint8_t L;
} ehFDEAddrType_t;

typedef struct __ehCIERecord {
  uint64_t cieBaseAddress;
  uint64_t cieSize;
  uint64_t cieAugDataOffset;
  uint8_t * cieAugData;
  uint64_t cieAddressOffset;  // stack offset
  ehFDEAddrType_t cieContainsType;
  ehFDEAddrType_t cieFDEEncType;
} ehCIERecord_t;

typedef struct __ehDecodeRecord {
  uint64_t ehSecAddr;
  uint64_t textSecAddr;
  uint64_t dataSecAddr;
  uint64_t totalOffset;  // record offset + interior offset
  uint8_t fdeEnc;
} ehDecodeRecord_t;

static uint64_t  decodeULEB128(uint8_t *input, uint64_t *sizeInBytes);
static int64_t   decodeSLEB128(uint8_t *input, uint64_t *sizeInBytes);
static uint64_t  decodeDwarfAddress(uint8_t *, ehDecodeRecord_t *, uint64_t *, char *);
static uint64_t  unalignedEndianRead(uint8_t *, size_t, uint64_t);

uint64_t
decodeDwarfAddress(uint8_t *streamPtr, ehDecodeRecord_t *decodeRecord, uint64_t *adv, char *errString)
{
  uint64_t unsignedBaseAddr, unsignedFunctionAddr, unsignedRelAddr;
  int64_t signedRelAddr;
  uint64_t l, advance;
  uint8_t fdeAddrDecodeType, fdeAddrOpType;

  fdeAddrDecodeType = (decodeRecord->fdeEnc) & EHF_FDE_DMASK;
  fdeAddrOpType = (decodeRecord->fdeEnc) & EHF_FDE_OMASK;

  if ( (fdeAddrOpType == DW_EH_PE_omit) || (fdeAddrDecodeType == DW_EH_PE_omit)) {
    ETMSG(FNBOUNDS, "Warning in eh_frame %s, omit found",errString);
    return EHF_DECDWRF_ERROR;
  }

  unsignedBaseAddr = 0ull;
  unsignedFunctionAddr = 0ull;
  advance = 0;

        switch(fdeAddrOpType) {
                case DW_EH_PE_absptr:
                        unsignedBaseAddr = 0ull;
                case DW_EH_PE_pcrel:
                case DW_EH_PE_indirect:         // personality only
                case DW_EH_PE_indpcrel:         // personality only
                        unsignedBaseAddr = decodeRecord->ehSecAddr;  // native uint64_t
                        unsignedBaseAddr += (decodeRecord->totalOffset);
                        break;
                case DW_EH_PE_textrel:
                        unsignedBaseAddr = decodeRecord->textSecAddr;
                        break;
                case DW_EH_PE_datarel:
                        unsignedBaseAddr = decodeRecord->dataSecAddr;
                        break;
                //
                // These cases do not apply.
                //
                case DW_EH_PE_funcrel:
                case DW_EH_PE_aligned:
                default:
                        ETMSG(FNBOUNDS, "Error in eh_frame %s, unsupported address operation type %.2x",
                                        errString, fdeAddrOpType);
      *adv = 0;
                        return EHF_DECDWRF_ERROR;
                        break;
        }

        switch (fdeAddrDecodeType) {
                case DW_EH_PE_absptr:
      unsignedRelAddr = unalignedEndianRead(streamPtr,sizeof(uint64_t),EHF_UER_UNSIGNED);
      unsignedFunctionAddr = unsignedBaseAddr + unsignedRelAddr;
      advance += sizeof(uint64_t);
                        break;
                case DW_EH_PE_uleb128:
                        unsignedRelAddr = decodeULEB128(streamPtr, &l);
                        unsignedFunctionAddr = unsignedBaseAddr + unsignedRelAddr;
      advance += l;
                        break;
                case DW_EH_PE_udata2:
      unsignedRelAddr = unalignedEndianRead(streamPtr,sizeof(uint16_t),EHF_UER_UNSIGNED);
                        unsignedFunctionAddr = unsignedBaseAddr + unsignedRelAddr;
      advance += sizeof(uint16_t);
                        break;
                case DW_EH_PE_udata4:
      unsignedRelAddr = unalignedEndianRead(streamPtr,sizeof(uint32_t),EHF_UER_UNSIGNED);
                        unsignedFunctionAddr = unsignedBaseAddr + unsignedRelAddr;
      advance += sizeof(uint32_t);
                        break;
                case DW_EH_PE_udata8:
      unsignedRelAddr = unalignedEndianRead(streamPtr,sizeof(uint64_t),EHF_UER_UNSIGNED);
                        unsignedFunctionAddr = unsignedBaseAddr + unsignedRelAddr;
      advance += sizeof(uint64_t);
                        break;
                //
                // if the relative part is signed, we assume the base has
                // the high bit clear (i.e. non-negative for signed arithmetic).
                // This should always be so in user space.  If it's not, we need a
                // special addition routine that does negate subtract of the addend.
                //
                case DW_EH_PE_sleb128:
                        signedRelAddr = decodeSLEB128(streamPtr, &l);
                        unsignedFunctionAddr = (uint64_t)((int64_t)unsignedBaseAddr + signedRelAddr);
      advance += l;
                        break;
                case DW_EH_PE_sdata2:
      signedRelAddr = (int64_t) unalignedEndianRead(streamPtr,sizeof(uint16_t),EHF_UER_SIGNED);
                        unsignedFunctionAddr = (uint64_t)((int64_t)unsignedBaseAddr + signedRelAddr);
      advance += sizeof(uint16_t);
                case DW_EH_PE_sdata4:
      signedRelAddr = (int64_t) unalignedEndianRead(streamPtr,sizeof(uint32_t),EHF_UER_SIGNED);
                        unsignedFunctionAddr = (uint64_t)((int64_t)unsignedBaseAddr + signedRelAddr);
      advance += sizeof(uint32_t);
                        break;
                case DW_EH_PE_sdata8:
      signedRelAddr = (int64_t) unalignedEndianRead(streamPtr,sizeof(uint64_t),EHF_UER_SIGNED);
                        unsignedFunctionAddr = (uint64_t)((int64_t)unsignedBaseAddr + signedRelAddr);
      advance += sizeof(uint64_t);
                default:
                        ETMSG(FNBOUNDS, "Error in eh_frame %s, unsupported address decode type %.2x",
        errString, fdeAddrDecodeType);
      *adv = 0;
                        return EHF_DECDWRF_ERROR;
                        break;
        }

  *adv = advance;
  return unsignedFunctionAddr;

}

int64_t
decodeSLEB128(uint8_t *input, uint64_t *adv)
{
  int64_t result;  // always ret int64, the caller will conv to i32 if required
  uint64_t shift, size;
  uint8_t byte;
  uint64_t ladv;

  result = 0;
  shift = 0;
  ladv = 0;
  size = 64;  // just a const to be eliminated at compile time

  do {
    byte = *input;
    input++;
    ++ladv;
    result |= (byte & 0x7f) << shift;
    shift += 7;

  } while (byte & 0x80);

  if ((shift < size) && (byte & 0x40)) { // 0x40 is the sign bit
    result |= (~0ull << shift);  // sign extend
  }

  if (shift >= size) {
    *adv = 0;
    return EHF_SLEB128_ERROR;
  }
  else {
    *adv = ladv;
    return result;
  }

}

uint64_t
decodeULEB128(uint8_t *input, uint64_t *adv)
{
  uint64_t result, shift, size;
  uint8_t byte;
  uint64_t ladv;

  result = 0;
  shift = 0;
  ladv = 0;
  size = 64;

  while (shift < (size-1))
  {
    byte = *input;
    input++;
    ++ladv;
    result |= (byte & 0x7f) << shift;
    if (!(byte & 0x80))
    {
      break;
    }
    shift += 7;
  }

  if (shift > (size-1))
  {
    *adv = 0;
    return EHF_ULEB128_ERROR;
  }
  else {
    *adv = ladv;
    return result;
  }
}

bool
fnb_doSectionScan(Elf *e, GElf_Shdr secHead, const char* xname)
{
  size_t secHeadStringIndex;
  char *secName;

  if (elf_getshdrstrndx(e, &secHeadStringIndex) != 0) {
    ETMSG(FNBOUNDS, "Warning, libelf error %s in scan check for %s",elf_errmsg(-1),xname);
    return false;
  }

  secName = elf_strptr(e, secHeadStringIndex, secHead.sh_name);
  if (secName == NULL) {
    ETMSG(FNBOUNDS, "Warning, libelf error %s in scan check for %s",elf_errmsg(-1),xname);
    return false;
  }

  TMSG(FNBOUNDS_EXT, "Processing functions from scanning %s instructions",secName);
  return true;
}

//
// return an address from a byte stream that might not be aligned, and where
// we don't know the endianness, and we can't assume unaligned access won't trap
// The caller might need a signed value, so we sign extend and cast based on "sign"
//
uint64_t
unalignedEndianRead(uint8_t *stream, size_t size, uint64_t sign)
{
  uint8_t *p;
  uint64_t r, i;
  uint64_t aligned;

  r = 0ull;
  p = (uint8_t *)&r;

  //
  // if the read is aligned for the size, just let the hw do the
  // endian bit. The stream is always LE.
  //
  aligned = !((uint64_t)stream & (size-1));

  if (aligned) {
    switch (size) {
      case sizeof(uint16_t):
        if (sign == EHF_UER_SIGNED) {
          r = (uint64_t)((int64_t) (* (int16_t *) stream));
        }
        else {
          r = (uint64_t) (* (uint16_t *) stream);
        }
        return r;
        break;
      case sizeof(uint32_t):
        if (sign == EHF_UER_SIGNED) {
          r = (uint64_t)((int64_t) (* (int32_t *) stream));
        }
        else {
          r = (uint64_t) (* (uint32_t *) stream);
        }
        return r;
        break;
      case sizeof(uint64_t):
        if (sign == EHF_UER_SIGNED) {
          r = (uint64_t)((int64_t) (* (int64_t *) stream));
        }
        else {
          r = * (uint64_t *) stream;
        }
        return r;
        break;
      //
      // if size was funky, just fall through without returning
      //
      default:
        break;
    }
  }

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

  for (i=0; i<size; i++) {
    *(p + i) = *(stream + i);
  }
  // sign extend if negative
  if (sign == EHF_UER_SIGNED) {
    if ( (*(p+i-1) & 0x80) != 0) {
      for (; i < sizeof(uint64_t); i++) {
        *(p + i) = 0xff;
      }
    }
  }

#else // big endian

#if EHF_STREAM_ORDER == EHF_STREAM_BE
  // this is big endian support for a big endian stream on BE core
  for (i=0; i<size; i++) {
    *(p + i + sizeof(uint64_t) - size) = *(stream + i);
  }
  if (sign == EHF_UER_SIGNED) {
    if ( (*(p+sizeof(uint64_t)-size) & 0x80) != 0) {
      for (i=0; i<(sizeof(uint64_t)-size); i++) {
        *(p + i) = 0xff;
      }
    }
  }

#else
  for (i=1; i<=size; i++) {
    *(p + (sizeof(uint64_t) - i)) = *(stream + i);
  }

  if (sign == EHF_UER_SIGNED) {
    if ( (*(p + (sizeof(uint64_t) - i)) & 0x80) != 0) {
      for (; i <= sizeof(uint64_t); i++) {
        *(p + (sizeof(uint64_t) - i)) = 0xff;
      }
    }
  }

#endif
#endif

  return r;
}
// scan the .plt section
//
bool
fnb_pltscan(FnboundsResponse* resp, Elf *e, GElf_Shdr secHead, const char* xname)
{
  uint64_t ii;
  uint64_t startAddr, endAddr, pltEntrySize;

  if (!fnb_doSectionScan(e, secHead, xname)) {
    return false;
  }

  startAddr = secHead.sh_addr;
  endAddr = startAddr + secHead.sh_size;
  pltEntrySize = secHead.sh_entsize;
  //
  // For a static build, even though the section contains trampolines,
  // the entry size may be zero.  If so, just skip the section for now.
  //
  if ( pltEntrySize == 0 ) {
    return true;
  }

  for (ii = startAddr + pltEntrySize; ii < endAddr; ii += pltEntrySize) {
    fnb_add_function(resp, (void*)ii);
  }

  return true;
}

// scan the .pltsec section
//
bool
fnb_pltsecscan(FnboundsResponse* resp, Elf *e, GElf_Shdr secHead, const char* xname)
{
  uint64_t ii;
  uint64_t startAddr, endAddr, pltEntrySize;

  if (!fnb_doSectionScan(e, secHead, xname)) {
    return false;
  }

  startAddr = secHead.sh_addr;
  endAddr = startAddr + secHead.sh_size;
  pltEntrySize = secHead.sh_entsize;
  //
  // For a static build, even though the section contains trampolines,
  // the entry size may be zero.  If so, just skip the section for now.
  //
  if ( pltEntrySize == 0 ) {
    return true;
  }

  for (ii = startAddr + pltEntrySize; ii < endAddr; ii += pltEntrySize) {
    fnb_add_function(resp, (void*)ii);
  }

  return true;
}

// scan the .init section
bool
fnb_initscan(FnboundsResponse* resp, Elf *e, GElf_Shdr secHead, const char* xname)
{
  if (!fnb_doSectionScan(e, secHead, xname)) {
    return false;
  }

  // NOT YET IMPLEMENTED
  TMSG(FNBOUNDS_EXT, "scanning .init instructions not yet implemented");
  return false;
}

// scan the .text section
bool
fnb_textscan(FnboundsResponse* resp, Elf *e, GElf_Shdr secHead, const char* xname)
{
  if (!fnb_doSectionScan(e, secHead, xname)) {
    return false;
  }

  // NOT YET IMPLEMENTED
  TMSG(FNBOUNDS_EXT, "scanning .text instructions not yet implemented");
  return false;
}

// scan the .fini section
bool
fnb_finiscan(FnboundsResponse* resp, Elf *e, GElf_Shdr secHead, const char* xname)
{
  if (!fnb_doSectionScan(e, secHead, xname)) {
    return false;
  }

  // NOT YET IMPLEMENTED
  TMSG(FNBOUNDS_EXT, "scanning .fini instructions not yet implemented");
  return false;
}

// scan the .fini section
bool
fnb_altinstr_replacementscan(FnboundsResponse* resp, Elf *e, GElf_Shdr secHead, const char* xname)
{
  if (!fnb_doSectionScan(e, secHead, xname)) {
    return false;
  }

  // NOT YET IMPLEMENTED
  TMSG(FNBOUNDS_EXT, "scanning .altinstr_replacement instructions not yet implemented");
  return false;
}

// scan the .eh_frame section
// For now, we will ignore the eh_frame_hdr, though it is possible to
// speed processing if it exists (it might not)
//
bool
fnb_ehframescan(FnboundsResponse* resp, Elf *e, ehRecord_t *ehRecord, const char* xname)
{
  GElf_Shdr ehSecHead,textSecHead,dataSecHead;
  Elf_Data *data;
  uint64_t recordOffset;
  uint64_t calculatedAddr64, calculatedAddrRange;
  uint64_t extra;
  uint8_t *pb;
  uint32_t *pw;
  uint32_t kc, kf, recType, recLen, cf, kk, curCIE;
  uint8_t cieVersion;
  char *augString;
  uint64_t codeAlign;
  int64_t  dataAlign;
  uint8_t fdeEnc;
  uint64_t augDataLen;
  uint64_t cieOffset, fdeOffset, a, b;
  uint64_t dataSize;
  uint64_t cieTableSize, cieAddress;
  ehCIERecord_t *cieTable;
  ehDecodeRecord_t decodeRecord;
  ehCIERecord_t *fdeRefCIE;
  uint64_t personalityFunctionAddr, landingFunctionAddr;
  static char elfFailMess[] = {"Error in eh_frame handling, aborting scan.  libelf failed with "};


  // don't process non-existent section
  if (ehRecord->ehFrameSection == NULL) {
    return false;
  }

  if (gelf_getshdr(ehRecord->ehFrameSection, &ehSecHead) != &ehSecHead) {
    ETMSG(FNBOUNDS, "%s %s", elfFailMess, elf_errmsg(-1));
    return false;
  }

  if (!fnb_doSectionScan(e, ehSecHead, xname)) {
    return false;
  }
  //
  // we may need these depending on the FDE encode field.
  // the other parts of decodeRecord must be set before each
  // call to decodeDwarfAddress.
  //
  decodeRecord.textSecAddr = 0ull;
  if (ehRecord->textSection != NULL) {
    if (gelf_getshdr(ehRecord->textSection, &textSecHead) != &textSecHead) {
      ETMSG(FNBOUNDS, "%s %s", elfFailMess, elf_errmsg(-1));
      return false;
    }
    decodeRecord.textSecAddr = textSecHead.sh_addr;
  }

  decodeRecord.dataSecAddr = 0ull;
  if (ehRecord->dataSection != NULL) {
    if (gelf_getshdr(ehRecord->dataSection, &dataSecHead) != &dataSecHead) {
      ETMSG(FNBOUNDS, "%s %s", elfFailMess, elf_errmsg(-1));
      return false;
    }
    decodeRecord.dataSecAddr = dataSecHead.sh_addr;
  }

  decodeRecord.ehSecAddr = ehSecHead.sh_addr;

  data = NULL;
  data = elf_getdata(ehRecord->ehFrameSection,data);
  if (data == NULL) {
    ETMSG(FNBOUNDS, "%s %s", elfFailMess, elf_errmsg(-1));
    return false;
  }

  dataSize = data->d_size;  // for clarity later
  if ((dataSize == 0) || (data->d_buf == NULL)) {
    return false;
  }

  kc = 0; // cie count
  kf = 0; // fde count
  cf = EHF_CF_CONT; // continue flag

  // allocate a cieTable
  cieTableSize = EHF_MAX_CIE;
  cieTable = (ehCIERecord_t *) malloc (cieTableSize * sizeof(ehCIERecord_t));
  if (cieTable == NULL) {
    ETMSG(FNBOUNDS, "Fatal error in eh_frame handler, cannot allocate cieTable.  Aborting.");
    return false;
  }

  recordOffset = 0;

  pb = (uint8_t *)(data -> d_buf);

  extra = sizeof(*pw);  // true because this is the sizeof recLen w.out extension

  //
  // read through the eh_frame until the terminating record is reached.
  // we assume each group of FDE reference the previous CIE; so only the
  // most recent fdeEnc is used.  there's at least 1 CIE required.
  //
  do {
    pw = (uint32_t *)pb;  // convenience to read words, and get rid of endian issue
    recLen = *pw;


    // support extended record lengths here if required
    if (recLen == EHF_CIE_EXTREC) {
      ETMSG(FNBOUNDS, "Fatal error in eh_frame handling, extended records not supported, aborting");
      return false;
    }

    //
    // This is the standard termination of the eh_frame scan
    //
    if (recLen == 0ul) {
      cf = EHF_CF_DONE;
      continue;
    }

    // record offsets may be defined in Dwarf, but I didn't find them, so redef'd in the dot-h
    recType = *(pw + EHF_WO_TYPE);

    //
    // if this is a CIE, we need to read through all the variable length records
    // in order to find the fdeEnc.  That's why we use cieOffset and a...  We don't actually need
    // some of the values here, but we have to decode them in order to find our place in the
    // byte stream, then let the optimizer throw them out.
    //
    if (recType == EHF_TP_CIE) {
      cieTable[kc].cieBaseAddress = recordOffset;
      cieTable[kc].cieSize = recLen + extra;

      cieVersion = *(pb + EHF_CIE_BO_VER);
      augString = (char *) (pb + EHF_CIE_BO_AUSTR);

      //
      // reject any augStrings generated by old compilers, or some other unknown condition
      // set the cieSize to 0, and skip any FDE associated with it.
      //
      if (augString[0] != 'z') {
        ETMSG(FNBOUNDS, "Warning in eh_frame handling, unsupported augmentation string %s found in %s",
            augString, xname);
        cieTable[kc].cieSize = 0;
        goto nextRecord;
      }

      //
      // cieOffset, in bytes, is used to traverse the CIE where numbers are variable length
      //
      cieOffset = EHF_CIE_BO_AUSTR + strlen(augString) + 1; // Extra byte for null terminator

      codeAlign = decodeULEB128(pb+cieOffset, &a);
      if (codeAlign == EHF_ULEB128_ERROR) {
        cieTable[kc].cieSize = 0;
        goto nextRecord;
      }
      cieOffset += a;
      dataAlign = decodeSLEB128(pb+cieOffset, &a);
      if (dataAlign == EHF_SLEB128_ERROR) {
        cieTable[kc].cieSize = 0;
        goto nextRecord;
      }
      cieOffset += a;

      if (cieVersion == EHF_CIE_VER_1) {
        ++cieOffset;
      }
      else if (cieVersion == EHF_CIE_VER_3) {
        uint64_t value = decodeULEB128(pb+cieOffset, &a);
        cieOffset += a;
        if (value == EHF_ULEB128_ERROR) {
          cieTable[kc].cieSize = 0;
          goto nextRecord;
        }
      }
      else {
        ETMSG(FNBOUNDS, "Warning in eh_frame handling, unsupported CIE version %d found in %s",
            (uint32_t)cieVersion, xname);
        cieTable[kc].cieSize = 0;
        goto nextRecord;
      }

      augDataLen = decodeULEB128(pb+cieOffset, &a);
      if (augDataLen == EHF_ULEB128_ERROR) {
        cieTable[kc].cieSize = 0;
        goto nextRecord;
      }
      cieOffset += a;

      //
      // Now process each byte of the augData according to the augString
      //
      cieTable[kc].cieContainsType.R = EHF_CIE_FALSE;
      cieTable[kc].cieContainsType.P = EHF_CIE_FALSE;
      cieTable[kc].cieContainsType.L = EHF_CIE_FALSE;
      cieTable[kc].cieAddressOffset = 0ull;  // not an S, in other words

      // b starts at 1 to skip the 'z'
      b = 1;
      while ( augString[b] != '\0' ) {
        switch (augString[b]) {
          case 'P':
            cieTable[kc].cieContainsType.P = EHF_CIE_TRUE;
            fdeEnc = *(pb+cieOffset);
            ++cieOffset;
            cieTable[kc].cieFDEEncType.P = fdeEnc;
            cieTable[kc].cieAugDataOffset = recordOffset + cieOffset;
            cieTable[kc].cieAugData = pb+cieOffset;
            //
            // in order to advance cieOffset with the length of the aug record,
            // we have to make a dummy call to decode, which will give us
            // an error condition if one exists, and the offset
            //
            decodeRecord.totalOffset = recordOffset + cieOffset;
            decodeRecord.fdeEnc = fdeEnc;
            if (decodeDwarfAddress(pb+cieOffset, &decodeRecord, &a, "personality dummy") == EHF_DECDWRF_ERROR) {
              goto nextRecord;
            }
            cieOffset += a;
            ++b;
            break;
          case 'L':
            cieTable[kc].cieContainsType.L = EHF_CIE_TRUE;
            fdeEnc = *(pb+cieOffset);
            cieTable[kc].cieFDEEncType.L = fdeEnc;
            ++cieOffset;
            ++b;
            break;
          case 'R':
            cieTable[kc].cieContainsType.R = EHF_CIE_TRUE;
            fdeEnc = *(pb+cieOffset);
            cieTable[kc].cieFDEEncType.R = fdeEnc;
            ++cieOffset;
            ++b;
            break;
          case 'S':
            //
            // non-zero stack offset, see the dot-h
            //
            cieTable[kc].cieAddressOffset = EHF_FORWARD_OFF;
            ++b;
            break;
          //
          // This is an error condition, and should never happen
          //
          default:
            ETMSG(FNBOUNDS, "Error in eh_frame handling, unsupported augmentation character %c found in %s",
                augString[b],  xname);
            cieTable[kc].cieSize = 0;
            goto nextRecord;
            break;
        } // !switch
      } // !while

      //
      // the cie count is also the index into the cieTable, that is the max
      // defined CIE.  If the below condition happens, we need to increase the size of
      // the table.  This seems unlikely, but if it happens it might be a good idea to
      // increase EHF_MAX_CIE so there is no realloc here.
      //
      ++kc;
      if (kc >= cieTableSize) {
        cieTableSize += EHF_MAX_CIE;
        cieTable = (ehCIERecord_t *) realloc ((void *)cieTable, cieTableSize * sizeof(ehCIERecord_t));
        if (cieTable == NULL) {
          ETMSG(FNBOUNDS, "Fatal error in eh_frame handler, cannot re-allocate cieTable at size %ld.  Aborting.",
              cieTableSize);
          return false;
        }
      }

    }  // ! recType is a CIE

    else {  // process FDE

      ++kf; // bump count

      //
      // determine to which CIE this FDE belongs. recType contains the offset to the CIE address
      // from that point in the stream.  Since we don't expect a huge amount of CIEs, just leaf
      // through them to find the right one.  (In theory the actual CIE should be near the last
      // one used in the list, this would be a performance enhancement).
      //
      cieAddress = recordOffset + sizeof(recLen) - recType;

      for (kk = 0; kk < kc; kk++) {
        if ( cieTable[kk].cieBaseAddress == cieAddress ) {
          curCIE = kk;
          break;
        }
      }

      //
      // Fatal error if the CIE wasn't found
      //
      if (kk == kc) {
        ETMSG(FNBOUNDS, "Fatal error in eh_frame handling, FDE without associated CIE in %s", xname);
        return false;
      }

      fdeRefCIE = &cieTable[curCIE];

      //
      // verify that the CIE parsed correctly
      //
      if ((fdeRefCIE -> cieSize) == 0) {
        ETMSG(FNBOUNDS, "Warning in eh_frame handling, FDE references unparsed CIE 0x%lx in %s, skipping",
            fdeRefCIE -> cieBaseAddress, xname);
        goto nextRecord;
      }

      fdeOffset = EHF_WO_FS*sizeof(*pw);

      //
      // grab the personality function if there's a 'P'. This comes from the reference CIE and doesn't
      // change any of the FDE offsets
      //
      if ( (fdeRefCIE -> cieContainsType.P) == EHF_CIE_TRUE) {
        decodeRecord.totalOffset = fdeRefCIE -> cieAugDataOffset;
        decodeRecord.fdeEnc = fdeRefCIE -> cieFDEEncType.P;

        personalityFunctionAddr = decodeDwarfAddress(fdeRefCIE->cieAugData, &decodeRecord, &a,
            "personality function");

        //
        // now add the personality function to the list
        // note that since the personality address comes from the cie,
        // we don't increase fdeOffset
        //
        if (personalityFunctionAddr != EHF_DECDWRF_ERROR) {
          fnb_add_function(resp, (void*)personalityFunctionAddr);
        }

      }

      //
      // pick off the actual function address, if there's an 'R'
      //
      if ( (fdeRefCIE -> cieContainsType.R) == EHF_CIE_TRUE) {
        decodeRecord.fdeEnc = fdeRefCIE -> cieFDEEncType.R;
        decodeRecord.totalOffset = recordOffset+fdeOffset;

        calculatedAddr64 = decodeDwarfAddress(pb+fdeOffset, &decodeRecord, &a, "FDE function address");

        //
        // add the function address to the list
        //
        if (calculatedAddr64 != EHF_DECDWRF_ERROR) {
          calculatedAddr64 += fdeRefCIE -> cieAddressOffset; // adjust pointer if S
          fnb_add_function(resp, (void*)calculatedAddr64);
        }
        else {
          goto nextRecord;
        }
        fdeOffset += a;

        //
        // we have to decode the function range, even though it's not used, to get the offset
        // to the LDSA function, if one is present.  The fdeEnc is the same
        //
        decodeRecord.totalOffset = recordOffset+fdeOffset;

        calculatedAddrRange = decodeDwarfAddress(pb+fdeOffset, &decodeRecord, &a, "FDE function range");
        if (calculatedAddrRange == EHF_DECDWRF_ERROR) {
          goto nextRecord;
        }
        fdeOffset += a;

      } // !R

      if ( (fdeRefCIE -> cieContainsType.L) == EHF_CIE_TRUE) {
        augDataLen = decodeULEB128(pb+fdeOffset, &a);
        if (augDataLen == EHF_ULEB128_ERROR) {
          ETMSG(FNBOUNDS, "Warning in eh_frame handling, cannot decode LSDA aug data len in %s", xname);
          goto nextRecord;
        }
        fdeOffset += a;
        fdeEnc = fdeRefCIE -> cieFDEEncType.L;
        if (fdeEnc != DW_EH_PE_omit) {
          decodeRecord.fdeEnc = fdeEnc;
          decodeRecord.totalOffset = recordOffset+fdeOffset;
          landingFunctionAddr = decodeDwarfAddress(pb+fdeOffset, &decodeRecord, &a, "LSDA function");
          //
          // add the lsda function
          //
          if (landingFunctionAddr != EHF_DECDWRF_ERROR) {
            fnb_add_function(resp, (void*)landingFunctionAddr);
          }
          fdeOffset += a;

        }
      } // !L

    } // ! recType is an FDE

    //
    // at this point, we don't care about the dwarf instructions in the rest
    // of the record, so we can simply skip to the next record.  This is also
    // the place we go if there was a non-critical error that cause us to skip
    // a frame.
    //

nextRecord:

    pb += (recLen + extra);
    recordOffset +=  (recLen + extra);
    //
    // corner case where the last FDE is at the end of the section
    //
    if (recordOffset >= dataSize) {
      cf = EHF_CF_DONE;
    }

  } while (cf == EHF_CF_CONT);  // ! traversal of eh_frame records

  free ( cieTable );

  TMSG(FNBOUNDS_EXT, "scanning .eh_frame, found %d CIE and %d FDE records", kc, kf);

  return true;
}
