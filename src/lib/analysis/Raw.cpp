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

//************************* System Include Files ****************************

#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
using std::string;

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

//*************************** User Include Files ****************************

#include "Raw.hpp"
#include "Util.hpp"

#include <lib/prof/CallPath-Profile.hpp>

#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/id-tuple.h>
#include <lib/prof-lean/formats/cctdb.h>
#include <lib/prof-lean/formats/metadb.h>
#include <lib/prof-lean/formats/primitive.h>
#include <lib/prof-lean/formats/profiledb.h>
#include <lib/prof-lean/formats/tracedb.h>


#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

//****************************************************************************

void
Analysis::Raw::writeAsText(/*destination,*/ const char* filenm, bool sm_easyToGrep)
{
  using namespace Analysis::Util;

  ProfType_t ty = getProfileType(filenm);
  if (ty == ProfType_Callpath) {
    writeAsText_callpath(filenm, sm_easyToGrep);
  }
  else if (ty == ProfType_CallpathMetricDB) {
    writeAsText_callpathMetricDB(filenm);
  }
  else if (ty == ProfType_CallpathTrace) {
    writeAsText_callpathTrace(filenm);
  }
  else if (ty == ProfType_ProfileDB) {
    writeAsText_profiledb(filenm, sm_easyToGrep);
  }
  else if (ty == ProfType_CctDB){
    writeAsText_cctdb(filenm, sm_easyToGrep);
  }
  else if (ty == ProfType_TraceDB){
    writeAsText_tracedb(filenm);
  }
  else if (ty == ProfType_MetaDB){
    writeAsText_metadb(filenm);
  }
  else {
    DIAG_Die(DIAG_Unimplemented);
  }
}


void
Analysis::Raw::writeAsText_callpath(const char* filenm, bool sm_easyToGrep)
{
  if (!filenm) { return; }

  try {
    Prof::CallPath::Profile::make(filenm, stdout, sm_easyToGrep);
  }
  catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }
}

void
Analysis::Raw::writeAsText_profiledb(const char* filenm, bool easygrep)
{
  if(!filenm) return;

  try {
    FILE* fs = hpcio_fopen_r(filenm);
    if (!fs) {
      DIAG_Throw("error opening profile.db file '" << filenm << "'");
    }

    {
      char buf[16];
      if(fread(buf, 1, sizeof buf, fs) < sizeof buf)
        DIAG_Throw("eof/error reading profile.db format header");
      uint8_t minor;
      auto ver = fmt_profiledb_check(buf, &minor);
      switch(ver) {
      case fmt_version_invalid:
        DIAG_Throw("Not a profile.db file");
      case fmt_version_backward:
        DIAG_Throw("Incompatible profile.db version (too old)");
      case fmt_version_major:
        DIAG_Throw("Incompatible profile.db version (major version mismatch)");
      case fmt_version_forward:
        DIAG_Msg(0, "WARNING: Minor version mismatch (" << (unsigned int)minor
          << " > " << FMT_PROFILEDB_MinorVersion << "), some fields may be missing");
        break;
      default:
        break;
      }
      if(ver < 0)
        DIAG_Throw("error parsing profile.db format header");

      std::cout << "profile.db version " << FMT_DB_MajorVersion << "."
                << (unsigned int)minor << "\n";
    }

    fmt_profiledb_fHdr_t fhdr;
    { // profile.db file header
      rewind(fs);
      char buf[FMT_PROFILEDB_SZ_FHdr];
      if(fread(buf, 1, sizeof buf, fs) < sizeof buf)
        DIAG_Throw("eof reading profile.db file header");
      fmt_profiledb_fHdr_read(&fhdr, buf);
      std::cout << std::hex <<
        "[file header:\n"
        "  (szProfileInfos: 0x" << fhdr.szProfileInfos << ") (pProfileInfos: 0x" << fhdr.pProfileInfos << ")\n"
        "  (szIdTuples: 0x" << fhdr.szIdTuples << ") (pIdTuples: 0x" << fhdr.pIdTuples << ")\n"
        "]\n" << std::dec;
    }

    std::vector<fmt_profiledb_profInfo_t> pis;
    { // Profile Info section
      if(fseeko(fs, fhdr.pProfileInfos, SEEK_SET) < 0)
        DIAG_Throw("error seeking to profile.db Profile Info section");
      std::vector<char> buf(fhdr.szProfileInfos);
      if(fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading profile.db Profile Info section");

      fmt_profiledb_profInfoSHdr_t shdr;
      fmt_profiledb_profInfoSHdr_read(&shdr, buf.data());
      std::cout << std::hex <<
        "[profile info:\n"
        "  (pProfiles: 0x" << shdr.pProfiles << ") (nProfiles: "
          << std::dec << shdr.nProfiles << std::hex << ")\n"
        "  (szProfile: 0x" << (unsigned int)shdr.szProfile << " >= 0x" << FMT_PROFILEDB_SZ_ProfInfo << ")\n";
      pis.reserve(shdr.nProfiles);
      for(uint32_t i = 0; i < shdr.nProfiles; i++) {
        fmt_profiledb_profInfo_t pi;
        fmt_profiledb_profInfo_read(&pi, &buf[shdr.pProfiles + i*shdr.szProfile - fhdr.pProfileInfos]);
        pis.push_back(pi);
        std::cout << "  [pProfiles[" << std::dec << i << std::hex << "]:\n"
          "    [profile-major sparse value block:\n"
          "      (nValues: " << std::dec << pi.valueBlock.nValues << std::hex << ")"
            " (pValues: 0x" << pi.valueBlock.pValues << ")\n"
          "      (nCtxs: " << std::dec << pi.valueBlock.nCtxs << std::hex << ")"
            " (pCtxIndices: 0x" << pi.valueBlock.pCtxIndices << ")\n"
          "    ]\n"
          "    (pIdTuple: 0x" << pi.pIdTuple << ")\n"
          "    (isSummary: " << (pi.isSummary ? 1 : 0) << ")\n"
          "  ]\n";
      }
      std::cout << "]\n" << std::dec;
    }

    { // Hierarchical Identifier Tuple section
      if(fseeko(fs, fhdr.pIdTuples, SEEK_SET) < 0)
        DIAG_Throw("error seeking to profile.db Identifier Tuple section");
      std::vector<char> buf(fhdr.szIdTuples);
      if(fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading profile.db Identifier Tuple section");

      std::cout << "[hierarchical identifier tuples:\n" << std::hex;

      // In actuality, each tuple is only identified by the Profile Info that
      // references it. So loop through the PIs in order by id tuple
      std::sort(pis.begin(), pis.end(), [](const auto& a, const auto& b){
        return a.pIdTuple < b.pIdTuple;
      });
      for(const auto& pi: pis) {
        if(pi.pIdTuple == 0) continue;
        fmt_profiledb_idTupleHdr_t hdr;
        fmt_profiledb_idTupleHdr_read(&hdr, &buf[pi.pIdTuple - fhdr.pIdTuples]);
        std::cout << "  (0x" << pi.pIdTuple << ") [";
        for(uint16_t i = 0; i < hdr.nIds; i++) {
          fmt_profiledb_idTupleElem_t elem;
          fmt_profiledb_idTupleElem_read(&elem, &buf[pi.pIdTuple - fhdr.pIdTuples
              + FMT_PROFILEDB_SZ_IdTupleHdr + i*FMT_PROFILEDB_SZ_IdTupleElem]);
          std::cout << (i > 0 ? " (" : "(");
          switch(elem.kind) {
          case IDTUPLE_SUMMARY: std::cout << "SUMMARY"; break;
          case IDTUPLE_NODE: std::cout << "NODE"; break;
          case IDTUPLE_THREAD: std::cout << "THREAD"; break;
          case IDTUPLE_GPUDEVICE: std::cout << "GPUDEVICE"; break;
          case IDTUPLE_GPUCONTEXT: std::cout << "GPUCONTEXT"; break;
          case IDTUPLE_GPUSTREAM: std::cout << "GPUSTREAM"; break;
          case IDTUPLE_CORE: std::cout << "CORE";
          }
          std::cout << " " << std::dec << elem.logicalId << std::hex;
          if(elem.isPhysical)
            std::cout << " / 0x" << elem.physicalId;
          std::cout << ")";
        }
        std::cout << "]\n";
      }
      std::cout << "]\n" << std::dec;
    }

    // Rest of the file is profile data. Pointers are listed in the PIs, we
    // output in file order.
    std::sort(pis.begin(), pis.end(), [](const auto& a, const auto& b){
      return a.valueBlock.pValues < b.valueBlock.pValues;
    });
    for(const auto& pi: pis) {
      const auto& psvb = pi.valueBlock;

      if(psvb.nValues == 0) {
        // Empty profile, no need to print anything
        continue;
      }

      if(fseeko(fs, psvb.pValues, SEEK_SET) < 0)
        DIAG_Throw("error seeking to profile.db profile data segment");
      std::vector<char> buf(psvb.pCtxIndices + psvb.nCtxs*FMT_PROFILEDB_SZ_CIdx - psvb.pValues);
      if(fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading profile.db profile data segment");

      std::cout << std::hex << "(0x" << psvb.pValues << ") [profile data:\n" << std::dec;
      if(!easygrep) {
        std::cout << "  [metric-value pairs:\n";
        for(uint64_t i = 0; i < psvb.nValues; i++) {
          fmt_profiledb_mVal_t val;
          fmt_profiledb_mVal_read(&val, &buf[i * FMT_PROFILEDB_SZ_MVal]);
          std::cout << "    [" << i << "] (metric id: " << val.metricId << ", value: " << val.value << ")\n";
        }
        std::cout << "  ]\n  [context-index pairs:\n";
        for(uint32_t i = 0; i < psvb.nCtxs; i++) {
          fmt_profiledb_cIdx_t idx;
          fmt_profiledb_cIdx_read(&idx, &buf[psvb.pCtxIndices - psvb.pValues + i * FMT_PROFILEDB_SZ_CIdx]);
          std::cout << "    (ctx id: " << idx.ctxId << ", index: " << idx.startIndex << ")\n";
        }
        std::cout << "  ]\n";
      } else if(psvb.nCtxs > 0) {
        fmt_profiledb_cIdx_t idx;
        fmt_profiledb_cIdx_read(&idx, &buf[psvb.pCtxIndices - psvb.pValues]);
        for(uint32_t i = 1; i <= psvb.nCtxs; i++) {
          std::cout << "  (ctx id: " << idx.ctxId << ")";

          uint64_t startIndex = idx.startIndex;
          uint64_t endIndex = psvb.nValues;
          if(i < psvb.nCtxs) {
            fmt_profiledb_cIdx_read(&idx, &buf[psvb.pCtxIndices - psvb.pValues + i * FMT_PROFILEDB_SZ_CIdx]);
            endIndex = idx.startIndex;
          }

          for(uint64_t j = startIndex; j < endIndex; j++) {
            fmt_profiledb_mVal_t val;
            fmt_profiledb_mVal_read(&val, &buf[j * FMT_PROFILEDB_SZ_MVal]);
            std::cout << " (metric id: " << val.metricId << ", value: " << val.value << ")";
          }
          std::cout << "\n";
        }
      }
      std::cout << "]\n" << std::dec;
    }

    { // File footer
      char buf[sizeof fmt_profiledb_footer + 1];
      if(fseeko(fs, -sizeof fmt_profiledb_footer, SEEK_END) < 0)
        DIAG_Throw("error seeking to profile.db footer");
      if(fread(buf, 1, sizeof fmt_profiledb_footer, fs) < sizeof fmt_profiledb_footer)
        DIAG_Throw("error reading profile.db footer");
      buf[sizeof fmt_profiledb_footer] = '\0';
      std::cout << "[footer: '" << buf << "'";
      if(memcmp(buf, fmt_profiledb_footer, sizeof fmt_profiledb_footer) == 0)
        std::cout << ", OK]\n";
      else
        std::cout << ", INVALID]\n";
    }

    hpcio_fclose(fs);
  }
  catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }
}

void
Analysis::Raw::writeAsText_cctdb(const char* filenm, bool easygrep)
{
  if (!filenm) return;

  try {
    FILE* fs = hpcio_fopen_r(filenm);
    if (!fs) {
      DIAG_Throw("error opening cct.db file '" << filenm << "'");
    }

    {
      char buf[16];
      if(fread(buf, 1, sizeof buf, fs) < sizeof buf)
        DIAG_Throw("eof/error reading cct.db format header");
      uint8_t minor;
      auto ver = fmt_cctdb_check(buf, &minor);
      switch(ver) {
      case fmt_version_invalid:
        DIAG_Throw("Not a cct.db file");
      case fmt_version_backward:
        DIAG_Throw("Incompatible cct.db version (too old)");
      case fmt_version_major:
        DIAG_Throw("Incompatible cct.db version (major version mismatch)");
      case fmt_version_forward:
        DIAG_Msg(0, "WARNING: Minor version mismatch (" << (unsigned int)minor
          << " > " << FMT_CCTDB_MinorVersion << "), some fields may be missing");
        break;
      default:
        break;
      }
      if(ver < 0)
        DIAG_Throw("error parsing cct.db format header");

      std::cout << "cct.db version " << FMT_DB_MajorVersion << "."
                << (unsigned int)minor << "\n";
    }

    fmt_cctdb_fHdr_t fhdr;
    { // cct.db file header
      rewind(fs);
      char buf[FMT_CCTDB_SZ_FHdr];
      if(fread(buf, 1, sizeof buf, fs) < sizeof buf)
        DIAG_Throw("eof reading cct.db file header");
      fmt_cctdb_fHdr_read(&fhdr, buf);
      std::cout << std::hex <<
        "[file header:\n"
        "  (szCtxInfo: 0x" << fhdr.szCtxInfo << ") (pCtxInfo: 0x" << fhdr.pCtxInfo << ")\n"
        "]\n" << std::dec;
    }

    std::vector<fmt_cctdb_ctxInfo_t> cis;
    { // Context Info section
      if(fseeko(fs, fhdr.pCtxInfo, SEEK_SET) < 0)
        DIAG_Throw("error seeking to cct.db Context Info section");
      std::vector<char> buf(fhdr.szCtxInfo);
      if(fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading cct.db Context Info section");

      fmt_cctdb_ctxInfoSHdr_t shdr;
      fmt_cctdb_ctxInfoSHdr_read(&shdr, buf.data());
      std::cout << std::hex <<
        "[context info:\n"
        "  (pCtxs: 0x" << shdr.pCtxs << ") (nCtxs: "
          << std::dec << shdr.nCtxs << std::hex << ")\n"
        "  (szCtx: 0x" << (unsigned int)shdr.szCtx << " >= 0x" << FMT_CCTDB_SZ_CtxInfo << ")\n";
      cis.reserve(shdr.nCtxs);
      for(uint32_t i = 0; i < shdr.nCtxs; i++) {
        fmt_cctdb_ctxInfo_t ci;
        fmt_cctdb_ctxInfo_read(&ci, &buf[shdr.pCtxs + i*shdr.szCtx - fhdr.pCtxInfo]);
        cis.push_back(ci);
        std::cout << "  [pCtxs[" << std::dec << i << std::hex << "]:\n"
          "    [context-major sparse value block:\n"
          "      (nValues: " << std::dec << ci.valueBlock.nValues << std::hex << ")"
            " (pValues: 0x" << ci.valueBlock.pValues << ")\n"
          "      (nMetrics: " << std::dec << ci.valueBlock.nMetrics << std::hex << ")"
            " (pMetricIndices: 0x" << ci.valueBlock.pMetricIndices << ")\n"
          "    ]\n"
          "  ]\n";
      }
      std::cout << "]\n" << std::dec;
    }

    // Rest of the file is context metric data. Pointers are listed in the CIs,
    // we output in file order.
    std::sort(cis.begin(), cis.end(), [](const auto& a, const auto& b){
      return a.valueBlock.pValues < b.valueBlock.pValues;
    });
    for(const auto& ci: cis) {
      const auto& csvb = ci.valueBlock;

      if(csvb.nValues == 0) {
        // Empty context, no need to print anything
        continue;
      }

      if(fseeko(fs, csvb.pValues, SEEK_SET) < 0)
        DIAG_Throw("error seeking to cct.db context data segment");
      std::vector<char> buf(csvb.pMetricIndices + csvb.nMetrics*FMT_CCTDB_SZ_MIdx - csvb.pValues);
      if(fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading cct.db context data segment");

      std::cout << std::hex << "(0x" << csvb.pValues << ") [context data:\n" << std::dec;
      if(!easygrep) {
        std::cout << "  [profile-value pairs:\n";
        for(uint64_t i = 0; i < csvb.nValues; i++) {
          fmt_cctdb_pVal_t val;
          fmt_cctdb_pVal_read(&val, &buf[i * FMT_CCTDB_SZ_PVal]);
          std::cout << "    [" << i << "] (profile index: " << val.profIndex << ", value: " << val.value << ")\n";
        }
        std::cout << "  ]\n  [metric-index pairs:\n";
        for(uint16_t i = 0; i < csvb.nMetrics; i++) {
          fmt_cctdb_mIdx_t idx;
          fmt_cctdb_mIdx_read(&idx, &buf[csvb.pMetricIndices - csvb.pValues + i * FMT_CCTDB_SZ_MIdx]);
          std::cout << "    (metric id: " << idx.metricId << ", index: " << idx.startIndex << ")\n";
        }
        std::cout << "  ]\n";
      } else if(csvb.nMetrics > 0) {
        fmt_cctdb_mIdx_t idx;
        fmt_cctdb_mIdx_read(&idx, &buf[csvb.pMetricIndices - csvb.pValues]);
        for(uint16_t i = 1; i <= csvb.nMetrics; i++) {
          std::cout << "  (metric id: " << idx.metricId << ")";

          uint64_t startIndex = idx.startIndex;
          uint64_t endIndex = csvb.nValues;
          if(i < csvb.nMetrics) {
            fmt_cctdb_mIdx_read(&idx, &buf[csvb.pMetricIndices - csvb.pValues + i * FMT_CCTDB_SZ_MIdx]);
            endIndex = idx.startIndex;
          }

          for(uint64_t j = startIndex; j < endIndex; j++) {
            fmt_cctdb_pVal_t val;
            fmt_cctdb_pVal_read(&val, &buf[j * FMT_CCTDB_SZ_PVal]);
            std::cout << " (profile index: " << val.profIndex << ", value: " << val.value << ")";
          }
          std::cout << "\n";
        }
      }
      std::cout << "]\n" << std::dec;
    }

    { // File footer
      char buf[sizeof fmt_cctdb_footer + 1];
      if(fseeko(fs, -sizeof fmt_cctdb_footer, SEEK_END) < 0)
        DIAG_Throw("error seeking to cct.db footer");
      if(fread(buf, 1, sizeof fmt_cctdb_footer, fs) < sizeof fmt_cctdb_footer)
        DIAG_Throw("error reading cct.db footer");
      buf[sizeof fmt_cctdb_footer] = '\0';
      std::cout << "[footer: '" << buf << "'";
      if(memcmp(buf, fmt_cctdb_footer, sizeof fmt_cctdb_footer) == 0)
        std::cout << ", OK]\n";
      else
        std::cout << ", INVALID]\n";
    }

    hpcio_fclose(fs);
  }
  catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }
}

void
Analysis::Raw::writeAsText_tracedb(const char* filenm)
{
  if (!filenm) return;

  try {
    FILE* fs = hpcio_fopen_r(filenm);
    if (!fs) {
      DIAG_Throw("error opening trace.db file '" << filenm << "'");
    }

    {
      char buf[16];
      if(fread(buf, 1, sizeof buf, fs) < sizeof buf)
        DIAG_Throw("eof/error reading trace.db format header");
      uint8_t minor;
      auto ver = fmt_tracedb_check(buf, &minor);
      switch(ver) {
      case fmt_version_invalid:
        DIAG_Throw("Not a trace.db file");
      case fmt_version_backward:
        DIAG_Throw("Incompatible trace.db version (too old)");
      case fmt_version_major:
        DIAG_Throw("Incompatible trace.db version (major version mismatch)");
      case fmt_version_forward:
        DIAG_Msg(0, "WARNING: Minor version mismatch (" << (unsigned int)minor
          << " > " << FMT_TRACEDB_MinorVersion << "), some fields may be missing");
        break;
      default:
        break;
      }
      if(ver < 0)
        DIAG_Throw("error parsing trace.db format header");

      std::cout << "trace.db version " << FMT_DB_MajorVersion << "."
                << (unsigned int)minor << "\n";
    }

    fmt_tracedb_fHdr_t fhdr;
    { // trace.db file header
      rewind(fs);
      char buf[FMT_CCTDB_SZ_FHdr];
      if(fread(buf, 1, sizeof buf, fs) < sizeof buf)
        DIAG_Throw("eof reading cct.db file header");
      fmt_tracedb_fHdr_read(&fhdr, buf);
      std::cout << std::hex <<
        "[file header:\n"
        "  (szCtxTraces: 0x" << fhdr.szCtxTraces << ") (pCtxTraces: 0x" << fhdr.pCtxTraces << ")\n"
        "]\n" << std::dec;
    }

    std::vector<fmt_tracedb_ctxTrace_t> ctxTraces;
    { // Context Trace Headers section
      if(fseeko(fs, fhdr.pCtxTraces, SEEK_SET) < 0)
        DIAG_Throw("error seeking to trace.db Context Trace Headers section");
      std::vector<char> buf(fhdr.szCtxTraces);
      if(fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading trace.db Context Trace Headers section");

      fmt_tracedb_ctxTraceSHdr_t shdr;
      fmt_tracedb_ctxTraceSHdr_read(&shdr, buf.data());
      std::cout << std::hex <<
        "[context trace headers:\n"
        "  (pTraces: 0x" << shdr.pTraces << ") (nTraces: " << std::dec << shdr.nTraces << std::hex << ")\n"
        "  (szTrace: 0x" << (unsigned int)shdr.szTrace << " >= 0x" << FMT_TRACEDB_SZ_CtxTrace << ")\n"
        "  (minTimestamp: " << std::dec << shdr.minTimestamp << ")\n"
        "  (maxTimestamp: " << shdr.maxTimestamp << ")\n";
      for(uint32_t i = 0; i < shdr.nTraces; i++) {
        fmt_tracedb_ctxTrace_t ct;
        fmt_tracedb_ctxTrace_read(&ct, &buf[shdr.pTraces + i * shdr.szTrace - fhdr.pCtxTraces]);
        ctxTraces.push_back(ct);
        std::cout << "  [pTraces[" << std::dec << i << "]:\n" <<
          "    (profIndex: " << ct.profIndex << ")\n" << std::hex <<
          "    (pStart: 0x" << ct.pStart << ") (pEnd: 0x" << ct.pEnd << ")\n"
          "  ]\n";
      }
      std::cout << "]\n" << std::dec;
    }

    // Rest of the file is context traces. Output is in file order.
    std::sort(ctxTraces.begin(), ctxTraces.end(), [](const auto& a, const auto& b){
      return a.pStart < b.pStart;
    });
    for(const auto& ct: ctxTraces) {
      if(fseeko(fs, ct.pStart, SEEK_SET) < 0)
        DIAG_Throw("error seeking to trace.db context trace data segment");
      std::vector<char> buf(ct.pEnd - ct.pStart);
      if(fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading trace.db context trace data segment");

      std::cout << std::hex << "(0x" << ct.pStart << ") [context trace:\n" << std::dec;
      for(char* cur = buf.data(), *end = cur + buf.size(); cur < end;
          cur += FMT_TRACEDB_SZ_CtxSample) {
        fmt_tracedb_ctxSample_t elem;
        fmt_tracedb_ctxSample_read(&elem, cur);
        std::cout << "  (timestamp: " << elem.timestamp << ", ctxId: " << elem.ctxId << ")\n";
      }
      std::cout << "]\n";
    }

    { // File footer
      char buf[sizeof fmt_tracedb_footer + 1];
      if(fseeko(fs, -sizeof fmt_tracedb_footer, SEEK_END) < 0)
        DIAG_Throw("error seeking to trace.db footer");
      if(fread(buf, 1, sizeof fmt_tracedb_footer, fs) < sizeof fmt_tracedb_footer)
        DIAG_Throw("error reading trace.db footer");
      buf[sizeof fmt_tracedb_footer] = '\0';
      std::cout << "[footer: '" << buf << "'";
      if(memcmp(buf, fmt_tracedb_footer, sizeof fmt_tracedb_footer) == 0)
        std::cout << ", OK]\n";
      else
        std::cout << ", INVALID]\n";
    }

    hpcio_fclose(fs);
  }
  catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }
}


void
Analysis::Raw::writeAsText_metadb(const char* filenm)
{
  if(!filenm) return;

  try {
    FILE* fs = hpcio_fopen_r(filenm);
    if (!fs) {
      DIAG_Throw("error opening meta.db file '" << filenm << "'");
    }

    {
      char buf[16];
      if(fread(buf, 1, sizeof buf, fs) < sizeof buf)
        DIAG_Throw("eof/error reading meta.db format header");
      uint8_t minor;
      auto ver = fmt_metadb_check(buf, &minor);
      switch(ver) {
      case fmt_version_invalid:
        DIAG_Throw("Not a meta.db file");
      case fmt_version_backward:
        DIAG_Throw("Incompatible meta.db version (too old)");
      case fmt_version_major:
        DIAG_Throw("Incompatible meta.db version (major version mismatch)");
      case fmt_version_forward:
        DIAG_Msg(0, "WARNING: Minor version mismatch (" << (unsigned int)minor
          << " > " << FMT_METADB_MinorVersion << "), some fields may be missing");
        break;
      default:
        break;
      }
      if(ver < 0)
        DIAG_Throw("error parsing meta.db format header");

      std::cout << "meta.db version " << FMT_DB_MajorVersion << "."
                << (unsigned int)minor << "\n";
    }

    fmt_metadb_fHdr_t fhdr;
    { // meta.db file header
      rewind(fs);
      char buf[FMT_METADB_SZ_FHdr];
      if(fread(buf, 1, sizeof buf, fs) < sizeof buf)
        DIAG_Throw("eof reading meta.db file header");
      fmt_metadb_fHdr_read(&fhdr, buf);
      std::cout << std::hex <<
        "[file header:\n"
        "  (szGeneral: 0x" << fhdr.szGeneral << ") (pGeneral: 0x" << fhdr.pGeneral <<  ")\n"
        "  (szIdNames: 0x" << fhdr.szIdNames << ") (pIdNames: 0x" << fhdr.pIdNames <<  ")\n"
        "  (szMetrics: 0x" << fhdr.szMetrics << ") (pMetrics: 0x" << fhdr.pMetrics <<  ")\n"
        "  (szContext: 0x" << fhdr.szContext << ") (pContext: 0x" << fhdr.pContext <<  ")\n"
        "  (szStrings: 0x" << fhdr.szStrings << ") (pStrings: 0x" << fhdr.pStrings <<  ")\n"
        "  (szModules: 0x" << fhdr.szModules << ") (pModules: 0x" << fhdr.pModules <<  ")\n"
        "  (szFiles: 0x" << fhdr.szFiles << ") (pFiles: 0x" << fhdr.pFiles <<  ")\n"
        "  (szFunctions: 0x" << fhdr.szFunctions << ") (pFunctions: 0x" << fhdr.pFunctions <<  ")\n"
        "]\n" << std::dec;
    }

    { // General Properties section
      if(fseeko(fs, fhdr.pGeneral, SEEK_SET) < 0)
        DIAG_Throw("error seeking to meta.db General Properties section");
      std::vector<char> buf(fhdr.szGeneral);
      if(fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading meta.db General Properties section");

      fmt_metadb_generalSHdr_t gphdr;
      fmt_metadb_generalSHdr_read(&gphdr, buf.data());
      std::cout << std::hex <<
        "[general properties:\n"
        "  (pTitle: 0x" << gphdr.pTitle << " = &\""
          << &buf.at(gphdr.pTitle - fhdr.pGeneral) << "\")\n"
        "  (pDescription: 0x" << gphdr.pDescription << " = &\"\"\"\n"
          << &buf.at(gphdr.pDescription - fhdr.pGeneral) << "\n"
        "\"\"\")\n"
        "]\n" << std::dec;
    }

    { // Identifier Names section
      if(fseeko(fs, fhdr.pIdNames, SEEK_SET) < 0)
        DIAG_Throw("error seeking to meta.db Identifier Names section");
      std::vector<char> buf(fhdr.szIdNames);
      if(fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading meta.db Identifier Names section");

      fmt_metadb_idNamesSHdr_t idhdr;
      fmt_metadb_idNamesSHdr_read(&idhdr, buf.data());
      std::cout << std::hex <<
        "[hierarchical identifier names:\n"
        "  (ppNames: 0x" << idhdr.ppNames << ") (nKinds: "
            << std::dec << (unsigned int)idhdr.nKinds << ")\n" << std::hex;
      for(unsigned int i = 0; i < idhdr.nKinds; i++) {
        auto pName = fmt_u64_read(&buf.at(idhdr.ppNames + 8*i - fhdr.pIdNames));
        std::cout << "    (ppNames[" << i << "]: 0x" << pName << " = &\""
                  << &buf.at(pName - fhdr.pIdNames) << "\")\n";
      }
      std::cout << "]\n" << std::dec;
    }

    { // Performance Metrics section
      if(fseeko(fs, fhdr.pMetrics, SEEK_SET) < 0)
        DIAG_Throw("error seeking to meta.db Performance Metrics section");
      std::vector<char> buf(fhdr.szMetrics);
      if(fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading meta.db Performance Metrics section");

      fmt_metadb_metricsSHdr_t mhdr;
      fmt_metadb_metricsSHdr_read(&mhdr, buf.data());
      std::cout << std::hex <<
        "[performance metrics:\n"
        "  (pMetrics: 0x" << mhdr.pMetrics << ") (nMetrics: "
          << std::dec << mhdr.nMetrics << std::hex << ")\n"
        "  (szMetric: 0x" << (unsigned int)mhdr.szMetric << " >= 0x" << FMT_METADB_SZ_MetricDesc << ")\n"
        "  (szScopeInst: 0x" << (unsigned int)mhdr.szScopeInst << " >= 0x" << FMT_METADB_SZ_PropScopeInst << ")\n"
        "  (szSummary: 0x" << (unsigned int)mhdr.szSummary << " >= 0x" << FMT_METADB_SZ_SummaryStat << ")\n"
        "  (pScopes: 0x" << mhdr.pScopes << ") (nScopes: "
          << std::dec << (unsigned int)mhdr.nScopes << std::hex << ")\n"
        "  (szScope: 0x" << (unsigned int)mhdr.szScope << " >= 0x" << FMT_METADB_SZ_PropScope << ")\n";
      for(unsigned long i = 0; i < mhdr.nMetrics; i++) {
        fmt_metadb_metricDesc_t mdesc;
        fmt_metadb_metricDesc_read(&mdesc, &buf.at(mhdr.pMetrics + mhdr.szMetric*i - fhdr.pMetrics));
        std::cout <<
          "  [pMetrics[" << std::dec << i << std::hex << "]:\n"
          "    (pName: 0x" << mdesc.pName << " = &\""
              << &buf.at(mdesc.pName - fhdr.pMetrics) << "\")\n"
          "    (pScopeInsts: 0x" << mdesc.pScopeInsts << ")"
             " (nScopeInsts: " << std::dec << mdesc.nScopeInsts << std::hex << ")\n"
          "    (pSummaries: 0x" << mdesc.pSummaries << ")"
             " (nSummaries: " << std::dec << mdesc.nSummaries << std::hex << ")\n";
        for(unsigned int j = 0; j < mdesc.nScopeInsts; j++) {
          fmt_metadb_propScopeInst_t inst;
          fmt_metadb_propScopeInst_read(&inst, &buf.at(mdesc.pScopeInsts + mhdr.szScopeInst*j - fhdr.pMetrics));
          fmt_metadb_propScope_t base;
          fmt_metadb_propScope_read(&base, &buf.at(inst.pScope - fhdr.pMetrics));
          std::cout <<
            "    [pScopeInsts[" << std::dec << j << std::hex << "]:\n"
            "      (pScope: 0x" << inst.pScope << " [pScopeName: 0x"
              << base.pScopeName << " = &\""
              << &buf.at(base.pScopeName - fhdr.pMetrics) << "\"])\n"
            "      (propMetricId: " << std::dec << inst.propMetricId << std::hex << ")\n"
            "    ]\n";
        }
        for(unsigned int j = 0; j < mdesc.nSummaries; j++) {
          fmt_metadb_summaryStat_t summary;
          fmt_metadb_summaryStat_read(&summary, &buf.at(mdesc.pSummaries + mhdr.szSummary*j - fhdr.pMetrics));
          fmt_metadb_propScope_t base;
          fmt_metadb_propScope_read(&base, &buf.at(summary.pScope - fhdr.pMetrics));
          std::cout <<
            "    [pSummaries[" << std::dec << j << std::hex << "]:\n"
            "      (pScope: 0x" << summary.pScope << " [pScopeName: 0x"
              << base.pScopeName << " = &\""
              << &buf.at(base.pScopeName - fhdr.pMetrics) << "\"])\n"
            "      (pFormula: 0x" << summary.pFormula << " = &\""
              << &buf.at(summary.pFormula - fhdr.pMetrics) << "\")\n"
            "      (combine: " << std::dec << (unsigned int)summary.combine << " = ";
          switch(summary.combine) {
          case FMT_METADB_COMBINE_Sum: std::cout << "sum"; break;
          case FMT_METADB_COMBINE_Min: std::cout << "min"; break;
          case FMT_METADB_COMBINE_Max: std::cout << "max"; break;
          default: std::cout << "???";
          }
          std::cout << ")\n"
            "      (statMetricId: " << std::dec << summary.statMetricId << ")\n"
            "    ]\n";
        }
        std::cout << "  ]\n";
      }
      for(unsigned int i = 0; i < mhdr.nScopes; i++) {
        fmt_metadb_propScope_t scope;
        fmt_metadb_propScope_read(&scope, &buf.at(mhdr.pScopes + mhdr.szScope*i - fhdr.pMetrics));
        std::cout <<
          "  [pScopes[" << std::dec << i << std::hex << "]:\n"
          "    (pScopeName: 0x" << scope.pScopeName << " = &\""
            << &buf.at(scope.pScopeName - fhdr.pMetrics) << "\")\n"
          "    (type: " << std::dec << (unsigned int)scope.type << " = ";
          switch(scope.type) {
          case FMT_METADB_SCOPETYPE_Custom: std::cout << "custom"; break;
          case FMT_METADB_SCOPETYPE_Point: std::cout << "point"; break;
          case FMT_METADB_SCOPETYPE_Execution: std::cout << "execution"; break;
          case FMT_METADB_SCOPETYPE_Transitive: std::cout << "transitive"; break;
          default: std::cout << "???";
          }
          std::cout << ")\n";
          if(scope.type == FMT_METADB_SCOPETYPE_Transitive) {
            std::cout << "    (propagationIndex: " << (unsigned int)scope.propagationIndex << ")\n";
          }
          std::cout << "  ]\n";
      }
      std::cout << "]\n" << std::dec;
    }

    std::vector<char> strings;
    { // Common String Table section
      if(fseeko(fs, fhdr.pStrings, SEEK_SET) < 0)
        DIAG_Throw("error seeking to meta.db Common String Table section");
      strings = std::vector<char>(fhdr.szStrings);
      if(fread(strings.data(), 1, strings.size(), fs) < strings.size())
        DIAG_Throw("eof reading meta.db Common String Table section");
    }

    { // Load Modules section
      if(fseeko(fs, fhdr.pModules, SEEK_SET) < 0)
        DIAG_Throw("error seeking to meta.db Load Modules section");
      std::vector<char> buf(fhdr.szModules);
      if(fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading meta.db Load Modules section");

      fmt_metadb_modulesSHdr_t shdr;
      fmt_metadb_modulesSHdr_read(&shdr, buf.data());
      std::cout << std::hex <<
        "[load modules:\n"
        "  (pModules: 0x" << shdr.pModules << ") (nModules: "
          << std::dec << shdr.nModules << std::hex << ")\n"
        "  (szModule: 0x" << shdr.szModule << " >= 0x" << FMT_METADB_SZ_ModuleSpec << ")\n";
      for(unsigned long i = 0; i < shdr.nModules; i++) {
        fmt_metadb_moduleSpec_t mspec;
        auto off = shdr.pModules + shdr.szModule*i;
        fmt_metadb_moduleSpec_read(&mspec, &buf.at(off - fhdr.pModules));
        std::cout <<
          "  [pModules[" << std::dec << i << std::hex << "]: (0x" << off << ")\n"
          "    (pPath: 0x" << mspec.pPath << " = &\""
            << &strings.at(mspec.pPath - fhdr.pStrings) << "\")\n"
          "  ]\n";
      }
      std::cout << "]\n" << std::dec;
    }

    { // Source Files section
      if(fseeko(fs, fhdr.pFiles, SEEK_SET) < 0)
        DIAG_Throw("error seeking to meta.db Source Files section");
      std::vector<char> buf(fhdr.szFiles);
      if(fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading meta.db Source Files section");

      fmt_metadb_filesSHdr_t shdr;
      fmt_metadb_filesSHdr_read(&shdr, buf.data());
      std::cout << std::hex <<
        "[source files:\n"
        "  (pFiles: 0x" << shdr.pFiles << ") (nFiles: "
          << std::dec << shdr.nFiles << std::hex << ")\n"
        "  (szFile: 0x" << shdr.szFile << " >= 0x" << FMT_METADB_SZ_FileSpec << ")\n";
      for(unsigned long i = 0; i < shdr.nFiles; i++) {
        fmt_metadb_fileSpec_t fspec;
        auto off = shdr.pFiles + shdr.szFile*i;
        fmt_metadb_fileSpec_read(&fspec, &buf.at(off - fhdr.pFiles));
        std::cout <<
          "  [pFiles[" << std::dec << i << std::hex << "]: (0x" << off << ")\n"
          "    (pPath: 0x" << fspec.pPath << " = &\""
            << &strings.at(fspec.pPath - fhdr.pStrings) << "\")\n"
          "  ]\n";
      }
      std::cout << "]\n" << std::dec;
    }

    { // Functions section
      if(fseeko(fs, fhdr.pFunctions, SEEK_SET) < 0)
        DIAG_Throw("error seeking to meta.db Functions section");
      std::vector<char> buf(fhdr.szFunctions);
      if(fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading meta.db Functions section");

      fmt_metadb_functionsSHdr_t shdr;
      fmt_metadb_functionsSHdr_read(&shdr, buf.data());
      std::cout << std::hex <<
        "[functions:\n"
        "  (pFunctions: 0x" << shdr.pFunctions << ") (nFunctions: "
          << std::dec << shdr.nFunctions << std::hex << ")\n"
        "  (szFunction: 0x" << shdr.szFunction << " >= 0x" << FMT_METADB_SZ_FunctionSpec << ")\n";
      for(unsigned long i = 0; i < shdr.nFunctions; i++) {
        fmt_metadb_functionSpec_t fspec;
        auto off = shdr.pFunctions + shdr.szFunction*i;
        fmt_metadb_functionSpec_read(&fspec, &buf.at(off - fhdr.pFunctions));

        std::cout <<
          "  [pFunctions[" << std::dec << i << std::hex << "]: (0x" << off << ")\n"
          "    (pName: 0x" << fspec.pName << " = &\""
            << &strings.at(fspec.pName - fhdr.pStrings) << "\")\n"
          "    (pModule: 0x" << fspec.pModule << ") (offset: 0x" << fspec.offset << ")\n"
          "    (pFile: 0x" << fspec.pFile << ") (line: " << std::dec << fspec.line << std::hex << ")\n"
          "  ]\n";
      }
      std::cout << "]\n" << std::dec;
    }

    { // Context Tree section
      if(fseeko(fs, fhdr.pContext, SEEK_SET) < 0)
        DIAG_Throw("error seeking to meta.db Context Tree section");
      std::vector<char> buf(fhdr.szContext);
      if(fread(buf.data(), 1, buf.size(), fs) < buf.size())
        DIAG_Throw("eof reading meta.db Context Tree section");

      const auto indent = [](unsigned int level) -> std::ostream& {
        for(unsigned int l = 0; l < level; l++) std::cout << "  ";
        return std::cout;
      };
      const std::function<void(unsigned int, uint64_t, uint64_t)> print
          = [&](unsigned int l, uint64_t pCtxs, uint64_t szCtxs){
        while(szCtxs > 0) {
          if(szCtxs < FMT_METADB_MINSZ_Context) {
            indent(l) << "ERROR: not enough bytes for next context, terminating iteration\n";
            return;
          }
          fmt_metadb_context_t ctx;
          if(!fmt_metadb_context_read(&ctx, &buf.at(pCtxs - fhdr.pContext))) {
            indent(l) << "ERROR: invalid next context, terminating iteration\n";
            return;
          }
          indent(l) << std::dec <<
            "[context: (relation: " << (
              ctx.relation == FMT_METADB_RELATION_LexicalNest ? "lexical nest" :
              ctx.relation == FMT_METADB_RELATION_Call ? "call" :
              ctx.relation == FMT_METADB_RELATION_InlinedCall ? "inlined call" :
              "???") << ") (propagation: 0x" << std::hex << ctx.propagation
              << std::dec << ") (ctxId: " << ctx.ctxId << ")"
              " (0x" << std::hex << pCtxs << ")\n";
          indent(l) << std::hex <<
            "  (szChildren: 0x" << ctx.szChildren << ") (pChildren: 0x" << ctx.pChildren << ")\n";
          indent(l) << std::hex <<
            "  (lexicalType: " << (
              ctx.lexicalType == FMT_METADB_LEXTYPE_Function ? "function" :
              ctx.lexicalType == FMT_METADB_LEXTYPE_Line ? "line" :
              ctx.lexicalType == FMT_METADB_LEXTYPE_Loop ? "loop" :
              ctx.lexicalType == FMT_METADB_LEXTYPE_Instruction ? "instruction" :
              "???") << ")\n";
          if(ctx.pFunction != 0) {
            indent(l) << std::hex <<
              "  (pFunction: 0x" << ctx.pFunction << ")\n";
          }
          if(ctx.pFile != 0) {
            indent(l) << std::hex <<
              "  (pFile: 0x" << ctx.pFile << ") (line: " << std::dec << ctx.line << ")\n";
          }
          if(ctx.pModule != 0) {
            indent(l) << std::hex <<
              "  (pModule: 0x" << ctx.pModule << ") (offset: 0x" << ctx.offset << ")\n";
          }
          print(l+1, ctx.pChildren, ctx.szChildren);
          indent(l) << "]\n";
          if(szCtxs < (uint64_t)FMT_METADB_SZ_Context(ctx.nFlexWords)) {
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
      std::cout << std::hex <<
        "[context tree:\n"
        "  (pEntryPoints: 0x" << shdr.pEntryPoints << ") "
          "(nEntryPoints: " << std::dec << shdr.nEntryPoints << ")\n"
        "  (szEntryPoint: 0x" << (unsigned int)shdr.szEntryPoint << " >= 0x" << FMT_METADB_SZ_EntryPoint << ")\n";
      for(unsigned int i = 0; i < shdr.nEntryPoints; i++) {
        fmt_metadb_entryPoint_t ep;
        fmt_metadb_entryPoint_read(&ep, &buf.at(shdr.pEntryPoints + i*shdr.szEntryPoint - fhdr.pContext));
        std::cout << "[entry point: (entryPoint: ";
        switch(ep.entryPoint) {
        case FMT_METADB_ENTRYPOINT_UNKNOWN_ENTRY: std::cout << "unknown entry"; break;
        case FMT_METADB_ENTRYPOINT_MAIN_THREAD: std::cout << "main thread"; break;
        case FMT_METADB_ENTRYPOINT_APPLICATION_THREAD: std::cout << "application thread"; break;
        default: std::cout << "???"; break;
        }
        std::cout << ") (pPrettyName: 0x" << (unsigned int)ep.pPrettyName << " = &\""
            << &strings.at(ep.pPrettyName - fhdr.pStrings) << "\")\n"
          "  (ctxId: " << ep.ctxId << ")\n";
        print(1, ep.pChildren, ep.szChildren);
        std::cout << "]\n";
      }
      std::cout << "]\n";
    }

    { // File footer
      char buf[sizeof fmt_metadb_footer + 1];
      if(fseeko(fs, -sizeof fmt_metadb_footer, SEEK_END) < 0)
        DIAG_Throw("error seeking to meta.db footer");
      if(fread(buf, 1, sizeof fmt_metadb_footer, fs) < sizeof fmt_metadb_footer)
        DIAG_Throw("error reading meta.db footer");
      buf[sizeof fmt_metadb_footer] = '\0';
      std::cout << "[footer: '" << buf << "'";
      if(memcmp(buf, fmt_metadb_footer, sizeof fmt_metadb_footer) == 0)
        std::cout << ", OK]\n";
      else
        std::cout << ", INVALID]\n";
    }

    hpcio_fclose(fs);
  }
  catch(...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }
}

void
Analysis::Raw::writeAsText_callpathMetricDB(const char* filenm)
{
  if (!filenm) { return; }

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

    for (unsigned int nodeId = 1; nodeId < hdr.numNodes + 1; ++nodeId) {
      fprintf(stdout, "(%6u: ", nodeId);
      for (unsigned int mId = 0; mId < hdr.numMetrics; ++mId) {
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
  }
  catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }
}


void
Analysis::Raw::writeAsText_callpathTrace(const char* filenm)
{
  if (!filenm) { return; }

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
    while ( !feof(fs) ) {
      hpctrace_fmt_datum_t datum;
      ret = hpctrace_fmt_datum_fread(&datum, hdr.flags, fs);
      if (ret == HPCFMT_EOF) {
	break;
      }
      else if (ret == HPCFMT_ERR) {
	DIAG_Throw("error reading trace file '" << filenm << "'");
      }

      hpctrace_fmt_datum_fprint(&datum, hdr.flags, stdout);
    }

    hpcio_fclose(fs);
  }
  catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }
}
