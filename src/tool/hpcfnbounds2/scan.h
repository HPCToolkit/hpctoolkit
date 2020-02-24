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

// prototypes
uint64_t	pltscan(Elf *e, GElf_Shdr sh);
uint64_t	initscan(Elf *e, GElf_Shdr sh);
uint64_t	textscan(Elf *e, GElf_Shdr sh);
uint64_t	finiscan(Elf *e, GElf_Shdr sh);
uint64_t	altinstr_replacementscan(Elf *e, GElf_Shdr sh);
uint64_t	ehframescan(Elf *e, ehRecord_t *ehRecord);
uint64_t 	skipSectionScan(Elf *e, GElf_Shdr secHead, int secFlag);
uint64_t  decodeULEB128(uint8_t *input, uint64_t *sizeInBytes);
int64_t  decodeSLEB128(uint8_t *input, uint64_t *sizeInBytes);


// Defines 
#define EHF_CF_DONE	      (0)
#define EHF_CF_CONT	      (1)
#define EHF_WO_TYPE	      (1)
#define EHF_WO_FS	        (2)
#define EHF_TP_CIE	      (0)
#define EHF_SLEB128_ERROR (-1)
#define EHF_ULEB128_ERROR (0xffffffffffffffffull)

#define EHF_CIE_BO_VER    (8)
#define EHF_CIE_BO_AUSTR  (9)

#define EHF_CIE_VER_1     (1)
#define EHF_CIE_VER_3     (3)


#endif  // _FNBOUNDS_SCAN_H_
