// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

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

#ifndef FORMATS_TRACEDB_H
#define FORMATS_TRACEDB_H

#include "common.h"

#if defined(__cplusplus)
extern "C" {
#endif

/// Minor version of the trace.db format implemented here
enum { FMT_TRACEDB_MinorVersion = 0 };

/// Check the given file start bytes for the trace.db format.
/// If minorVer != NULL, also returns the exact minor version.
enum fmt_version_t fmt_tracedb_check(const char[16], uint8_t* minorVer);

/// Footer byte sequence for trace.db files.
extern const char fmt_tracedb_footer[8];

//
// trace.db file
//

/// Size of the trace.db file header in serialized form
enum { FMT_TRACEDB_SZ_FHdr = 0x20 };

/// trace.db file header, names match FORMATS.md
typedef struct fmt_tracedb_fHdr_t {
  // NOTE: magic and versions are constant and cannot be adjusted
  uint64_t szCtxTraces;
  uint64_t pCtxTraces;
} fmt_tracedb_fHdr_t;

/// Read a trace.db file header from a byte array
void fmt_tracedb_fHdr_read(fmt_tracedb_fHdr_t*, const char[FMT_TRACEDB_SZ_FHdr]);

/// Write a trace.db file header into a byte array
void fmt_tracedb_fHdr_write(char[FMT_TRACEDB_SZ_FHdr], const fmt_tracedb_fHdr_t*);

//
// Context Trace Headers section
//

// Context Trace Headers section header
enum { FMT_TRACEDB_SZ_CtxTraceSHdr = 0x20 };
typedef struct fmt_tracedb_ctxTraceSHdr_t {
  uint64_t pTraces;
  uint32_t nTraces;
  uint8_t szTrace;
  uint64_t minTimestamp;
  uint64_t maxTimestamp;
} fmt_tracedb_ctxTraceSHdr_t;

void fmt_tracedb_ctxTraceSHdr_read(fmt_tracedb_ctxTraceSHdr_t*, const char[FMT_TRACEDB_SZ_CtxTraceSHdr]);
void fmt_tracedb_ctxTraceSHdr_write(char[FMT_TRACEDB_SZ_CtxTraceSHdr], const fmt_tracedb_ctxTraceSHdr_t*);

// Context Trace Header structure {CTH}
enum { FMT_TRACEDB_SZ_CtxTrace = 0x18 };
typedef struct fmt_tracedb_ctxTrace_t {
  uint32_t profIndex;
  uint64_t pStart;
  uint64_t pEnd;
} fmt_tracedb_ctxTrace_t;

void fmt_tracedb_ctxTrace_read(fmt_tracedb_ctxTrace_t*, const char[FMT_TRACEDB_SZ_CtxTrace]);
void fmt_tracedb_ctxTrace_write(char[FMT_TRACEDB_SZ_CtxTrace], const fmt_tracedb_ctxTrace_t*);

// Context Trace Sample {Elem}
enum { FMT_TRACEDB_SZ_CtxSample = 0x0c };
typedef struct fmt_tracedb_ctxSample_t {
  uint64_t timestamp;
  uint32_t ctxId;
} fmt_tracedb_ctxSample_t;

void fmt_tracedb_ctxSample_read(fmt_tracedb_ctxSample_t*, const char[FMT_TRACEDB_SZ_CtxSample]);
void fmt_tracedb_ctxSample_write(char[FMT_TRACEDB_SZ_CtxSample], const fmt_tracedb_ctxSample_t*);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif  // FORMATS_TRACEDB_H
