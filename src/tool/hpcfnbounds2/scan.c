// -*-Mode: C++;-*-

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
// Copyright ((c)) 2002-2020, Rice University
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
// This file contains the routines that scan instructions to identify functions

#include <fnbounds.h>


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

uint64_t
skipSectionScan(Elf *e, GElf_Shdr secHead, int secFlag)
{
  size_t secHeadStringIndex;
  char *secName;
  Elf_Scn *section;

  elf_getshdrstrndx(e, &secHeadStringIndex);
  secName = elf_strptr(e, secHeadStringIndex, secHead.sh_name);

  if (secFlag == SC_SKIP) {
    if(verbose) {
      fprintf (stderr, "Skipping scanning %s instructions\n",secName);
    }
    return SC_SKIP;
  } else {
    if(verbose) {
      fprintf (stderr, "Processing functions from scanning %s instructions\n",secName);
    }
  }

  return SC_DONE;
}

// scan the .plt section
//
uint64_t
pltscan(Elf *e, GElf_Shdr secHead)
{
  uint64_t ii;
  uint64_t startAddr, endAddr, pltEntrySize;
  char nameBuff[TB_SIZE];
  char *vegamite;

  if (skipSectionScan(e, secHead, pltscan_f) == SC_SKIP) {
    return SC_SKIP;
  }
  
  startAddr = secHead.sh_addr;
  endAddr = startAddr + secHead.sh_size;
  pltEntrySize = secHead.sh_entsize;

  for (ii = startAddr + pltEntrySize; ii < endAddr; ii += pltEntrySize) {
    sprintf(nameBuff,"stripped_0x%lx",ii);
    vegamite = strdup(nameBuff);
    add_function(ii, vegamite, "p",FR_YES);
  }

  return SC_DONE;
}

// scan the .init section
uint64_t
initscan(Elf *e, GElf_Shdr secHead)
{
  if (skipSectionScan(e, secHead, initscan_f) == SC_SKIP) {
    return SC_SKIP;
  }
  
  // NOT YET IMPLEMENTED
  if(verbose) {
    fprintf (stderr, "\tscanning .init instructions not yet implemented\n");
  }
  // use "i" as source string in add_function() calls
}

// scan the .text section
uint64_t
textscan(Elf *e, GElf_Shdr secHead)
{
  if (skipSectionScan(e, secHead, textscan_f) == SC_SKIP) {
    return SC_SKIP;
  }
  
  // NOT YET IMPLEMENTED
  if(verbose) {
    fprintf (stderr, "\tscanning .text instructions not yet implemented\n");
  }
  // use "t" as source string in add_function() calls
}

// scan the .fini section
uint64_t
finiscan(Elf *e, GElf_Shdr secHead)
{
  if (skipSectionScan(e, secHead, finiscan_f) == SC_SKIP) {
    return SC_SKIP;
  }
  
  // NOT YET IMPLEMENTED
  if(verbose) {
    fprintf (stderr, "\tscanning .fini instructions not yet implemented\n");
  }
  // use "f" as source string in add_function() calls
}

// scan the .fini section
uint64_t
altinstr_replacementscan(Elf *e, GElf_Shdr secHead)
{
  if (skipSectionScan(e, secHead, altinstr_replacementscan_f) == SC_SKIP) {
    return SC_SKIP;
  }
  
  // NOT YET IMPLEMENTED
  if(verbose) {
    fprintf (stderr, "\tscanning .altinstr_replacement instructions not yet implemented\n");
  }
  // use "a" as source string in add_function() calls
}

// scan the .eh_frame section
// For now, we will ignore the eh_frame_hdr, though it is possible to
// speed processing if it exists (it might not)
//
uint64_t
ehframescan(Elf *e, ehRecord_t *ehRecord)
{
  GElf_Shdr ehSecHead,textSecHead,dataSecHead;
  Elf_Data *data;
  int32_t signedBaseAddr32;
  uint64_t unsignedBaseAddr64;
  int32_t signedRelAddr32;
  int32_t signedStreamOffset;
  uint64_t recordOffset;
  uint64_t calculatedAddr64;
  uint32_t extra;
  uint8_t *pb;
  uint32_t *pw;
  uint64_t kc, kf, recType, recLen, cf;
  uint8_t fdeAddrDecodeType, fdeAddrOpType;
  char nameBuff[TB_SIZE];
  char *promite;  // mmmm
  uint8_t cieVersion;
  char *augString;
  uint64_t codeAlign;
  int64_t  dataAlign;
  uint8_t retReg;
  uint8_t fdeEnc;
  uint64_t augDataLen;
  uint64_t cieOffset, a;
  uint8_t *upt8;
  int64_t signedRelAddr64;
  uint64_t unsignedRelAddr64;


  gelf_getshdr(ehRecord->ehFrameSection, &ehSecHead);

  if (skipSectionScan(e, ehSecHead, ehframeread_f) == SC_SKIP) {
    return SC_SKIP;
  }
  //
  // we may need these depending on the FDE encode field
  //
  gelf_getshdr(ehRecord->textSection, &textSecHead);
  gelf_getshdr(ehRecord->dataSection, &dataSecHead);

  data = NULL;
  data = elf_getdata(ehRecord->ehFrameSection,data);

  // rethink how this is done.  per fdeEnc method?
  signedBaseAddr32 = (int32_t)ehSecHead.sh_addr;  //remove this FIXME

  kc = 0; // cie count
  kf = 0; // fde count
  cf = EHF_CF_CONT; // continue flag

  signedStreamOffset = 0;
  recordOffset = 0;

  pb = (uint8_t *)(data -> d_buf);

  extra = sizeof(*pw);  // FIXME: is this true for all addrDecodeType?
  // addrDecodeType = DW_EH_PE_absptr;  // FIXME: get this from the CIE

  //
  // read through the eh_frame until the terminating record is reached.
  // we assume each group of FDE reference the previous CIE; so only the
  // most recent fdeEnc is used.  there's at least 1 CIE required.
  //
  do {
    pw = (uint32_t *)pb;  // convenience to read words, and get rid of endian issue
    recLen = *pw;

    // support extended record lengths here
    if (recLen == 0xfffffffful) {
      fprintf(stderr, "Error in eh_frame handling, extended records not supported\n");
      return SC_SKIP;
    }

    if (recLen == 0ull) {
      cf = EHF_CF_DONE;
      continue;
    }

    // record offsets may be defined in Dwarf, but I didn't find them, so redef'd in the dot-h
    recType = *(pw + EHF_WO_TYPE);

    //
    // if this is a CIE, we need to read through all the variable length records 
    // in order to find the fdeEnc.  That's why we do a,b,c,d,e...  We don't actually need 
    // some of the values here, but we have to decode them in order to find our place in the
    // byte stream, then let the optimizer throw them out.
    // 
    if (recType == EHF_TP_CIE) {
      ++kc;
      cieVersion = *(pb + EHF_CIE_BO_VER);
      augString = pb + EHF_CIE_BO_AUSTR;

      //
      // we only support zR data. FIXME skip all FDEs not associated with zR?  better
      //
      if (strcmp((const char *)augString,"zR")) {
        fprintf(stderr, "Error in eh_frame handling, unsupported augmentation string %s found\n",augString);
        return SC_SKIP;
      }

      // cieOffset is used to traverse the CIE where numbers are variable length
      //
      cieOffset = EHF_CIE_BO_AUSTR + strlen(augString) + 1; // Extra byte for null terminator

      codeAlign = decodeULEB128(pb+cieOffset, &a);
      cieOffset += a;
      dataAlign = decodeSLEB128(pb+cieOffset, &a);
      cieOffset += a;

      if (cieVersion == EHF_CIE_VER_1) {
        retReg = *(pb+cieOffset);
        ++cieOffset;
      }
      else if (cieVersion == EHF_CIE_VER_3) {
        retReg = (uint8_t) decodeULEB128(pb+cieOffset, &a);
        cieOffset += a;
      }
      else {
        // FIXME error message?
        return SC_SKIP;
      }

      augDataLen = decodeULEB128(pb+cieOffset, &a);
      cieOffset += a;

      fdeEnc = *(pb+cieOffset);
      ++cieOffset;  // should we need to read more...

      fdeAddrDecodeType = fdeEnc & 0x0f;
      fdeAddrOpType = fdeEnc & 0xf0;
    }

    else {
      ++kf; // bump count

      switch(fdeAddrOpType) {
        case DW_EH_PE_absptr:
          unsignedBaseAddr64 = 0ull;
          break;
        case DW_EH_PE_pcrel:
          unsignedBaseAddr64 = ehSecHead.sh_addr;  // native uint64_t
          unsignedBaseAddr64 += (recordOffset + EHF_WO_FS*sizeof(*pw));
          break;
        case DW_EH_PE_textrel:
          unsignedBaseAddr64 = textSecHead.sh_addr; 
          break;
        case DW_EH_PE_datarel:
          unsignedBaseAddr64 = dataSecHead.sh_addr; 
          break;
        //
        // funcrel is for non-contiguous functions where a single function
        // is split over multiple cie/fde groups. This is not supported here.
        // Aligned may apply to changing some natural data sized addresses?
        //
        case DW_EH_PE_funcrel:
        case DW_EH_PE_aligned:
        default:
          fprintf(stderr, "Error in eh_frame handling, unsupported address operation type %.2x\n",fdeAddrOpType);
          return SC_SKIP;
          break;
      }

      switch (fdeAddrDecodeType) {
        case DW_EH_PE_absptr:
          calculatedAddr64 = * (uint64_t *) (pw+EHF_WO_FS);  // abs 64 bit addr
          break;
        case DW_EH_PE_uleb128:
          upt8 = (uint8_t *)(pw+EHF_WO_FS);
          unsignedRelAddr64 = decodeULEB128(upt8, &a);
          calculatedAddr64 = unsignedBaseAddr64 + unsignedRelAddr64;
          break;
        case DW_EH_PE_udata2:
          unsignedRelAddr64 = (uint64_t) (* (uint16_t *) (pw+EHF_WO_FS));
          calculatedAddr64 = unsignedBaseAddr64 + unsignedRelAddr64;
          break;
        case DW_EH_PE_udata4:
          unsignedRelAddr64 = (uint64_t) (* (uint32_t *) (pw+EHF_WO_FS));
          calculatedAddr64 = unsignedBaseAddr64 + unsignedRelAddr64;
          break;
        case DW_EH_PE_udata8:
          unsignedRelAddr64 = (uint64_t) (* (uint64_t *) (pw+EHF_WO_FS));
          calculatedAddr64 = unsignedBaseAddr64 + unsignedRelAddr64;
          break;
          //
          // if the relative part is signed, we assume the base has
          // the high bit clear (i.e. non-negative for signed arithmetic).
          // This should always be so in user space.  If it's not, we need a
          // special addition routine that does negate subtract of the addend.
          //
        case DW_EH_PE_sleb128:
          upt8 = (uint8_t *)(pw+EHF_WO_FS);
          signedRelAddr64 = decodeSLEB128(upt8, &a);
          calculatedAddr64 = (uint64_t)((int64_t)unsignedBaseAddr64 + signedRelAddr64);
          break;
        case DW_EH_PE_sdata2:
          signedRelAddr64 = (int64_t) (* (int16_t *) (pw+EHF_WO_FS));
          calculatedAddr64 = (uint64_t)((int64_t)unsignedBaseAddr64 + signedRelAddr64);
        case DW_EH_PE_sdata4:
          signedRelAddr32 = (int32_t)*(pw + EHF_WO_FS);
          calculatedAddr64 = (uint64_t)((int64_t)unsignedBaseAddr64 + (int64_t)signedRelAddr32);
          break;
        case DW_EH_PE_sdata8:
          signedRelAddr64 = (int64_t) (* (int64_t *) (pw+EHF_WO_FS));
          calculatedAddr64 = (uint64_t)((int64_t)unsignedBaseAddr64 + signedRelAddr64);
        default:
          fprintf(stderr, "Error in eh_frame handling, unsupported address decode type %.2x\n",fdeAddrDecodeType);
          return SC_SKIP;
          break;
      }

      sprintf(nameBuff,"stripped_0x%lx",calculatedAddr64);
      promite = strdup(nameBuff);
      add_function(calculatedAddr64, promite, "e", FR_YES);
    }

    pb += (recLen + extra);
    recordOffset +=  (recLen + extra);
  } while (cf == EHF_CF_CONT);


  if(verbose) {
    fprintf (stderr, "scanning .eh_frame, found %ld CIE and %ld FDE records\n", kc, kf);
    }
  
  return SC_DONE;
}

