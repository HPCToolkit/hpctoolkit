// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2022, Rice University
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
enum { FMT_TRACEDB_SZ_FHdr = 0x30 };

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
