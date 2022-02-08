// -*-Mode: C++;-*- // technically C99

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

//***************************************************************************
//
// Purpose:
//   Low-level types and functions for reading/writing profile.db
//
//   See doc/FORMATS.md.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include "profiledb.h"

#include "primitive.h"

#include <string.h>

static_assert('a' == 0x61, "Byte encoding isn't ASCII?");
static const char fmt_profiledb_magic[14] = "HPCTOOLKITprof";
const char fmt_profiledb_footer[8] = "_prof.db";

enum fmt_version_t fmt_profiledb_check(const char hdr[16], uint8_t* minorVer) {
  if(memcmp(hdr, fmt_profiledb_magic, sizeof fmt_profiledb_magic) != 0)
    return fmt_version_invalid;
  if(hdr[0xe] != FMT_DB_MajorVersion)
    return fmt_version_major;
  if(minorVer != NULL) *minorVer = hdr[0xf];
  if(hdr[0xf] < FMT_PROFILEDB_MinorVersion)
    return fmt_version_backward;
  return hdr[0xf] > FMT_PROFILEDB_MinorVersion
         ? fmt_version_forward : fmt_version_exact;
}

void fmt_profiledb_fHdr_read(fmt_profiledb_fHdr_t* hdr, const char d[FMT_PROFILEDB_SZ_FHdr]) {
  hdr->szProfileInfos = fmt_u64_read(d+0x10);
  hdr->pProfileInfos = fmt_u64_read(d+0x18);
  hdr->szIdTuples = fmt_u64_read(d+0x20);
  hdr->pIdTuples = fmt_u64_read(d+0x28);
}
void fmt_profiledb_fHdr_write(char d[FMT_PROFILEDB_SZ_FHdr], const fmt_profiledb_fHdr_t* hdr) {
  memcpy(d, fmt_profiledb_magic, sizeof fmt_profiledb_magic);
  d[0x0e] = FMT_DB_MajorVersion;
  d[0x0f] = FMT_PROFILEDB_MinorVersion;
  fmt_u64_write(d+0x10, hdr->szProfileInfos);
  fmt_u64_write(d+0x18, hdr->pProfileInfos);
  fmt_u64_write(d+0x20, hdr->szIdTuples);
  fmt_u64_write(d+0x28, hdr->pIdTuples);
}

void fmt_profiledb_profInfoSHdr_read(fmt_profiledb_profInfoSHdr_t* hdr, const char d[FMT_PROFILEDB_SZ_ProfInfoSHdr]) {
  hdr->pProfiles = fmt_u64_read(d+0x00);
  hdr->nProfiles = fmt_u32_read(d+0x08);
  hdr->szProfile = d[0x0c];
}
void fmt_profiledb_profInfoSHdr_write(char d[FMT_PROFILEDB_SZ_ProfInfoSHdr], const fmt_profiledb_profInfoSHdr_t* hdr) {
  fmt_u64_write(d+0x00, hdr->pProfiles);
  fmt_u32_write(d+0x08, hdr->nProfiles);
  d[0x0c] = FMT_PROFILEDB_SZ_ProfInfo;
}

void fmt_profiledb_profInfo_read(fmt_profiledb_profInfo_t* pi, const char d[FMT_PROFILEDB_SZ_ProfInfo]) {
  pi->valueBlock.nValues = fmt_u64_read(d+0x00);
  pi->valueBlock.pValues = fmt_u64_read(d+0x08);
  pi->valueBlock.nCtxs = fmt_u32_read(d+0x10);
  pi->valueBlock.pCtxIndices = fmt_u64_read(d+0x18);
  pi->pIdTuple = fmt_u64_read(d+0x20);
}
void fmt_profiledb_profInfo_write(char d[FMT_PROFILEDB_SZ_ProfInfo], const fmt_profiledb_profInfo_t* pi) {
  fmt_u64_write(d+0x00, pi->valueBlock.nValues);
  fmt_u64_write(d+0x08, pi->valueBlock.pValues);
  fmt_u32_write(d+0x10, pi->valueBlock.nCtxs);
  fmt_u64_write(d+0x18, pi->valueBlock.pCtxIndices);
  fmt_u64_write(d+0x20, pi->pIdTuple);
}

void fmt_profiledb_mVal_read(fmt_profiledb_mVal_t* mv, const char d[FMT_PROFILEDB_SZ_MVal]) {
  mv->metricId = fmt_u16_read(d+0x00);
  mv->value = fmt_f64_read(d+0x02);
}
void fmt_profiledb_mVal_write(char d[FMT_PROFILEDB_SZ_MVal], const fmt_profiledb_mVal_t* mv) {
  fmt_u16_write(d+0x00, mv->metricId);
  fmt_f64_write(d+0x02, mv->value);
}

void fmt_profiledb_cIdx_read(fmt_profiledb_cIdx_t* ci, const char d[FMT_PROFILEDB_SZ_CIdx]) {
  ci->ctxId = fmt_u32_read(d+0x00);
  ci->startIndex = fmt_u64_read(d+0x04);
}
void fmt_profiledb_cIdx_write(char d[FMT_PROFILEDB_SZ_CIdx], const fmt_profiledb_cIdx_t* ci) {
  fmt_u32_write(d+0x00, ci->ctxId);
  fmt_u64_write(d+0x04, ci->startIndex);
}

void fmt_profiledb_idTupleHdr_read(fmt_profiledb_idTupleHdr_t* hdr, const char d[FMT_PROFILEDB_SZ_IdTupleHdr]) {
  hdr->nIds = fmt_u16_read(d+0x00);
}
void fmt_profiledb_idTupleHdr_write(char d[FMT_PROFILEDB_SZ_IdTupleHdr], const fmt_profiledb_idTupleHdr_t* hdr) {
  fmt_u16_write(d+0x00, hdr->nIds);
}

void fmt_profiledb_idTupleElem_read(fmt_profiledb_idTupleElem_t* elem, const char d[FMT_PROFILEDB_SZ_IdTupleElem]) {
  elem->kind = d[0x00];
  elem->isPhysical = (d[0x02] & 0x1) != 0;
  elem->logicalId = fmt_u32_read(d+0x04);
  elem->physicalId = fmt_u64_read(d+0x08);
}
void fmt_profiledb_idTupleElem_write(char d[FMT_PROFILEDB_SZ_IdTupleElem], const fmt_profiledb_idTupleElem_t* elem) {
  d[0x00] = elem->kind;
  d[0x02] = 0
    | (elem->isPhysical ? 0x1 : 0);
  fmt_u32_write(d+0x04, elem->logicalId);
  fmt_u64_write(d+0x08, elem->physicalId);
}
