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
//   Low-level types and functions for reading/writing profile.db
//
//   See doc/FORMATS.md.
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#ifndef FORMATS_PROFILEDB_H
#define FORMATS_PROFILEDB_H

#include "common.h"

#if defined(__cplusplus)
extern "C" {
#endif

/// Minor version of the profile.db format implemented here
enum { FMT_PROFILEDB_MinorVersion = 0 };

/// Check the given file start bytes for the profile.db format.
/// If minorVer != NULL, also returns the exact minor version.
enum fmt_version_t fmt_profiledb_check(const char[16], uint8_t* minorVer);

/// Footer byte sequence for profile.db files.
extern const char fmt_profiledb_footer[8];

//
// profile.db file
//

/// Size of the profile.db file header in serialized form
enum { FMT_PROFILEDB_SZ_FHdr = 0x30 };

/// profile.db file header, names match FORMATS.md
typedef struct fmt_profiledb_fHdr_t {
  // NOTE: magic and versions are constant and cannot be adjusted
  uint64_t szProfileInfos;
  uint64_t pProfileInfos;
  uint64_t szIdTuples;
  uint64_t pIdTuples;
} fmt_profiledb_fHdr_t;

/// Read a profile.db file header from a byte array
void fmt_profiledb_fHdr_read(fmt_profiledb_fHdr_t*, const char[FMT_PROFILEDB_SZ_FHdr]);

/// Write a profile.db file header into a byte array
void fmt_profiledb_fHdr_write(char[FMT_PROFILEDB_SZ_FHdr], const fmt_profiledb_fHdr_t*);

//
// profile.db Profile Info section
//

// profile.db Profile Info section header
enum { FMT_PROFILEDB_SZ_ProfInfoSHdr = 0x0d };
typedef struct fmt_profiledb_profInfoSHdr_t {
  uint64_t pProfiles;
  uint32_t nProfiles;
  // NOTE: The following member is ignored on write
  uint8_t szProfile;
} fmt_profiledb_profInfoSHdr_t;

void fmt_profiledb_profInfoSHdr_read(fmt_profiledb_profInfoSHdr_t*, const char[FMT_PROFILEDB_SZ_ProfInfoSHdr]);
void fmt_profiledb_profInfoSHdr_write(char[FMT_PROFILEDB_SZ_ProfInfoSHdr], const fmt_profiledb_profInfoSHdr_t*);

// Profile Information block {PI}
enum { FMT_PROFILEDB_SZ_ProfInfo = 0x28 };
typedef struct fmt_profiledb_profInfo_t {
  // Profile-Major Sparse Value Block [PSVB]
  struct {
    uint64_t nValues;
    uint64_t pValues;
    uint32_t nCtxs;
    uint64_t pCtxIndices;
  } valueBlock;
  uint64_t pIdTuple;
} fmt_profiledb_profInfo_t;

void fmt_profiledb_profInfo_read(fmt_profiledb_profInfo_t*, const char[FMT_PROFILEDB_SZ_ProfInfo]);
void fmt_profiledb_profInfo_write(char[FMT_PROFILEDB_SZ_ProfInfo], const fmt_profiledb_profInfo_t*);

// Metric-Value pair {Val}
enum { FMT_PROFILEDB_SZ_MVal = 0x0a };
typedef struct fmt_profiledb_mVal_t {
  uint16_t metricId;
  double value;
} fmt_profiledb_mVal_t;

void fmt_profiledb_mVal_read(fmt_profiledb_mVal_t*, const char[FMT_PROFILEDB_SZ_MVal]);
void fmt_profiledb_mVal_write(char[FMT_PROFILEDB_SZ_MVal], const fmt_profiledb_mVal_t*);

// Context-Index pair {Idx}
enum { FMT_PROFILEDB_SZ_CIdx = 0x0c };
typedef struct fmt_profiledb_cIdx_t {
  uint32_t ctxId;
  uint64_t startIndex;
} fmt_profiledb_cIdx_t;

void fmt_profiledb_cIdx_read(fmt_profiledb_cIdx_t*, const char[FMT_PROFILEDB_SZ_CIdx]);
void fmt_profiledb_cIdx_write(char[FMT_PROFILEDB_SZ_CIdx], const fmt_profiledb_cIdx_t*);

//
// profile.db Hierarchical Identifier Tuple section
//

// Hierarchical Identifier Tuple [HIT] header
enum { FMT_PROFILEDB_SZ_IdTupleHdr = 0x08 };
typedef struct fmt_profiledb_idTupleHdr_t {
  uint16_t nIds;
} fmt_profiledb_idTupleHdr_t;

void fmt_profiledb_idTupleHdr_read(fmt_profiledb_idTupleHdr_t*, const char[FMT_PROFILEDB_SZ_IdTupleHdr]);
void fmt_profiledb_idTupleHdr_write(char[FMT_PROFILEDB_SZ_IdTupleHdr], const fmt_profiledb_idTupleHdr_t*);

// Hierarchical Identifier Tuple [HIT] element
enum { FMT_PROFILEDB_SZ_IdTupleElem = 0x10 };
typedef struct fmt_profiledb_idTupleElem_t {
  uint8_t kind;
  bool isPhysical : 1;
  uint32_t logicalId;
  uint64_t physicalId;
} fmt_profiledb_idTupleElem_t;

void fmt_profiledb_idTupleElem_read(fmt_profiledb_idTupleElem_t*, const char[FMT_PROFILEDB_SZ_IdTupleElem]);
void fmt_profiledb_idTupleElem_write(char[FMT_PROFILEDB_SZ_IdTupleElem], const fmt_profiledb_idTupleElem_t*);

#define FMT_PROFILEDB_SZ_IdTuple(nIds) (FMT_PROFILEDB_SZ_IdTupleHdr + (nIds) * FMT_PROFILEDB_SZ_IdTupleElem)

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif  // FORMATS_PROFILEDB_H
