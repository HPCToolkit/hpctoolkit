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
#include <scan.h>
#include <endian.h>

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
    fprintf(stderr, "Warning in eh_frame %s, omit found\n",errString);
    return EHF_DECDWRF_ERROR;
  }

  unsignedBaseAddr = 0ull;
  unsignedFunctionAddr = 0ull;
  advance = 0;

	switch(fdeAddrOpType) {
		case DW_EH_PE_absptr:
			unsignedBaseAddr = 0ull;
		case DW_EH_PE_pcrel:
		case DW_EH_PE_indirect:		// personality only
		case DW_EH_PE_indpcrel:		// personality only
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
			fprintf(stderr, "Error in eh_frame %s, unsupported address operation type %.2x\n",
					errString, fdeAddrOpType);
      *adv = 0;
			return EHF_DECDWRF_ERROR;
			break;
	}

	switch (fdeAddrDecodeType) {
		case DW_EH_PE_absptr:
      unsignedFunctionAddr = unalignedEndianRead(streamPtr,sizeof(uint64_t),EHF_UER_UNSIGNED);
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
			fprintf(stderr, "Error in eh_frame %s, unsupported address decode type %.2x\n",
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

uint64_t
skipSectionScan(Elf *e, GElf_Shdr secHead, int secFlag)
{
  size_t secHeadStringIndex;
  char *secName;

  if (elf_getshdrstrndx(e, &secHeadStringIndex) != 0) {
    fprintf (stderr, "Warning, libelf error %s in scan check for %s\n",elf_errmsg(-1),xname);
    return SC_SKIP;
  }

  secName = elf_strptr(e, secHeadStringIndex, secHead.sh_name);
  if (secName == NULL) {
    fprintf (stderr, "Warning, libelf error %s in scan check for %s\n",elf_errmsg(-1),xname);
    return SC_SKIP;
  }

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
  //
  // For a static build, even though the section contains trampolines,
  // the entry size may be zero.  If so, just skip the section for now.
  //
  if ( pltEntrySize == 0 ) {
    return SC_DONE;
  }

  for (ii = startAddr + pltEntrySize; ii < endAddr; ii += pltEntrySize) {
    sprintf(nameBuff,"stripped_0x%lx",ii);
    vegamite = strdup(nameBuff);
    add_function(ii, vegamite, SC_FNTYPE_PLT, FR_YES);
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
  // use SC_FNTYPE_INIT as source string in add_function() calls
  return SC_SKIP;
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
  // use SC_FNTYPE_TEXT as source string in add_function() calls
  return SC_SKIP;
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
  // use SC_FNTYPE_FINI as source string in add_function() calls
  return SC_SKIP;
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
  // use SC_FNTYPE_ALTINSTR as source string in add_function() calls
  return SC_SKIP;
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
  uint64_t recordOffset;
  uint64_t calculatedAddr64, calculatedAddrRange;
  uint64_t extra;
  uint8_t *pb;
  uint32_t *pw;
  uint32_t kc, kf, recType, recLen, cf, kk, curCIE;
  char nameBuff[TB_SIZE];
  char *promite;  // mmmm
  uint8_t cieVersion;
  char *augString;
  uint64_t codeAlign;
  int64_t  dataAlign;
  uint8_t retReg;
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


  // don't process non-existant section
  if (ehRecord->ehFrameSection == NULL) {
    return SC_SKIP;
  }

  if (gelf_getshdr(ehRecord->ehFrameSection, &ehSecHead) != &ehSecHead) {
    fprintf(stderr, "%s %s\n", elfFailMess, elf_errmsg(-1));
    return SC_SKIP;
  }
    
  if (skipSectionScan(e, ehSecHead, ehframeread_f) == SC_SKIP) {
    return SC_SKIP;
  }
  //
  // we may need these depending on the FDE encode field.
  // the other parts of decodeRecord must be set before each 
  // call to decodeDwarfAddress.
  //
  decodeRecord.textSecAddr = 0ull;
  if (ehRecord->textSection != NULL) {
    if (gelf_getshdr(ehRecord->textSection, &textSecHead) != &textSecHead) {
      fprintf(stderr, "%s %s\n", elfFailMess, elf_errmsg(-1));
      return SC_SKIP;
    }
    decodeRecord.textSecAddr = textSecHead.sh_addr;
  }

  decodeRecord.dataSecAddr = 0ull;
  if (ehRecord->dataSection != NULL) {
    if (gelf_getshdr(ehRecord->dataSection, &dataSecHead) != &dataSecHead) {
      fprintf(stderr, "%s %s\n", elfFailMess, elf_errmsg(-1));
      return SC_SKIP;
    }
    decodeRecord.dataSecAddr = dataSecHead.sh_addr;
  }

  decodeRecord.ehSecAddr = ehSecHead.sh_addr;

  data = NULL;
  data = elf_getdata(ehRecord->ehFrameSection,data);
  if (data == NULL) {
    fprintf(stderr, "%s %s\n", elfFailMess, elf_errmsg(-1));
    return SC_SKIP;
  }

  dataSize = data->d_size;  // for clarity later
  if ((dataSize == 0) || (data->d_buf == NULL)) {
    return SC_SKIP;
  }

  kc = 0; // cie count
  kf = 0; // fde count
  cf = EHF_CF_CONT; // continue flag

  // allocate a cieTable
  cieTableSize = EHF_MAX_CIE;
  cieTable = (ehCIERecord_t *) malloc (cieTableSize * sizeof(ehCIERecord_t));
  if (cieTable == NULL) {
    fprintf(stderr, "Fatal error in eh_frame handler, cannot allocate cieTable.  Aborting.\n");
    return SC_SKIP;
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
      fprintf(stderr, "Fatal error in eh_frame handling, extended records not supported, aborting\n");
      return SC_SKIP;
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
        fprintf(stderr, "Warning in eh_frame handling, unsupported augmentation string %s found in %s\n",
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
        retReg = *(pb+cieOffset);
        ++cieOffset;
      }
      else if (cieVersion == EHF_CIE_VER_3) {
        retReg = (uint8_t) decodeULEB128(pb+cieOffset, &a);
        cieOffset += a;
      }
      else {
        fprintf(stderr, "Warning in eh_frame handling, unsupported CIE version %d found in %s\n",
            (uint32_t)cieVersion, xname);
        cieTable[kc].cieSize = 0;
        goto nextRecord;
      }
      if (retReg == EHF_ULEB128_ERROR) {
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
            // in order to advence cieOffset with the length of the aug record,
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
            // non-zero stack offet, see the dot-h
            //
            cieTable[kc].cieAddressOffset = EHF_FORWARD_OFF;
            ++b;
            break;
          //
          // This is an error condition, and should never happen
          //
          default:
            fprintf(stderr, "Error in eh_frame handling, unsupported augmentation character %c found in %s\n",
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
          fprintf(stderr, "Fatal error in eh_frame handler, cannot re-allocate cieTable at size %ld.  Aborting.\n",
              cieTableSize);
          return SC_SKIP;
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
        fprintf(stderr, "Fatal error in eh_frame handling, FDE without associated CIE in %s\n", xname);
        return SC_SKIP;
      }

      fdeRefCIE = &cieTable[curCIE];

      // 
      // verify that the CIE parsed correctly
      //
      if ((fdeRefCIE -> cieSize) == 0) {
        fprintf(stderr, "Warning in eh_frame handling, FDE references unparsed CIE 0x%lx in %s, skipping\n",
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
          sprintf(nameBuff,"personality_0x%lx", personalityFunctionAddr);
          promite = strdup(nameBuff);
          add_function(personalityFunctionAddr, promite, SC_FNTYPE_EH_FRAME, FR_YES);
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
          sprintf(nameBuff,"stripped_0x%lx",calculatedAddr64);
          promite = strdup(nameBuff);
          add_function(calculatedAddr64, promite, SC_FNTYPE_EH_FRAME, FR_YES);
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
          fprintf(stderr, "Warning in eh_frame handling, cannot decode LSDA aug data len in %s\n", xname);
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
            sprintf(nameBuff,"LSDA_0x%lx",landingFunctionAddr);
            promite = strdup(nameBuff);
            add_function(landingFunctionAddr, promite, SC_FNTYPE_EH_FRAME, FR_YES);
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

  if(verbose) {
    fprintf (stderr, "scanning .eh_frame, found %d CIE and %d FDE records\n", kc, kf);
    }
  
  return SC_DONE;
}


