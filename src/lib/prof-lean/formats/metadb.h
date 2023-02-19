// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2023, Rice University
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

#ifndef FORMATS_METADB_H
#define FORMATS_METADB_H

#include "common.h"

#if defined(__cplusplus)
extern "C" {
#endif

/// Minor version of the meta.db format implemented here
enum { FMT_METADB_MinorVersion = 0 };

/// Check the given file start bytes for the meta.db format.
/// If minorVer != NULL, also returns the exact minor version.
enum fmt_version_t fmt_metadb_check(const char[16], uint8_t* minorVer);

/// Footer byte sequence for meta.db files.
extern const char fmt_metadb_footer[8];

//
// meta.db file
//

/// Size of the meta.db file header in serialized form
enum { FMT_METADB_SZ_FHdr = 0x90 };

/// meta.db file header, names match FORMATS.md
typedef struct fmt_metadb_fHdr_t {
  // NOTE: magic and versions are constant and cannot be adjusted
  uint64_t szGeneral;
  uint64_t pGeneral;
  uint64_t szIdNames;
  uint64_t pIdNames;
  uint64_t szMetrics;
  uint64_t pMetrics;
  uint64_t szContext;
  uint64_t pContext;
  uint64_t szStrings;
  uint64_t pStrings;
  uint64_t szModules;
  uint64_t pModules;
  uint64_t szFiles;
  uint64_t pFiles;
  uint64_t szFunctions;
  uint64_t pFunctions;
} fmt_metadb_fHdr_t;

/// Read a meta.db file header from a byte array
void fmt_metadb_fHdr_read(fmt_metadb_fHdr_t*, const char[FMT_METADB_SZ_FHdr]);

/// Write a meta.db file header into a byte array
void fmt_metadb_fHdr_write(char[FMT_METADB_SZ_FHdr], const fmt_metadb_fHdr_t*);

//
// meta.db General Properties section
//

/// meta.db General Properties section header
enum { FMT_METADB_SZ_GeneralSHdr = 0x10 };
typedef struct fmt_metadb_generalSHdr_t {
  uint64_t pTitle;
  uint64_t pDescription;
} fmt_metadb_generalSHdr_t;

void fmt_metadb_generalSHdr_read(fmt_metadb_generalSHdr_t*, const char[FMT_METADB_SZ_GeneralSHdr]);
void fmt_metadb_generalSHdr_write(char[FMT_METADB_SZ_GeneralSHdr], const fmt_metadb_generalSHdr_t*);

//
// meta.db Hierarchical Identifier Names section
//

/// meta.db Hierarchical Identifier Names section header
enum { FMT_METADB_SZ_IdNamesSHdr = 0x09 };
typedef struct fmt_metadb_idNamesSHdr_t {
  uint64_t ppNames;
  uint8_t nKinds;
} fmt_metadb_idNamesSHdr_t;

void fmt_metadb_idNamesSHdr_read(fmt_metadb_idNamesSHdr_t*, const char[FMT_METADB_SZ_IdNamesSHdr]);
void fmt_metadb_idNamesSHdr_write(char[FMT_METADB_SZ_IdNamesSHdr], const fmt_metadb_idNamesSHdr_t*);

//
// meta.db Performance Metrics section
//

/// meta.db Performance Metrics section header
enum { FMT_METADB_SZ_MetricsSHdr = 0x1b };
typedef struct fmt_metadb_metricsSHdr_t {
  uint64_t pMetrics;
  uint32_t nMetrics;
  // NOTE: The following 3 members are ignored on write
  uint8_t szMetric;
  uint8_t szScopeInst;
  uint8_t szSummary;

  uint64_t pScopes;
  uint8_t nScopes;
  // NOTE: The following member is ignored on write
  uint8_t szScope;
} fmt_metadb_metricsSHdr_t;

void fmt_metadb_metricsSHdr_read(fmt_metadb_metricsSHdr_t*, const char[FMT_METADB_SZ_MetricsSHdr]);
void fmt_metadb_metricsSHdr_write(char[FMT_METADB_SZ_MetricsSHdr], const fmt_metadb_metricsSHdr_t*);

// Metric Description structure {MD}
enum { FMT_METADB_SZ_MetricDesc = 0x20 };
typedef struct fmt_metadb_metricDesc_t {
  uint64_t pName;
  uint64_t pScopeInsts;
  uint64_t pSummaries;
  uint16_t nScopeInsts;
  uint16_t nSummaries;
} fmt_metadb_metricDesc_t;

void fmt_metadb_metricDesc_read(fmt_metadb_metricDesc_t*, const char[FMT_METADB_SZ_MetricDesc]);
void fmt_metadb_metricDesc_write(char[FMT_METADB_SZ_MetricDesc], const fmt_metadb_metricDesc_t*);

// Propagation Scope Instance structure {PSI}
enum { FMT_METADB_SZ_PropScopeInst = 0x10 };
typedef struct fmt_metadb_propScopeInst_t {
  uint64_t pScope;
  uint16_t propMetricId;
} fmt_metadb_propScopeInst_t;

void fmt_metadb_propScopeInst_read(fmt_metadb_propScopeInst_t*, const char[FMT_METADB_SZ_PropScopeInst]);
void fmt_metadb_propScopeInst_write(char[FMT_METADB_SZ_PropScopeInst], const fmt_metadb_propScopeInst_t*);

// Summary Statistic description structure {SS}
enum { FMT_METADB_SZ_SummaryStat = 0x18 };
typedef struct fmt_metadb_summaryStat_t {
  uint64_t pScope;
  uint64_t pFormula;
  uint8_t combine;
  uint16_t statMetricId;
} fmt_metadb_summaryStat_t;
enum {
  FMT_METADB_COMBINE_Sum = 0,
  FMT_METADB_COMBINE_Min = 1,
  FMT_METADB_COMBINE_Max = 2,
};

void fmt_metadb_summaryStat_read(fmt_metadb_summaryStat_t*, const char[FMT_METADB_SZ_SummaryStat]);
void fmt_metadb_summaryStat_write(char[FMT_METADB_SZ_SummaryStat], const fmt_metadb_summaryStat_t*);

  // Propagation Scope description structure {PS}
enum { FMT_METADB_SZ_PropScope = 0x10 };
typedef struct fmt_metadb_propScope_t {
  uint64_t pScopeName;
  uint8_t type;
  uint8_t propagationIndex;
} fmt_metadb_propScope_t;
enum {
  FMT_METADB_SCOPETYPE_Custom = 0,
  FMT_METADB_SCOPETYPE_Point = 1,
  FMT_METADB_SCOPETYPE_Execution = 2,
  FMT_METADB_SCOPETYPE_Transitive = 3,
};

void fmt_metadb_propScope_read(fmt_metadb_propScope_t*, const char[FMT_METADB_SZ_PropScope]);
void fmt_metadb_propScope_write(char[FMT_METADB_SZ_PropScope], const fmt_metadb_propScope_t*);

//
// meta.db Load Modules section
//

// meta.db Load Modules section header
enum { FMT_METADB_SZ_ModulesSHdr = 0x0e };
typedef struct fmt_metadb_modulesSHdr_t {
  uint64_t pModules;
  uint32_t nModules;
  // NOTE: The following member is ignored on write
  uint16_t szModule;
} fmt_metadb_modulesSHdr_t;

void fmt_metadb_modulesSHdr_read(fmt_metadb_modulesSHdr_t*, const char[FMT_METADB_SZ_ModulesSHdr]);
void fmt_metadb_modulesSHdr_write(char[FMT_METADB_SZ_ModulesSHdr], const fmt_metadb_modulesSHdr_t*);

// Load Module Specification [LMS]
enum { FMT_METADB_SZ_ModuleSpec = 0x10 };
typedef struct fmt_metadb_moduleSpec_t {
  uint64_t pPath;
} fmt_metadb_moduleSpec_t;

void fmt_metadb_moduleSpec_read(fmt_metadb_moduleSpec_t*, const char[FMT_METADB_SZ_ModuleSpec]);
void fmt_metadb_moduleSpec_write(char[FMT_METADB_SZ_ModuleSpec], const fmt_metadb_moduleSpec_t*);

//
// meta.db Source Files section
//

// meta.db Source Files section header
enum { FMT_METADB_SZ_FilesSHdr = 0x0e };
typedef struct fmt_metadb_filesSHdr_t {
  uint64_t pFiles;
  uint32_t nFiles;
  // NOTE: The following member is ignored on write
  uint16_t szFile;
} fmt_metadb_filesSHdr_t;

void fmt_metadb_filesSHdr_read(fmt_metadb_filesSHdr_t*, const char[FMT_METADB_SZ_FilesSHdr]);
void fmt_metadb_filesSHdr_write(char[FMT_METADB_SZ_FilesSHdr], const fmt_metadb_filesSHdr_t*);

// Source File Specification [SFS]
enum { FMT_METADB_SZ_FileSpec = 0x10 };
typedef struct fmt_metadb_fileSpec_t {
  bool copied : 1;
  uint64_t pPath;
} fmt_metadb_fileSpec_t;

void fmt_metadb_fileSpec_read(fmt_metadb_fileSpec_t*, const char[FMT_METADB_SZ_FileSpec]);
void fmt_metadb_fileSpec_write(char[FMT_METADB_SZ_FileSpec], const fmt_metadb_fileSpec_t*);

//
// meta.db Functions section
//

// meta.db Functions section header
enum { FMT_METADB_SZ_FunctionsSHdr = 0x0e };
typedef struct fmt_metadb_functionsSHdr_t {
  uint64_t pFunctions;
  uint32_t nFunctions;
  // NOTE: The following member is ignored on write
  uint16_t szFunction;
} fmt_metadb_functionsSHdr_t;

void fmt_metadb_functionsSHdr_read(fmt_metadb_functionsSHdr_t*, const char[FMT_METADB_SZ_FunctionsSHdr]);
void fmt_metadb_functionsSHdr_write(char[FMT_METADB_SZ_FunctionsSHdr], const fmt_metadb_functionsSHdr_t*);

// Function Specification [FS]
enum { FMT_METADB_SZ_FunctionSpec = 0x28 };
typedef struct fmt_metadb_functionSpec_t {
  uint64_t pName;
  uint64_t pModule;
  uint64_t offset;
  uint64_t pFile;
  uint32_t line;
} fmt_metadb_functionSpec_t;

void fmt_metadb_functionSpec_read(fmt_metadb_functionSpec_t*, const char[FMT_METADB_SZ_FunctionSpec]);
void fmt_metadb_functionSpec_write(char[FMT_METADB_SZ_FunctionSpec], const fmt_metadb_functionSpec_t*);

//
// meta.db Context Tree section
//

// meta.db Context Tree section header
enum { FMT_METADB_SZ_ContextsSHdr = 0x0b };
typedef struct fmt_metadb_contextsSHdr_t {
  uint64_t pEntryPoints;
  uint16_t nEntryPoints;
  // NOTE: The following member is ignored on write
  uint8_t szEntryPoint;
} fmt_metadb_contextsSHdr_t;

void fmt_metadb_contextsSHdr_read(fmt_metadb_contextsSHdr_t*, const char[FMT_METADB_SZ_ContextsSHdr]);
void fmt_metadb_contextsSHdr_write(char[FMT_METADB_SZ_ContextsSHdr], const fmt_metadb_contextsSHdr_t*);

// Entry point specification {Entry}
enum { FMT_METADB_SZ_EntryPoint = 0x20 };
typedef struct fmt_metadb_entryPoint_t {
  uint64_t szChildren;
  uint64_t pChildren;
  uint32_t ctxId;
  uint16_t entryPoint;
  uint64_t pPrettyName;
} fmt_metadb_entryPoint_t;
enum {
  FMT_METADB_ENTRYPOINT_UNKNOWN_ENTRY = 0,
  FMT_METADB_ENTRYPOINT_MAIN_THREAD = 1,
  FMT_METADB_ENTRYPOINT_APPLICATION_THREAD = 2,
};

void fmt_metadb_entryPoint_read(fmt_metadb_entryPoint_t*, const char[FMT_METADB_SZ_EntryPoint]);
void fmt_metadb_entryPoint_write(char[FMT_METADB_SZ_EntryPoint], const fmt_metadb_entryPoint_t*);

// Context specification {Ctx}
enum {
  FMT_METADB_MINSZ_Context = 0x20,
#define FMT_METADB_SZ_Context(nFlexWords) (FMT_METADB_MINSZ_Context + 8 * (nFlexWords))
  FMT_METADB_MAXSZ_Context = FMT_METADB_SZ_Context(5),
};
typedef struct fmt_metadb_context_t {
  uint64_t szChildren;
  uint64_t pChildren;
  uint32_t ctxId;
  uint8_t relation;
  uint8_t lexicalType;
  // NOTE: The following member is ignored on write
  uint8_t nFlexWords;
  uint16_t propagation;

  // NOTE: hasFunction = pFunction != 0
  uint64_t pFunction;
  // NOTE: hasSrcLoc = pFile != 0
  uint64_t pFile;
  uint32_t line;
  // NOTE: hasPoint = pModule != 0
  uint64_t pModule;
  uint64_t offset;
} fmt_metadb_context_t;
enum {
  FMT_METADB_RELATION_LexicalNest = 0,
  FMT_METADB_RELATION_Call = 1,
  FMT_METADB_RELATION_InlinedCall = 2,

  FMT_METADB_LEXTYPE_Function = 0,
  FMT_METADB_LEXTYPE_Loop = 1,
  FMT_METADB_LEXTYPE_Line = 2,
  FMT_METADB_LEXTYPE_Instruction = 3,
};

// Returns false if the read context spec is invalid (nFlexWords is too small)
bool fmt_metadb_context_read(fmt_metadb_context_t*, const char*);
// Returns the total size of the written context spec, in bytes
size_t fmt_metadb_context_write(char*, const fmt_metadb_context_t*);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // METADB_FMT_H
