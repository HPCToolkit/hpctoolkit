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
//   Low-level types and functions for reading/writing meta.db
//
//   See doc/FORMATS.md.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include "metadb.h"

#include "primitive.h"

#include <string.h>

static_assert('a' == 0x61, "Byte encoding isn't ASCII?");
static const char fmt_metadb_magic[14] = "HPCTOOLKITmeta";
const char fmt_metadb_footer[8] = "_meta.db";

enum fmt_version_t fmt_metadb_check(const char hdr[16], uint8_t* minorVer) {
  if (memcmp(hdr, fmt_metadb_magic, sizeof fmt_metadb_magic) != 0)
    return fmt_version_invalid;
  if (hdr[0xe] != FMT_DB_MajorVersion)
    return fmt_version_major;
  if (minorVer != NULL)
    *minorVer = hdr[0xf];
  if (hdr[0xf] < FMT_METADB_MinorVersion)
    return fmt_version_backward;
  return hdr[0xf] > FMT_METADB_MinorVersion ? fmt_version_forward : fmt_version_exact;
}

void fmt_metadb_fHdr_read(fmt_metadb_fHdr_t* hdr, const char d[FMT_METADB_SZ_FHdr]) {
  hdr->szGeneral = fmt_u64_read(d + 0x10);
  hdr->pGeneral = fmt_u64_read(d + 0x18);
  hdr->szIdNames = fmt_u64_read(d + 0x20);
  hdr->pIdNames = fmt_u64_read(d + 0x28);
  hdr->szMetrics = fmt_u64_read(d + 0x30);
  hdr->pMetrics = fmt_u64_read(d + 0x38);
  hdr->szContext = fmt_u64_read(d + 0x40);
  hdr->pContext = fmt_u64_read(d + 0x48);
  hdr->szStrings = fmt_u64_read(d + 0x50);
  hdr->pStrings = fmt_u64_read(d + 0x58);
  hdr->szModules = fmt_u64_read(d + 0x60);
  hdr->pModules = fmt_u64_read(d + 0x68);
  hdr->szFiles = fmt_u64_read(d + 0x70);
  hdr->pFiles = fmt_u64_read(d + 0x78);
  hdr->szFunctions = fmt_u64_read(d + 0x80);
  hdr->pFunctions = fmt_u64_read(d + 0x88);
}
void fmt_metadb_fHdr_write(char d[FMT_METADB_SZ_FHdr], const fmt_metadb_fHdr_t* hdr) {
  memcpy(d, fmt_metadb_magic, sizeof fmt_metadb_magic);
  d[0x0e] = FMT_DB_MajorVersion;
  d[0x0f] = FMT_METADB_MinorVersion;
  fmt_u64_write(d + 0x10, hdr->szGeneral);
  fmt_u64_write(d + 0x18, hdr->pGeneral);
  fmt_u64_write(d + 0x20, hdr->szIdNames);
  fmt_u64_write(d + 0x28, hdr->pIdNames);
  fmt_u64_write(d + 0x30, hdr->szMetrics);
  fmt_u64_write(d + 0x38, hdr->pMetrics);
  fmt_u64_write(d + 0x40, hdr->szContext);
  fmt_u64_write(d + 0x48, hdr->pContext);
  fmt_u64_write(d + 0x50, hdr->szStrings);
  fmt_u64_write(d + 0x58, hdr->pStrings);
  fmt_u64_write(d + 0x60, hdr->szModules);
  fmt_u64_write(d + 0x68, hdr->pModules);
  fmt_u64_write(d + 0x70, hdr->szFiles);
  fmt_u64_write(d + 0x78, hdr->pFiles);
  fmt_u64_write(d + 0x80, hdr->szFunctions);
  fmt_u64_write(d + 0x88, hdr->pFunctions);
}

void fmt_metadb_generalSHdr_read(
    fmt_metadb_generalSHdr_t* ghdr, const char d[FMT_METADB_SZ_GeneralSHdr]) {
  ghdr->pTitle = fmt_u64_read(d + 0x00);
  ghdr->pDescription = fmt_u64_read(d + 0x08);
}
void fmt_metadb_generalSHdr_write(
    char d[FMT_METADB_SZ_GeneralSHdr], const fmt_metadb_generalSHdr_t* ghdr) {
  fmt_u64_write(d + 0x00, ghdr->pTitle);
  fmt_u64_write(d + 0x08, ghdr->pDescription);
}

void fmt_metadb_idNamesSHdr_read(
    fmt_metadb_idNamesSHdr_t* ihdr, const char d[FMT_METADB_SZ_IdNamesSHdr]) {
  ihdr->ppNames = fmt_u64_read(d + 0x00);
  ihdr->nKinds = d[0x08];
}
void fmt_metadb_idNamesSHdr_write(
    char d[FMT_METADB_SZ_IdNamesSHdr], const fmt_metadb_idNamesSHdr_t* ihdr) {
  fmt_u64_write(d + 0x00, ihdr->ppNames);
  d[0x08] = ihdr->nKinds;
}

void fmt_metadb_metricsSHdr_read(
    fmt_metadb_metricsSHdr_t* mhdr, const char d[FMT_METADB_SZ_MetricsSHdr]) {
  mhdr->pMetrics = fmt_u64_read(d + 0x00);
  mhdr->nMetrics = fmt_u32_read(d + 0x08);
  mhdr->szMetric = d[0x0c];
  mhdr->szScope = d[0x0d];
  mhdr->szSummary = d[0x0e];
}
void fmt_metadb_metricsSHdr_write(
    char d[FMT_METADB_SZ_MetricsSHdr], const fmt_metadb_metricsSHdr_t* mhdr) {
  fmt_u64_write(d + 0x00, mhdr->pMetrics);
  fmt_u64_write(d + 0x08, mhdr->nMetrics);
  d[0x0c] = FMT_METADB_SZ_MetricDesc;
  d[0x0d] = FMT_METADB_SZ_MetricScope;
  d[0x0e] = FMT_METADB_SZ_MetricSummary;
}

void fmt_metadb_metricDesc_read(
    fmt_metadb_metricDesc_t* md, const char d[FMT_METADB_SZ_MetricDesc]) {
  md->pName = fmt_u64_read(d + 0x00);
  md->nScopes = fmt_u16_read(d + 0x08);
  md->pScopes = fmt_u64_read(d + 0x10);
}
void fmt_metadb_metricDesc_write(
    char d[FMT_METADB_SZ_MetricDesc], const fmt_metadb_metricDesc_t* md) {
  fmt_u64_write(d + 0x00, md->pName);
  fmt_u16_write(d + 0x08, md->nScopes);
  fmt_u64_write(d + 0x10, md->pScopes);
}

void fmt_metadb_metricScope_read(
    fmt_metadb_metricScope_t* ps, const char d[FMT_METADB_SZ_MetricScope]) {
  ps->pScope = fmt_u64_read(d + 0x00);
  ps->nSummaries = fmt_u16_read(d + 0x08);
  ps->propMetricId = fmt_u16_read(d + 0x0a);
  ps->pSummaries = fmt_u64_read(d + 0x10);
}
void fmt_metadb_metricScope_write(
    char d[FMT_METADB_SZ_MetricScope], const fmt_metadb_metricScope_t* ps) {
  fmt_u64_write(d + 0x00, ps->pScope);
  fmt_u16_write(d + 0x08, ps->nSummaries);
  fmt_u16_write(d + 0x0a, ps->propMetricId);
  fmt_u64_write(d + 0x10, ps->pSummaries);
}

void fmt_metadb_metricSummary_read(
    fmt_metadb_metricSummary_t* ss, const char d[FMT_METADB_SZ_MetricSummary]) {
  ss->pFormula = fmt_u64_read(d + 0x00);
  ss->combine = d[0x08];
  ss->statMetricId = fmt_u16_read(d + 0x0a);
}
void fmt_metadb_metricSummary_write(
    char d[FMT_METADB_SZ_MetricSummary], const fmt_metadb_metricSummary_t* ss) {
  fmt_u64_write(d + 0x00, ss->pFormula);
  d[0x08] = ss->combine;
  fmt_u16_write(d + 0x0a, ss->statMetricId);
}

void fmt_metadb_modulesSHdr_read(
    fmt_metadb_modulesSHdr_t* mhdr, const char d[FMT_METADB_SZ_ModulesSHdr]) {
  mhdr->pModules = fmt_u64_read(d + 0x00);
  mhdr->nModules = fmt_u32_read(d + 0x08);
  mhdr->szModule = fmt_u16_read(d + 0x0c);
}
void fmt_metadb_modulesSHdr_write(
    char d[FMT_METADB_SZ_ModulesSHdr], const fmt_metadb_modulesSHdr_t* mhdr) {
  fmt_u64_write(d + 0x00, mhdr->pModules);
  fmt_u32_write(d + 0x08, mhdr->nModules);
  fmt_u16_write(d + 0x0c, FMT_METADB_SZ_ModuleSpec);
}

void fmt_metadb_moduleSpec_read(
    fmt_metadb_moduleSpec_t* lms, const char d[FMT_METADB_SZ_ModuleSpec]) {
  lms->pPath = fmt_u64_read(d + 0x08);
}
void fmt_metadb_moduleSpec_write(
    char d[FMT_METADB_SZ_ModuleSpec], const fmt_metadb_moduleSpec_t* lms) {
  fmt_u64_write(d + 0x08, lms->pPath);
}

void fmt_metadb_filesSHdr_read(
    fmt_metadb_filesSHdr_t* fhdr, const char d[FMT_METADB_SZ_FilesSHdr]) {
  fhdr->pFiles = fmt_u64_read(d + 0x00);
  fhdr->nFiles = fmt_u32_read(d + 0x08);
  fhdr->szFile = fmt_u16_read(d + 0x0c);
}
void fmt_metadb_filesSHdr_write(
    char d[FMT_METADB_SZ_FilesSHdr], const fmt_metadb_filesSHdr_t* fhdr) {
  fmt_u64_write(d + 0x00, fhdr->pFiles);
  fmt_u32_write(d + 0x08, fhdr->nFiles);
  fmt_u16_write(d + 0x0c, FMT_METADB_SZ_FileSpec);
}

void fmt_metadb_fileSpec_read(fmt_metadb_fileSpec_t* sfs, const char d[FMT_METADB_SZ_FileSpec]) {
  sfs->copied = (d[0x00] & 0x1) != 0;
  sfs->pPath = fmt_u64_read(d + 0x08);
}
void fmt_metadb_fileSpec_write(char d[FMT_METADB_SZ_FileSpec], const fmt_metadb_fileSpec_t* sfs) {
  d[0x00] = (sfs->copied ? 0x1 : 0) | 0;
  d[0x01] = 0;
  d[0x02] = 0;
  d[0x03] = 0;
  fmt_u64_write(d + 0x08, sfs->pPath);
}

void fmt_metadb_functionsSHdr_read(
    fmt_metadb_functionsSHdr_t* fhdr, const char d[FMT_METADB_SZ_FunctionsSHdr]) {
  fhdr->pFunctions = fmt_u64_read(d + 0x00);
  fhdr->nFunctions = fmt_u32_read(d + 0x08);
  fhdr->szFunction = fmt_u16_read(d + 0x0c);
}
void fmt_metadb_functionsSHdr_write(
    char d[FMT_METADB_SZ_FunctionsSHdr], const fmt_metadb_functionsSHdr_t* fhdr) {
  fmt_u64_write(d + 0x00, fhdr->pFunctions);
  fmt_u32_write(d + 0x08, fhdr->nFunctions);
  fmt_u16_write(d + 0x0c, FMT_METADB_SZ_FunctionSpec);
}

void fmt_metadb_functionSpec_read(
    fmt_metadb_functionSpec_t* fs, const char d[FMT_METADB_SZ_FunctionSpec]) {
  fs->pName = fmt_u64_read(d + 0x00);
  fs->pModule = fmt_u64_read(d + 0x08);
  fs->offset = fmt_u64_read(d + 0x10);
  fs->pFile = fmt_u64_read(d + 0x18);
  fs->line = fmt_u32_read(d + 0x20);
}
void fmt_metadb_functionSpec_write(
    char d[FMT_METADB_SZ_FunctionSpec], const fmt_metadb_functionSpec_t* fs) {
  fmt_u64_write(d + 0x00, fs->pName);
  fmt_u64_write(d + 0x08, fs->pModule);
  fmt_u64_write(d + 0x10, fs->offset);
  fmt_u64_write(d + 0x18, fs->pFile);
  fmt_u32_write(d + 0x20, fs->line);
}

void fmt_metadb_contextsSHdr_read(
    fmt_metadb_contextsSHdr_t* chdr, const char d[FMT_METADB_SZ_ContextsSHdr]) {
  chdr->szRoots = fmt_u64_read(d + 0x00);
  chdr->pRoots = fmt_u64_read(d + 0x08);
}
void fmt_metadb_contextsSHdr_write(
    char d[FMT_METADB_SZ_ContextsSHdr], const fmt_metadb_contextsSHdr_t* chdr) {
  fmt_u64_write(d + 0x00, chdr->szRoots);
  fmt_u64_write(d + 0x08, chdr->pRoots);
}

bool fmt_metadb_context_read(fmt_metadb_context_t* ctx, const char* d) {
  ctx->szChildren = fmt_u64_read(d + 0x00);
  ctx->pChildren = fmt_u64_read(d + 0x08);
  ctx->ctxId = fmt_u32_read(d + 0x10);
  ctx->relation = d[0x15];
  ctx->lexicalType = d[0x16];
  ctx->nFlexWords = d[0x17];

  // NOTE: At the moment, there is only one flex sub-field smaller than u64, so
  // we can just count in words.
  int nwords = 0;

  ctx->pFunction = 0;
  if ((d[0x14] & 0x1) != 0) {
    if (ctx->nFlexWords < nwords + 1)
      return false;
    ctx->pFunction = fmt_u64_read(d + 0x18 + nwords * 8);
    nwords += 1;
  }

  ctx->pFile = 0;
  ctx->line = 0;
  if ((d[0x14] & 0x2) != 0) {
    if (ctx->nFlexWords < nwords + 2)
      return false;
    ctx->pFile = fmt_u64_read(d + 0x18 + nwords * 8);
    ctx->line = fmt_u32_read(d + 0x18 + (nwords + 1) * 8);
    nwords += 2;
  }

  ctx->pModule = 0;
  ctx->offset = 0;
  if ((d[0x14] & 0x4) != 0) {
    if (ctx->nFlexWords < nwords + 2)
      return false;
    ctx->pModule = fmt_u64_read(d + 0x18 + nwords * 8);
    ctx->offset = fmt_u64_read(d + 0x18 + (nwords + 1) * 8);
    nwords += 2;
  }

  return true;
}
size_t fmt_metadb_context_write(char* d, const fmt_metadb_context_t* ctx) {
  fmt_u64_write(d + 0x00, ctx->szChildren);
  fmt_u64_write(d + 0x08, ctx->pChildren);
  fmt_u32_write(d + 0x10, ctx->ctxId);
  d[0x14] = 0;
  d[0x15] = ctx->relation;
  d[0x16] = ctx->lexicalType;

  // NOTE: At the moment, there is only one flex sub-field smaller than u64, so
  // we can just count in words.
  int nwords = 0;

  if (ctx->pFunction != 0) {
    d[0x14] |= 0x1;  // hasFunction
    fmt_u64_write(d + 0x18 + nwords * 8, ctx->pFunction);
    nwords += 1;
  }

  if (ctx->pFile != 0) {
    d[0x14] |= 0x2;  // hasSrcLine
    fmt_u64_write(d + 0x18 + nwords * 8, ctx->pFile);
    fmt_u32_write(d + 0x18 + (nwords + 1) * 8, ctx->line);
    nwords += 2;
  }

  if (ctx->pModule != 0) {
    d[0x14] |= 0x4;  // hasPoint
    fmt_u64_write(d + 0x18 + nwords * 8, ctx->pModule);
    fmt_u64_write(d + 0x18 + (nwords + 1) * 8, ctx->offset);
    nwords += 2;
  }

  d[0x17] = nwords;
  return FMT_METADB_SZ_Context(nwords);
}
