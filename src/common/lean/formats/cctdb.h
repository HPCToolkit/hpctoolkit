// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//***************************************************************************
//
// Purpose:
//   Low-level types and functions for reading/writing cct.db
//
//   See doc/FORMATS.md.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef FORMATS_CCTDB_H
#define FORMATS_CCTDB_H

#include "common.h"

#if defined(__cplusplus)
extern "C" {
#endif

/// Minor version of the cct.db format implemented here
enum { FMT_CCTDB_MinorVersion = 0 };

/// Check the given file start bytes for the cct.db format.
/// If minorVer != NULL, also returns the exact minor version.
enum fmt_version_t fmt_cctdb_check(const char[16], uint8_t* minorVer);

/// Footer byte sequence for cct.db files.
extern const char fmt_cctdb_footer[8];

//
// cct.db file
//

/// Size of the cct.db file header in serialized form
enum { FMT_CCTDB_SZ_FHdr = 0x30 };

/// cct.db file header, names match FORMATS.md
typedef struct fmt_cctdb_fHdr_t {
  // NOTE: magic and versions are constant and cannot be adjusted
  uint64_t szCtxInfo;
  uint64_t pCtxInfo;
} fmt_cctdb_fHdr_t;

/// Read a cct.db file header from a byte array
void fmt_cctdb_fHdr_read(fmt_cctdb_fHdr_t*, const char[FMT_CCTDB_SZ_FHdr]);

/// Write a cct.db file header into a byte array
void fmt_cctdb_fHdr_write(char[FMT_CCTDB_SZ_FHdr], const fmt_cctdb_fHdr_t*);

//
// Context Info section
//

// Context Info section header
enum { FMT_CCTDB_SZ_CtxInfoSHdr = 0x0d };
typedef struct fmt_cctdb_ctxInfoSHdr_t {
  uint64_t pCtxs;
  uint32_t nCtxs;
  // NOTE: The following field is ignored on write
  uint8_t szCtx;
} fmt_cctdb_ctxInfoSHdr_t;

void fmt_cctdb_ctxInfoSHdr_read(fmt_cctdb_ctxInfoSHdr_t*, const char[FMT_CCTDB_SZ_CtxInfoSHdr]);
void fmt_cctdb_ctxInfoSHdr_write(char[FMT_CCTDB_SZ_CtxInfoSHdr], const fmt_cctdb_ctxInfoSHdr_t*);

// Context Information block {CI}
enum { FMT_CCTDB_SZ_CtxInfo = 0x20 };
typedef struct fmt_cctdb_ctxInfo_t {
  struct {
    uint64_t nValues;
    uint64_t pValues;
    uint16_t nMetrics;
    uint64_t pMetricIndices;
  } valueBlock;
} fmt_cctdb_ctxInfo_t;

void fmt_cctdb_ctxInfo_read(fmt_cctdb_ctxInfo_t*, const char[FMT_CCTDB_SZ_CtxInfo]);
void fmt_cctdb_ctxInfo_write(char[FMT_CCTDB_SZ_CtxInfo], const fmt_cctdb_ctxInfo_t*);

// Profile-Value pair {Val}
enum { FMT_CCTDB_SZ_PVal = 0x0c };
typedef struct fmt_cctdb_pVal_t {
  uint32_t profIndex;
  double value;
} fmt_cctdb_pVal_t;

void fmt_cctdb_pVal_read(fmt_cctdb_pVal_t*, const char[FMT_CCTDB_SZ_PVal]);
void fmt_cctdb_pVal_write(char[FMT_CCTDB_SZ_PVal], const fmt_cctdb_pVal_t*);

// Metric-Index pair
enum { FMT_CCTDB_SZ_MIdx = 0x0a };
typedef struct fmt_cctdb_mIdx_t {
  uint16_t metricId;
  uint64_t startIndex;
} fmt_cctdb_mIdx_t;

void fmt_cctdb_mIdx_read(fmt_cctdb_mIdx_t*, const char[FMT_CCTDB_SZ_MIdx]);
void fmt_cctdb_mIdx_write(char[FMT_CCTDB_SZ_MIdx], const fmt_cctdb_mIdx_t*);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif  // FORMATS_CCTDB_H
