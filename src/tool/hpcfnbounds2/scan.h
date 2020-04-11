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
// scan.h - program to print the function list from its load-object arguments

#ifndef _FNBOUNDS_SCAN_H_
#define _FNBOUNDS_SCAN_H_


#include  <stdio.h>
#include  <stdlib.h>
#include  <unistd.h>
#include  <string.h>
#include  <elf.h>
#include  <libelf.h>
#include  <gelf.h>
#include 	<dwarf.h>

typedef struct __ehRecord {
  Elf_Scn   *ehHdrSection;
  Elf_Scn   *ehFrameSection;
  Elf_Scn   *textSection;
  Elf_Scn   *dataSection; 
  size_t    ehHdrIndex;
  size_t    ehFrameIndex;
} ehRecord_t;

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



// prototypes
uint64_t	pltscan(Elf *e, GElf_Shdr sh);
uint64_t	initscan(Elf *e, GElf_Shdr sh);
uint64_t	textscan(Elf *e, GElf_Shdr sh);
uint64_t	finiscan(Elf *e, GElf_Shdr sh);
uint64_t	altinstr_replacementscan(Elf *e, GElf_Shdr sh);
uint64_t	ehframescan(Elf *e, ehRecord_t *ehRecord);
uint64_t 	skipSectionScan(Elf *e, GElf_Shdr secHead, int secFlag);
uint64_t  decodeULEB128(uint8_t *input, uint64_t *sizeInBytes);
int64_t   decodeSLEB128(uint8_t *input, uint64_t *sizeInBytes);
uint64_t  decodeDwarfAddress(uint8_t *, ehDecodeRecord_t *, uint64_t *, char *);
uint64_t  unalignedEndianRead(uint8_t *, size_t, uint64_t);


// Defines 
#define EHF_MAX_CIE       (64)
#define EHF_CIE_TRUE      (1)
#define EHF_CIE_FALSE     (0)
#define EHF_CIE_EXTREC    (0xfffffffful)
#define EHF_FDE_DMASK     (0x0f)
#define EHF_FDE_OMASK     (0xf0)
#define EHF_CF_DONE	      (0)
#define EHF_CF_CONT	      (1)
#define EHF_WO_TYPE	      (1)
#define EHF_WO_FS	        (2)
#define EHF_TP_CIE	      (0)
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

#endif  // _FNBOUNDS_SCAN_H_
