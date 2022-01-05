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
//   Low-level types and functions for reading/writing trace.db
//
//   See doc/FORMATS.md.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include "tracedb.h"

#include "primitive.h"

#include <string.h>

static_assert('a' == 0x61, "Byte encoding isn't ASCII?");
static const char fmt_tracedb_magic[14] = "HPCTOOLKITtrce";
const char fmt_tracedb_footer[8] = "trace.db";

enum fmt_version_t fmt_tracedb_check(const char hdr[16], uint8_t* minorVer) {
  if(memcmp(hdr, fmt_tracedb_magic, sizeof fmt_tracedb_magic) != 0)
    return fmt_version_invalid;
  if(hdr[0xe] != FMT_DB_MajorVersion)
    return fmt_version_major;
  if(minorVer != NULL) *minorVer = hdr[0xf];
  if(hdr[0xf] < FMT_TRACEDB_MinorVersion)
    return fmt_version_backward;
  return hdr[0xf] > FMT_TRACEDB_MinorVersion
         ? fmt_version_forward : fmt_version_exact;
}

void fmt_tracedb_fHdr_read(fmt_tracedb_fHdr_t* hdr, const char d[FMT_TRACEDB_SZ_FHdr]) {
  hdr->szCtxTraces = fmt_u64_read(d+0x10);
  hdr->pCtxTraces = fmt_u64_read(d+0x18);
}
void fmt_tracedb_fHdr_write(char d[FMT_TRACEDB_SZ_FHdr], const fmt_tracedb_fHdr_t* hdr) {
  memcpy(d, fmt_tracedb_magic, sizeof fmt_tracedb_magic);
  d[0x0e] = FMT_DB_MajorVersion;
  d[0x0f] = FMT_TRACEDB_MinorVersion;
  fmt_u64_write(d+0x10, hdr->szCtxTraces);
  fmt_u64_write(d+0x18, hdr->pCtxTraces);
}

void fmt_tracedb_ctxTraceSHdr_read(fmt_tracedb_ctxTraceSHdr_t* hdr, const char d[FMT_TRACEDB_SZ_CtxTraceSHdr]) {
  hdr->pTraces = fmt_u64_read(d+0x00);
  hdr->nTraces = fmt_u32_read(d+0x08);
  hdr->szTrace = d[0x0c];
  hdr->minTimestamp = fmt_u64_read(d+0x10);
  hdr->maxTimestamp = fmt_u64_read(d+0x18);
}
void fmt_tracedb_ctxTraceSHdr_write(char d[FMT_TRACEDB_SZ_CtxTraceSHdr], const fmt_tracedb_ctxTraceSHdr_t* hdr) {
  fmt_u64_write(d+0x00, hdr->pTraces);
  fmt_u32_write(d+0x08, hdr->nTraces);
  d[0x0c] = FMT_TRACEDB_SZ_CtxTrace;
  fmt_u64_write(d+0x10, hdr->minTimestamp);
  fmt_u64_write(d+0x18, hdr->maxTimestamp);
}

void fmt_tracedb_ctxTrace_read(fmt_tracedb_ctxTrace_t* cth, const char d[FMT_TRACEDB_SZ_CtxTrace]) {
  cth->profIndex = fmt_u32_read(d+0x00);
  cth->pStart = fmt_u64_read(d+0x08);
  cth->pEnd = fmt_u64_read(d+0x10);
}
void fmt_tracedb_ctxTrace_write(char d[FMT_TRACEDB_SZ_CtxTrace], const fmt_tracedb_ctxTrace_t* cth) {
  fmt_u32_write(d+0x00, cth->profIndex);
  fmt_u64_write(d+0x08, cth->pStart);
  fmt_u64_write(d+0x10, cth->pEnd);
}

void fmt_tracedb_ctxSample_read(fmt_tracedb_ctxSample_t* elem, const char d[FMT_TRACEDB_SZ_CtxSample]) {
  elem->timestamp = fmt_u64_read(d+0x00);
  elem->ctxId = fmt_u32_read(d+0x08);
}
void fmt_tracedb_ctxSample_write(char d[FMT_TRACEDB_SZ_CtxSample], const fmt_tracedb_ctxSample_t* elem) {
  fmt_u64_write(d+0x00, elem->timestamp);
  fmt_u32_write(d+0x08, elem->ctxId);
}
