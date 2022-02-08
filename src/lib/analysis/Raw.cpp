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
// File:
//   $HeadURL$
//
// Purpose:
//   [The purpose of this file]
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
//***************************************************************************

#include <functional>
#include <iostream>
#include <string>

using std::string;

#define __STDC_FORMAT_MACROS
#include "Raw.hpp"
#include "Util.hpp"

#include "lib/prof-lean/formats/metadb.h"
#include "lib/prof-lean/formats/primitive.h"
#include "lib/prof-lean/formats/profiledb.h"
#include "lib/prof-lean/hpcfmt.h"
#include "lib/prof-lean/hpcio.h"
#include "lib/prof-lean/hpcrun-fmt.h"
#include "lib/prof-lean/id-tuple.h"
#include "lib/prof-lean/tracedb.h"
#include "lib/prof/CallPath-Profile.hpp"
#include "lib/prof/cms-format.h"
#include "lib/prof/Flat-ProfileData.hpp"
#include "lib/support/diagnostics.h"

#include <inttypes.h>

void Analysis::Raw::writeAsText(/*destination,*/ const char* filenm, bool sm_easyToGrep) {
  using namespace Analysis::Util;

  ProfType_t ty = getProfileType(filenm);
  if (ty == ProfType_Callpath) {
    writeAsText_callpath(filenm, sm_easyToGrep);
  } else if (ty == ProfType_CallpathMetricDB) {
    writeAsText_callpathMetricDB(filenm);
  } else if (ty == ProfType_CallpathTrace) {
    writeAsText_callpathTrace(filenm);
  } else if (ty == ProfType_Flat) {
    writeAsText_flat(filenm);
  } else if (ty == ProfType_ProfileDB) {
    writeAsText_profiledb(filenm, sm_easyToGrep);
  } else if (ty == ProfType_SparseDBcct) {  // YUMENG
    writeAsText_sparseDBcct(filenm, sm_easyToGrep);
  } else if (ty == ProfType_TraceDB) {  // YUMENG
    writeAsText_tracedb(filenm);
  } else if (ty == ProfType_MetaDB) {
    writeAsText_metadb(filenm);
  } else {
    DIAG_Die(DIAG_Unimplemented);
  }
}

void Analysis::Raw::writeAsText_callpath(const char* filenm, bool sm_easyToGrep) {
  if (!filenm) {
    return;
  }
  Prof::CallPath::Profile* prof = NULL;
  try {
    prof = Prof::CallPath::Profile::make(filenm, 0 /*rFlags*/, stdout, sm_easyToGrep);
  } catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }
  delete prof;
}

bool Analysis::Raw::traceHdr_sorter(trace_hdr_t const& lhs, trace_hdr_t const& rhs) {
  return lhs.start < rhs.start;
}

void Analysis::Raw::sortTraceHdrs_onStarts(trace_hdr_t* x, uint32_t num_t) {
  std::sort(x, x + num_t, &traceHdr_sorter);
}

void Analysis::Raw::writeAsText_profiledb(const char* filenm, bool easygrep) {
  if (!filenm)
    return;

  try {
    FILE* fs = hpcio_fopen_r(filenm);
    if (!fs) {
      DIAG_Throw("error opening profile.db file '" << filenm << "'");
    }

    {
      char buf[16];
      if (fread(buf, 1, sizeof buf, fs) < sizeof buf)
        DIAG_Throw("eof/error reading profile.db format header");
      uint8_t minor;
      auto ver = fmt_profiledb_check(buf, &minor);
      switch (ver) {
      case fmt_version_invalid: DIAG_Throw("Not a profile.db file");
      case fmt_version_backward: DIAG_Throw("Incompatible profile.db version (too old)");
      case fmt_version_major:
        DIAG_Throw("Incompatible profile.db version (major version mismatch)");
      case fmt_version_forward:
        DIAG_Msg(
            0, "WARNING: Minor version mismatch (" << (unsigned int)minor << " > "
                                                   << FMT_PROFILEDB_MinorVersion
                                                   << "), some fields may be missing");
        break;
      default: break;
      }
      if (ver < 0)
        DIAG_Throw("error parsing profile.db format header");

      std::cout << "profile.db version " << FMT_DB_MajorVersion << "." << (unsigned int)minor
                << "\n";
    }

    fmt_profiledb_fHdr_t fhdr;
    {  // profile.db file header
      rewind(fs);
      char buf[FMT_PROFILEDB_SZ_FHdr];
      if (fread(buf, 1, sizeof buf, fs) < sizeof buf)
        DIAG_Throw("eof reading profile.db file header");
      fmt_profiledb_fHdr_read(&fhdr, buf);
      std::cout << std::hex
                << "[file header:\n"
                   "  (szProfileInfos: 0x"
                << fhdr.szProfileInfos << ") (pProfileInfos: 0x" << fhdr.pProfileInfos
                << ")\n"
                   "  (szIdTuples: 0x"
                << fhdr.szIdTuples << ") (pIdTuples: 0x" << fhdr.pIdTuples
                << ")\n"
                   "]\n"
                << std::dec;
    }

    std::vector<fmt_profiledb_profInfo_t> pis;
    {  // Profile Info section
      if (fseeko(fs, fhdr.pProfileInfos, SEEK_SET) < 0)
        DIAG_Throw("error seeking to profile.db Profile Info section");
      std::vector<char> buf(fhdr.szProfileInfos);
      if (fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading profile.db Profile Info section");

      fmt_profiledb_profInfoSHdr_t shdr;
      fmt_profiledb_profInfoSHdr_read(&shdr, buf.data());
      std::cout << std::hex
                << "[profile info:\n"
                   "  (pProfiles: 0x"
                << shdr.pProfiles << ") (nProfiles: " << std::dec << shdr.nProfiles << std::hex
                << ")\n"
                   "  (szProfile: 0x"
                << (unsigned int)shdr.szProfile << " >= 0x" << FMT_PROFILEDB_SZ_ProfInfo << ")\n";
      pis.reserve(shdr.nProfiles);
      for (uint32_t i = 0; i < shdr.nProfiles; i++) {
        fmt_profiledb_profInfo_t pi;
        fmt_profiledb_profInfo_read(
            &pi, &buf[shdr.pProfiles + i * shdr.szProfile - fhdr.pProfileInfos]);
        pis.push_back(pi);
        std::cout << "  [pProfiles[" << i
                  << "]:\n"
                     "    [profile-major sparse value block:\n"
                     "      (nValues: "
                  << std::dec << pi.valueBlock.nValues << std::hex
                  << ")"
                     " (pValues: 0x"
                  << pi.valueBlock.pValues
                  << ")\n"
                     "      (nCtxs: "
                  << std::dec << pi.valueBlock.nCtxs << std::hex
                  << ")"
                     " (pCtxIndices: 0x"
                  << pi.valueBlock.pCtxIndices
                  << ")\n"
                     "    ]\n"
                     "    (pIdTuple: 0x"
                  << pi.pIdTuple
                  << ")\n"
                     "  ]\n";
      }
      std::cout << "]\n" << std::dec;
    }

    {  // Hierarchical Identifier Tuple section
      if (fseeko(fs, fhdr.pIdTuples, SEEK_SET) < 0)
        DIAG_Throw("error seeking to profile.db Identifier Tuple section");
      std::vector<char> buf(fhdr.szIdTuples);
      if (fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading profile.db Identifier Tuple section");

      std::cout << "[hierarchical identifier tuples:\n" << std::hex;

      // In actuality, each tuple is only identified by the Profile Info that
      // references it. So loop through the PIs in order by id tuple
      std::sort(pis.begin(), pis.end(), [](const auto& a, const auto& b) {
        return a.pIdTuple < b.pIdTuple;
      });
      for (const auto& pi : pis) {
        if (pi.pIdTuple == 0)
          continue;
        fmt_profiledb_idTupleHdr_t hdr;
        fmt_profiledb_idTupleHdr_read(&hdr, &buf[pi.pIdTuple - fhdr.pIdTuples]);
        std::cout << "  (0x" << pi.pIdTuple << ") [";
        for (uint16_t i = 0; i < hdr.nIds; i++) {
          fmt_profiledb_idTupleElem_t elem;
          fmt_profiledb_idTupleElem_read(
              &elem, &buf
                         [pi.pIdTuple - fhdr.pIdTuples + FMT_PROFILEDB_SZ_IdTupleHdr
                          + i * FMT_PROFILEDB_SZ_IdTupleElem]);
          std::cout << (i > 0 ? " (" : "(");
          switch (elem.kind) {
          case IDTUPLE_SUMMARY: std::cout << "SUMMARY"; break;
          case IDTUPLE_NODE: std::cout << "NODE"; break;
          case IDTUPLE_THREAD: std::cout << "THREAD"; break;
          case IDTUPLE_GPUDEVICE: std::cout << "GPUDEVICE"; break;
          case IDTUPLE_GPUCONTEXT: std::cout << "GPUCONTEXT"; break;
          case IDTUPLE_GPUSTREAM: std::cout << "GPUSTREAM"; break;
          case IDTUPLE_CORE: std::cout << "CORE";
          }
          std::cout << " " << std::dec << elem.logicalId << std::hex;
          if (elem.isPhysical)
            std::cout << " / 0x" << elem.physicalId;
          std::cout << ")";
        }
        std::cout << "]\n";
      }
      std::cout << "]\n" << std::dec;
    }

    // Rest of the file is profile data. Pointers are listed in the PIs, we
    // output in file order.
    std::sort(pis.begin(), pis.end(), [](const auto& a, const auto& b) {
      return a.valueBlock.pValues < b.valueBlock.pValues;
    });
    for (const auto& pi : pis) {
      const auto& psvb = pi.valueBlock;

      if (fseeko(fs, psvb.pValues, SEEK_SET) < 0)
        DIAG_Throw("error seeking to profile.db profile data segment");
      std::vector<char> buf(psvb.pCtxIndices + psvb.nCtxs * FMT_PROFILEDB_SZ_CIdx - psvb.pValues);
      if (fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading profile.db profile data segment");

      std::cout << std::hex << "(0x" << psvb.pValues << ") [profile data:\n" << std::dec;
      if (!easygrep) {
        std::cout << "  [metric-value pairs:\n";
        for (uint64_t i = 0; i < psvb.nValues; i++) {
          fmt_profiledb_mVal_t val;
          fmt_profiledb_mVal_read(&val, &buf[i * FMT_PROFILEDB_SZ_MVal]);
          std::cout << "    [" << i << "] (metric id: " << val.metricId << ", value: " << val.value
                    << ")\n";
        }
        std::cout << "  ]\n  [context-index pairs:\n";
        for (uint64_t i = 0; i < psvb.nCtxs; i++) {
          fmt_profiledb_cIdx_t idx;
          fmt_profiledb_cIdx_read(
              &idx, &buf[psvb.pCtxIndices - psvb.pValues + i * FMT_PROFILEDB_SZ_CIdx]);
          std::cout << "    (ctx id: " << idx.ctxId << ", index: " << idx.startIndex << ")\n";
        }
        std::cout << "  ]\n";
      } else {
        fmt_profiledb_cIdx_t idx;
        fmt_profiledb_cIdx_read(&idx, &buf[psvb.pCtxIndices - psvb.pValues]);
        for (uint64_t i = 1; i <= psvb.nCtxs; i++) {
          std::cout << "  (ctx id: " << idx.ctxId << ")";

          uint64_t startIndex = idx.startIndex;
          uint64_t endIndex = psvb.nValues;
          if (i < psvb.nCtxs) {
            fmt_profiledb_cIdx_read(
                &idx, &buf[psvb.pCtxIndices - psvb.pValues + i * FMT_PROFILEDB_SZ_CIdx]);
            endIndex = idx.startIndex;
          }

          for (uint64_t j = startIndex; j < endIndex; j++) {
            fmt_profiledb_mVal_t val;
            fmt_profiledb_mVal_read(&val, &buf[j * FMT_PROFILEDB_SZ_MVal]);
            std::cout << " (metric id: " << val.metricId << ", value: " << val.value << ")";
          }
          std::cout << "\n";
        }
      }
      std::cout << "]\n" << std::dec;
    }

    {  // File footer
      char buf[sizeof fmt_profiledb_footer + 1];
      if (fseeko(fs, -sizeof fmt_profiledb_footer, SEEK_END) < 0)
        DIAG_Throw("error seeking to profile.db footer");
      if (fread(buf, 1, sizeof fmt_profiledb_footer, fs) < sizeof fmt_profiledb_footer)
        DIAG_Throw("error reading profile.db footer");
      buf[sizeof fmt_profiledb_footer] = '\0';
      std::cout << "[footer: '" << buf << "'";
      if (memcmp(buf, fmt_profiledb_footer, sizeof fmt_profiledb_footer) == 0)
        std::cout << ", OK]\n";
      else
        std::cout << ", INVALID]\n";
    }
  } catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }
}

#define MULTIPLE_8(X) (((X) + 7) / 8 * 8)

// YUMENG
void Analysis::Raw::writeAsText_sparseDBcct(const char* filenm, bool easygrep) {
  if (!filenm) {
    return;
  }

  try {
    FILE* fs = hpcio_fopen_r(filenm);
    if (!fs) {
      DIAG_Throw("error opening cct sparse file '" << filenm << "'");
    }

    cms_hdr_t hdr;
    int ret = cms_hdr_fread(&hdr, fs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("error reading hdr from sparse metrics file '" << filenm << "'");
    }
    cms_hdr_fprint(&hdr, stdout);

    fseek(fs, hdr.ctx_info_sec_ptr, SEEK_SET);
    uint32_t num_ctx = hdr.num_ctx;
    cms_ctx_info_t* x;
    ret = cms_ctx_info_fread(&x, num_ctx, fs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("error reading cct information from sparse metrics file '" << filenm << "'");
    }
    cms_ctx_info_fprint(num_ctx, x, stdout);

    fseek(fs, hdr.ctx_info_sec_ptr + (MULTIPLE_8(hdr.ctx_info_sec_size)), SEEK_SET);
    for (uint i = 0; i < num_ctx; i++) {
      if (x[i].num_vals != 0) {
        cct_sparse_metrics_t csm;
        csm.ctx_id = x[i].ctx_id;
        csm.num_vals = x[i].num_vals;
        csm.num_nzmids = x[i].num_nzmids;
        ret = cms_sparse_metrics_fread(&csm, fs);
        if (ret != HPCFMT_OK) {
          DIAG_Throw("error reading cct data from sparse metrics file '" << filenm << "'");
        }
        cms_sparse_metrics_fprint(&csm, stdout, "  ", easygrep);
        cms_sparse_metrics_free(&csm);
      }
    }

    uint64_t footer;
    fread(&footer, sizeof(footer), 1, fs);
    if (footer != CCTDBftr)
      DIAG_Throw("'" << filenm << "' is incomplete");
    fprintf(stdout, "CCTDB FOOTER CORRECT, FILE COMPLETE\n");

    cms_ctx_info_free(&x);

    hpcio_fclose(fs);
  } catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }
}

// YUMENG
void Analysis::Raw::writeAsText_tracedb(const char* filenm) {
  if (!filenm) {
    return;
  }

  try {
    FILE* fs = hpcio_fopen_r(filenm);
    if (!fs) {
      DIAG_Throw("error opening tracedb file '" << filenm << "'");
    }

    tracedb_hdr_t hdr;
    int ret = tracedb_hdr_fread(&hdr, fs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("error reading hdr from tracedb file '" << filenm << "'");
    }
    tracedb_hdr_fprint(&hdr, stdout);

    uint32_t num_t = hdr.num_trace;

    fseek(fs, hdr.trace_hdr_sec_ptr, SEEK_SET);
    trace_hdr_t* x;
    ret = trace_hdrs_fread(&x, num_t, fs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("error reading trace hdrs from tracedb file '" << filenm << "'");
    }
    trace_hdrs_fprint(num_t, x, stdout);

    sortTraceHdrs_onStarts(x, num_t);
    fseek(fs, hdr.trace_hdr_sec_ptr + (MULTIPLE_8(hdr.trace_hdr_sec_size)), SEEK_SET);
    for (uint i = 0; i < num_t; i++) {
      uint64_t start = x[i].start;
      uint64_t end = x[i].end;
      hpctrace_fmt_datum_t* trace_data;
      ret = tracedb_data_fread(&trace_data, (end - start) / timepoint_SIZE, {0}, fs);
      if (ret != HPCFMT_OK) {
        DIAG_Throw("error reading trace data from tracedb file '" << filenm << "'");
      }
      tracedb_data_fprint(
          trace_data, (end - start) / timepoint_SIZE, x[i].prof_info_idx, {0}, stdout);
      tracedb_data_free(&trace_data);
    }

    if (fgetc(fs) == EOF)
      fprintf(stdout, "END OF FILE\n");
    else
      fprintf(stdout, "SHOULD BE EOF, BUT NOT, SOME ERRORS HAPPENED\n");

    /*
    uint64_t footer;
    fread(&footer, sizeof(footer), 1, fs);
    if(footer != TRACDBft) DIAG_Throw("'" << filenm << "' is incomplete");
    fprintf(stdout, "TRACEDB FOOTER CORRECT, FILE COMPLETE\n");
    */

    trace_hdrs_free(&x);
    hpcio_fclose(fs);
  } catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }
}

void Analysis::Raw::writeAsText_metadb(const char* filenm) {
  if (!filenm)
    return;

  try {
    FILE* fs = hpcio_fopen_r(filenm);
    if (!fs) {
      DIAG_Throw("error opening meta.db file '" << filenm << "'");
    }

    {
      char buf[16];
      if (fread(buf, 1, sizeof buf, fs) < sizeof buf)
        DIAG_Throw("eof/error reading meta.db format header");
      uint8_t minor;
      auto ver = fmt_metadb_check(buf, &minor);
      switch (ver) {
      case fmt_version_invalid: DIAG_Throw("Not a meta.db file");
      case fmt_version_backward: DIAG_Throw("Incompatible meta.db version (too old)");
      case fmt_version_major: DIAG_Throw("Incompatible meta.db version (major version mismatch)");
      case fmt_version_forward:
        DIAG_Msg(
            0, "WARNING: Minor version mismatch (" << (unsigned int)minor << " > "
                                                   << FMT_METADB_MinorVersion
                                                   << "), some fields may be missing");
        break;
      default: break;
      }
      if (ver < 0)
        DIAG_Throw("error parsing meta.db format header");

      std::cout << "meta.db version " << FMT_DB_MajorVersion << "." << (unsigned int)minor << "\n";
    }

    fmt_metadb_fHdr_t fhdr;
    {  // meta.db file header
      rewind(fs);
      char buf[FMT_METADB_SZ_FHdr];
      if (fread(buf, 1, sizeof buf, fs) < sizeof buf)
        DIAG_Throw("eof reading meta.db file header");
      fmt_metadb_fHdr_read(&fhdr, buf);
      std::cout << std::hex
                << "[file header:\n"
                   "  (szGeneral: 0x"
                << fhdr.szGeneral << ") (pGeneral: 0x" << fhdr.pGeneral
                << ")\n"
                   "  (szIdNames: 0x"
                << fhdr.szIdNames << ") (pIdNames: 0x" << fhdr.pIdNames
                << ")\n"
                   "  (szMetrics: 0x"
                << fhdr.szMetrics << ") (pMetrics: 0x" << fhdr.pMetrics
                << ")\n"
                   "  (szContext: 0x"
                << fhdr.szContext << ") (pContext: 0x" << fhdr.pContext
                << ")\n"
                   "  (szStrings: 0x"
                << fhdr.szStrings << ") (pStrings: 0x" << fhdr.pStrings
                << ")\n"
                   "  (szModules: 0x"
                << fhdr.szModules << ") (pModules: 0x" << fhdr.pModules
                << ")\n"
                   "  (szFiles: 0x"
                << fhdr.szFiles << ") (pFiles: 0x" << fhdr.pFiles
                << ")\n"
                   "  (szFunctions: 0x"
                << fhdr.szFunctions << ") (pFunctions: 0x" << fhdr.pFunctions
                << ")\n"
                   "]\n"
                << std::dec;
    }

    {  // General Properties section
      if (fseeko(fs, fhdr.pGeneral, SEEK_SET) < 0)
        DIAG_Throw("error seeking to meta.db General Properties section");
      std::vector<char> buf(fhdr.szGeneral);
      if (fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading meta.db General Properties section");

      fmt_metadb_generalSHdr_t gphdr;
      fmt_metadb_generalSHdr_read(&gphdr, buf.data());
      std::cout << std::hex
                << "[general properties:\n"
                   "  (pTitle: 0x"
                << gphdr.pTitle << " = &\"" << &buf.at(gphdr.pTitle - fhdr.pGeneral)
                << "\")\n"
                   "  (pDescription: 0x"
                << gphdr.pDescription << " = &\"\"\"\n"
                << &buf.at(gphdr.pDescription - fhdr.pGeneral)
                << "\n"
                   "\"\"\")\n"
                   "]\n"
                << std::dec;
    }

    {  // Identifier Names section
      if (fseeko(fs, fhdr.pIdNames, SEEK_SET) < 0)
        DIAG_Throw("error seeking to meta.db Identifier Names section");
      std::vector<char> buf(fhdr.szIdNames);
      if (fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading meta.db Identifier Names section");

      fmt_metadb_idNamesSHdr_t idhdr;
      fmt_metadb_idNamesSHdr_read(&idhdr, buf.data());
      std::cout << std::hex
                << "[hierarchical identifier names:\n"
                   "  (ppNames: 0x"
                << idhdr.ppNames << ") (nKinds: " << std::dec << (unsigned int)idhdr.nKinds << ")\n"
                << std::hex;
      for (unsigned int i = 0; i < idhdr.nKinds; i++) {
        auto pName = fmt_u64_read(&buf.at(idhdr.ppNames + 8 * i - fhdr.pIdNames));
        std::cout << "    (ppNames[" << i << "]: 0x" << pName << " = &\""
                  << &buf.at(pName - fhdr.pIdNames) << "\")\n";
      }
      std::cout << "]\n" << std::dec;
    }

    {  // Performance Metrics section
      if (fseeko(fs, fhdr.pMetrics, SEEK_SET) < 0)
        DIAG_Throw("error seeking to meta.db Performance Metrics section");
      std::vector<char> buf(fhdr.szMetrics);
      if (fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading meta.db Performance Metrics section");

      fmt_metadb_metricsSHdr_t mhdr;
      fmt_metadb_metricsSHdr_read(&mhdr, buf.data());
      std::cout << std::hex
                << "[performance metrics:\n"
                   "  (pMetrics: 0x"
                << mhdr.pMetrics << ") (nMetrics: " << std::dec << mhdr.nMetrics << std::hex
                << ")\n"
                   "  (szMetric: 0x"
                << (unsigned int)mhdr.szMetric << " >= 0x" << FMT_METADB_SZ_MetricDesc
                << ")\n"
                   "  (szScope: 0x"
                << (unsigned int)mhdr.szScope << " >= 0x" << FMT_METADB_SZ_MetricScope
                << ")\n"
                   "  (szSummary: 0x"
                << (unsigned int)mhdr.szSummary << " >= 0x" << FMT_METADB_SZ_MetricSummary << ")\n";
      for (unsigned long i = 0; i < mhdr.nMetrics; i++) {
        fmt_metadb_metricDesc_t mdesc;
        fmt_metadb_metricDesc_read(
            &mdesc, &buf.at(mhdr.pMetrics + mhdr.szMetric * i - fhdr.pMetrics));
        std::cout << "  [pMetrics[" << std::dec << i << std::hex
                  << "]:\n"
                     "    (pName: 0x"
                  << mdesc.pName << " = &\"" << &buf.at(mdesc.pName - fhdr.pMetrics)
                  << "\")\n"
                     "    (nScopes: "
                  << std::dec << mdesc.nScopes << std::hex
                  << ")"
                     " (pScopes: 0x"
                  << mdesc.pScopes << ")\n";
        for (unsigned int j = 0; j < mdesc.nScopes; j++) {
          fmt_metadb_metricScope_t scope;
          fmt_metadb_metricScope_read(
              &scope, &buf.at(mdesc.pScopes + mhdr.szScope * j - fhdr.pMetrics));
          std::cout << "    [pScopes[" << std::dec << j << std::hex
                    << "]:\n"
                       "      (pScope: 0x"
                    << scope.pScope << " = &\"" << &buf.at(scope.pScope - fhdr.pMetrics)
                    << "\")\n"
                       "      (nSummaries: "
                    << std::dec << scope.nSummaries
                    << ")\n"
                       "      (propMetricId: "
                    << scope.propMetricId
                    << ")\n"
                       "      (pSummaries: 0x"
                    << std::hex << scope.pSummaries << ")\n";
          for (unsigned int k = 0; k < scope.nSummaries; k++) {
            fmt_metadb_metricSummary_t summary;
            fmt_metadb_metricSummary_read(
                &summary, &buf.at(scope.pSummaries + mhdr.szSummary * k - fhdr.pMetrics));
            std::cout << "      [pSummaries[" << std::dec << k << std::hex
                      << "]:\n"
                         "        (pFormula: 0x"
                      << summary.pFormula << " = &\"" << &buf.at(summary.pFormula - fhdr.pMetrics)
                      << "\")\n"
                         "        (combine: "
                      << std::dec << (unsigned int)summary.combine << " = ";
            switch (summary.combine) {
            case FMT_METADB_COMBINE_Sum: std::cout << "sum"; break;
            case FMT_METADB_COMBINE_Min: std::cout << "min"; break;
            case FMT_METADB_COMBINE_Max: std::cout << "max"; break;
            default: std::cout << "???";
            }
            std::cout << ")\n"
                         "        (statMetricId: "
                      << std::dec << summary.statMetricId
                      << ")\n"
                         "      ]\n";
          }
          std::cout << "    ]\n";
        }
        std::cout << "  ]\n";
      }
      std::cout << "]\n" << std::dec;
    }

    std::vector<char> strings;
    {  // Common String Table section
      if (fseeko(fs, fhdr.pStrings, SEEK_SET) < 0)
        DIAG_Throw("error seeking to meta.db Common String Table section");
      strings = std::vector<char>(fhdr.szStrings);
      if (fread(strings.data(), 1, strings.size(), fs) < strings.size())
        DIAG_Throw("eof reading meta.db Common String Table section");
    }

    {  // Load Modules section
      if (fseeko(fs, fhdr.pModules, SEEK_SET) < 0)
        DIAG_Throw("error seeking to meta.db Load Modules section");
      std::vector<char> buf(fhdr.szModules);
      if (fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading meta.db Load Modules section");

      fmt_metadb_modulesSHdr_t shdr;
      fmt_metadb_modulesSHdr_read(&shdr, buf.data());
      std::cout << std::hex
                << "[load modules:\n"
                   "  (pModules: 0x"
                << shdr.pModules << ") (nModules: " << std::dec << shdr.nModules << std::hex
                << ")\n"
                   "  (szModule: 0x"
                << shdr.szModule << " >= 0x" << FMT_METADB_SZ_ModuleSpec << ")\n";
      for (unsigned long i = 0; i < shdr.nModules; i++) {
        fmt_metadb_moduleSpec_t mspec;
        auto off = shdr.pModules + shdr.szModule * i;
        fmt_metadb_moduleSpec_read(&mspec, &buf.at(off - fhdr.pModules));
        std::cout << "  [pModules[" << std::dec << i << std::hex << "]: (0x" << off
                  << ")\n"
                     "    (pPath: 0x"
                  << mspec.pPath << " = &\"" << &strings.at(mspec.pPath - fhdr.pStrings)
                  << "\")\n"
                     "  ]\n";
      }
      std::cout << "]\n" << std::dec;
    }

    {  // Source Files section
      if (fseeko(fs, fhdr.pFiles, SEEK_SET) < 0)
        DIAG_Throw("error seeking to meta.db Source Files section");
      std::vector<char> buf(fhdr.szFiles);
      if (fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading meta.db Source Files section");

      fmt_metadb_filesSHdr_t shdr;
      fmt_metadb_filesSHdr_read(&shdr, buf.data());
      std::cout << std::hex
                << "[source files:\n"
                   "  (pFiles: 0x"
                << shdr.pFiles << ") (nFiles: " << std::dec << shdr.nFiles << std::hex
                << ")\n"
                   "  (szFile: 0x"
                << shdr.szFile << " >= 0x" << FMT_METADB_SZ_FileSpec << ")\n";
      for (unsigned long i = 0; i < shdr.nFiles; i++) {
        fmt_metadb_fileSpec_t fspec;
        auto off = shdr.pFiles + shdr.szFile * i;
        fmt_metadb_fileSpec_read(&fspec, &buf.at(off - fhdr.pFiles));
        std::cout << "  [pFiles[" << std::dec << i << std::hex << "]: (0x" << off
                  << ")\n"
                     "    (pPath: 0x"
                  << fspec.pPath << " = &\"" << &strings.at(fspec.pPath - fhdr.pStrings)
                  << "\")\n"
                     "  ]\n";
      }
      std::cout << "]\n" << std::dec;
    }

    {  // Functions section
      if (fseeko(fs, fhdr.pFunctions, SEEK_SET) < 0)
        DIAG_Throw("error seeking to meta.db Functions section");
      std::vector<char> buf(fhdr.szFunctions);
      if (fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading meta.db Functions section");

      fmt_metadb_functionsSHdr_t shdr;
      fmt_metadb_functionsSHdr_read(&shdr, buf.data());
      std::cout << std::hex
                << "[functions:\n"
                   "  (pFunctions: 0x"
                << shdr.pFunctions << ") (nFunctions: " << std::dec << shdr.nFunctions << std::hex
                << ")\n"
                   "  (szFunction: 0x"
                << shdr.szFunction << " >= 0x" << FMT_METADB_SZ_FunctionSpec << ")\n";
      for (unsigned long i = 0; i < shdr.nFunctions; i++) {
        fmt_metadb_functionSpec_t fspec;
        auto off = shdr.pFunctions + shdr.szFunction * i;
        fmt_metadb_functionSpec_read(&fspec, &buf.at(off - fhdr.pFunctions));

        std::cout << "  [pFunctions[" << std::dec << i << std::hex << "]: (0x" << off
                  << ")\n"
                     "    (pName: 0x"
                  << fspec.pName << " = &\"" << &strings.at(fspec.pName - fhdr.pStrings)
                  << "\")\n"
                     "    (pModule: 0x"
                  << fspec.pModule << ") (offset: 0x" << fspec.offset
                  << ")\n"
                     "    (pFile: 0x"
                  << fspec.pFile << ") (line: " << std::dec << fspec.line << std::hex
                  << ")\n"
                     "  ]\n";
      }
      std::cout << "]\n" << std::dec;
    }

    {  // Context Tree section
      if (fseeko(fs, fhdr.pContext, SEEK_SET) < 0)
        DIAG_Throw("error seeking to meta.db Context Tree section");
      std::vector<char> buf(fhdr.szContext);
      if (fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading meta.db Context Tree section");

      const auto indent = [](unsigned int level) -> std::ostream& {
        for (unsigned int l = 0; l < level; l++)
          std::cout << "  ";
        return std::cout;
      };
      const std::function<void(unsigned int, uint64_t, uint64_t)> print =
          [&](unsigned int l, uint64_t pCtxs, uint64_t szCtxs) {
            while (szCtxs > 0) {
              if (szCtxs < FMT_METADB_MINSZ_Context) {
                indent(l) << "ERROR: not enough bytes for next context, terminating iteration\n";
                return;
              }
              fmt_metadb_context_t ctx;
              if (!fmt_metadb_context_read(&ctx, &buf.at(pCtxs - fhdr.pContext))) {
                indent(l) << "ERROR: invalid next context, terminating iteration\n";
                return;
              }
              indent(l) << std::dec << "[context: (relation: "
                        << (ctx.relation == FMT_METADB_RELATION_LexicalNest   ? "lexical nest"
                            : ctx.relation == FMT_METADB_RELATION_Call        ? "call"
                            : ctx.relation == FMT_METADB_RELATION_InlinedCall ? "inlined call"
                                                                              : "???")
                        << ") (ctxId: " << ctx.ctxId
                        << ")"
                           " (0x"
                        << std::hex << pCtxs << ")\n";
              indent(l) << std::hex << "  (szChildren: 0x" << ctx.szChildren << ") (pChildren: 0x"
                        << ctx.pChildren << ")\n";
              indent(l) << std::hex << "  (lexicalType: "
                        << (ctx.lexicalType == FMT_METADB_LEXTYPE_Function      ? "function"
                            : ctx.lexicalType == FMT_METADB_LEXTYPE_Line        ? "line"
                            : ctx.lexicalType == FMT_METADB_LEXTYPE_Loop        ? "loop"
                            : ctx.lexicalType == FMT_METADB_LEXTYPE_Instruction ? "instruction"
                                                                                : "???")
                        << ")\n";
              indent(l) << std::hex << "  (pFunction: 0x" << ctx.pFunction << ")\n";
              indent(l) << std::hex << "  (pFile: 0x" << ctx.pFile << ") (line: " << std::dec
                        << ctx.line << ")\n";
              indent(l) << std::hex << "  (pModule: 0x" << ctx.pModule << ") (offset: 0x"
                        << ctx.offset << ")\n";
              print(l + 1, ctx.pChildren, ctx.szChildren);
              indent(l) << "]\n";
              if (szCtxs < (uint64_t)FMT_METADB_SZ_Context(ctx.nFlexWords)) {
                indent(l) << "ERROR: array size too short!";
                break;
              } else {
                szCtxs -= FMT_METADB_SZ_Context(ctx.nFlexWords);
                pCtxs += FMT_METADB_SZ_Context(ctx.nFlexWords);
              }
            };
          };

      fmt_metadb_contextsSHdr_t shdr;
      fmt_metadb_contextsSHdr_read(&shdr, buf.data());
      std::cout << std::hex
                << "[context tree:\n"
                   "  (szRoots: 0x"
                << shdr.szRoots << ") (pRoots: 0x" << shdr.pRoots << ")\n";
      print(0, shdr.pRoots, shdr.szRoots);
      std::cout << "]\n";
    }

    {  // File footer
      char buf[sizeof fmt_metadb_footer + 1];
      if (fseeko(fs, -sizeof fmt_metadb_footer, SEEK_END) < 0)
        DIAG_Throw("error seeking to meta.db footer");
      if (fread(buf, 1, sizeof fmt_metadb_footer, fs) < sizeof fmt_metadb_footer)
        DIAG_Throw("error reading meta.db footer");
      buf[sizeof fmt_metadb_footer] = '\0';
      std::cout << "[footer: '" << buf << "'";
      if (memcmp(buf, fmt_metadb_footer, sizeof fmt_metadb_footer) == 0)
        std::cout << ", OK]\n";
      else
        std::cout << ", INVALID]\n";
    }
  } catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }
}

void Analysis::Raw::writeAsText_callpathMetricDB(const char* filenm) {
  if (!filenm) {
    return;
  }

  try {
    FILE* fs = hpcio_fopen_r(filenm);
    if (!fs) {
      DIAG_Throw("error opening metric-db file '" << filenm << "'");
    }

    hpcmetricDB_fmt_hdr_t hdr;
    int ret = hpcmetricDB_fmt_hdr_fread(&hdr, fs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("error reading metric-db file '" << filenm << "'");
    }

    hpcmetricDB_fmt_hdr_fprint(&hdr, stdout);

    for (uint nodeId = 1; nodeId < hdr.numNodes + 1; ++nodeId) {
      fprintf(stdout, "(%6u: ", nodeId);
      for (uint mId = 0; mId < hdr.numMetrics; ++mId) {
        double mval = 0;
        ret = hpcfmt_real8_fread(&mval, fs);
        if (ret != HPCFMT_OK) {
          DIAG_Throw("error reading trace file '" << filenm << "'");
        }
        fprintf(stdout, "%12g ", mval);
      }
      fprintf(stdout, ")\n");
    }

    hpcio_fclose(fs);
  } catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }
}

void Analysis::Raw::writeAsText_callpathTrace(const char* filenm) {
  if (!filenm) {
    return;
  }

  try {
    FILE* fs = hpcio_fopen_r(filenm);
    if (!fs) {
      DIAG_Throw("error opening trace file '" << filenm << "'");
    }

    hpctrace_fmt_hdr_t hdr;

    int ret = hpctrace_fmt_hdr_fread(&hdr, fs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("error reading trace file '" << filenm << "'");
    }

    hpctrace_fmt_hdr_fprint(&hdr, stdout);

    // Read trace records and exit on EOF
    while (!feof(fs)) {
      hpctrace_fmt_datum_t datum;
      ret = hpctrace_fmt_datum_fread(&datum, hdr.flags, fs);
      if (ret == HPCFMT_EOF) {
        break;
      } else if (ret == HPCFMT_ERR) {
        DIAG_Throw("error reading trace file '" << filenm << "'");
      }

      hpctrace_fmt_datum_fprint(&datum, hdr.flags, stdout);
    }

    hpcio_fclose(fs);
  } catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }
}

void Analysis::Raw::writeAsText_flat(const char* filenm) {
  if (!filenm) {
    return;
  }

  Prof::Flat::ProfileData prof;
  try {
    prof.openread(filenm);
  } catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }

  prof.dump(std::cout);
}
