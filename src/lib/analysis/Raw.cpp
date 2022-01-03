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
#include <lib/prof/Flat-ProfileData.hpp>


#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>
#include <lib/prof-lean/id-tuple.h>
#include <lib/prof-lean/tracedb.h>
#include <lib/prof-lean/formats/metadb.h>
#include <lib/prof-lean/formats/primitive.h>
#include <lib/prof/pms-format.h>
#include <lib/prof/cms-format.h>


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
  else if (ty == ProfType_Flat) {
    writeAsText_flat(filenm);
  }
  else if (ty == ProfType_SparseDBtmp) { //YUMENG
    writeAsText_sparseDBtmp(filenm, sm_easyToGrep);
  }
  else if (ty == ProfType_SparseDBthread){ //YUMENG
    writeAsText_sparseDBthread(filenm, sm_easyToGrep);
  }
  else if (ty == ProfType_SparseDBcct){ //YUMENG
    writeAsText_sparseDBcct(filenm, sm_easyToGrep);
  }
  else if (ty == ProfType_TraceDB){ //YUMENG
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
  Prof::CallPath::Profile* prof = NULL;
  try {
    prof = Prof::CallPath::Profile::make(filenm, 0/*rFlags*/, stdout, sm_easyToGrep);
  }
  catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }
  delete prof;
}

//YUMENG
void
Analysis::Raw::writeAsText_sparseDBtmp(const char* filenm, bool sm_easyToGrep)
{
  if (!filenm) { return; }

  try {
    FILE* fs = hpcio_fopen_r(filenm);
    if (!fs) {
      DIAG_Throw("error opening tmp sparse-db file '" << filenm << "'");
    }

    hpcrun_fmt_sparse_metrics_t sm;
    int ret = hpcrun_fmt_sparse_metrics_fread(&sm,fs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("error reading tmp sparse-db file '" << filenm << "'");
    }
    hpcrun_fmt_sparse_metrics_fprint(&sm,stdout,NULL, "  ", sm_easyToGrep);
    hpcrun_fmt_sparse_metrics_free(&sm, free);
    hpcio_fclose(fs);
  }
  catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }
}

bool 
Analysis::Raw::profileInfoOffsets_sorter(pms_profile_info_t const& lhs, pms_profile_info_t const& rhs) {
    return lhs.offset< rhs.offset;
}

bool 
Analysis::Raw::traceHdr_sorter(trace_hdr_t const& lhs, trace_hdr_t const& rhs) {
    return lhs.start< rhs.start;
}

void
Analysis::Raw::sortProfileInfo_onOffsets(pms_profile_info_t* x, uint32_t num_prof)
{
    std::sort(x,x+num_prof,&profileInfoOffsets_sorter);
}

void
Analysis::Raw::sortTraceHdrs_onStarts(trace_hdr_t* x, uint32_t num_t)
{
    std::sort(x,x+num_t,&traceHdr_sorter);
}

//YUMENG
void
Analysis::Raw::writeAsText_sparseDBthread(const char* filenm, bool easygrep)
{
  if (!filenm) { return; }

  try {
    FILE* fs = hpcio_fopen_r(filenm);
    if (!fs) {
      DIAG_Throw("error opening thread sparse file '" << filenm << "'");
    }

    pms_hdr_t hdr;
    int ret = pms_hdr_fread(&hdr, fs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("error reading hdr from sparse metrics file '" << filenm << "'");
    }
    pms_hdr_fprint(&hdr, stdout);

    uint32_t num_prof = hdr.num_prof;

    fseek(fs, hdr.prof_info_sec_ptr, SEEK_SET);
    pms_profile_info_t* x;
    ret = pms_profile_info_fread(&x,num_prof,fs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("error reading profile information from sparse metrics file '" << filenm << "'");
    }
    pms_profile_info_fprint(num_prof,x,stdout);

    fseek(fs, hdr.id_tuples_sec_ptr, SEEK_SET);
    id_tuple_t* tuples;
    uint64_t tuples_size = hdr.id_tuples_sec_size;
    ret = id_tuples_pms_fread(&tuples, num_prof,fs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("error reading profile identifier tuples from sparse metrics file '" << filenm << "'");
    }
    id_tuples_pms_fprint(num_prof,tuples_size,tuples,stdout);

    sortProfileInfo_onOffsets(x,num_prof);
    fseek(fs, hdr.id_tuples_sec_ptr + (MULTIPLE_8(hdr.id_tuples_sec_size)), SEEK_SET);
    for(uint i = 0; i<num_prof; i++){
      hpcrun_fmt_sparse_metrics_t sm;
      sm.num_vals = x[i].num_vals;
      sm.num_nz_cct_nodes = x[i].num_nzctxs;
      ret = pms_sparse_metrics_fread(&sm,fs);
      if (ret != HPCFMT_OK) {
        DIAG_Throw("error reading sparse metrics data from sparse metrics file '" << filenm << "'");
      }
      pms_sparse_metrics_fprint(&sm,stdout, NULL, x[i].prof_info_idx, "  ", easygrep);
      pms_sparse_metrics_free(&sm);
    }

    uint64_t footer;
    fread(&footer, sizeof(footer), 1, fs); 
    if(footer != PROFDBft) DIAG_Throw("'" << filenm << "' is incomplete");
    fprintf(stdout, "PROFILEDB FOOTER CORRECT, FILE COMPLETE\n");
   
    pms_profile_info_free(&x);     
    id_tuples_pms_free(&tuples, num_prof);

    hpcio_fclose(fs);
  }
  catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }
}

//YUMENG
void
Analysis::Raw::writeAsText_sparseDBcct(const char* filenm, bool easygrep)
{
  if (!filenm) { return; }

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
    ret = cms_ctx_info_fread(&x, num_ctx,fs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("error reading cct information from sparse metrics file '" << filenm << "'");
    }
    cms_ctx_info_fprint(num_ctx,x,stdout);

    fseek(fs, hdr.ctx_info_sec_ptr + (MULTIPLE_8(hdr.ctx_info_sec_size)), SEEK_SET);
    for(uint i = 0; i<num_ctx; i++){
      if(x[i].num_vals != 0){
        cct_sparse_metrics_t csm;
        csm.ctx_id = x[i].ctx_id;
        csm.num_vals = x[i].num_vals;
        csm.num_nzmids = x[i].num_nzmids;
        ret = cms_sparse_metrics_fread(&csm,fs);
        if (ret != HPCFMT_OK) {
          DIAG_Throw("error reading cct data from sparse metrics file '" << filenm << "'");
        }
        cms_sparse_metrics_fprint(&csm,stdout, "  ", easygrep);
        cms_sparse_metrics_free(&csm);
      }
      
    }

    uint64_t footer;
    fread(&footer, sizeof(footer), 1, fs); 
    if(footer != CCTDBftr) DIAG_Throw("'" << filenm << "' is incomplete");
    fprintf(stdout, "CCTDB FOOTER CORRECT, FILE COMPLETE\n");

    cms_ctx_info_free(&x);
   
    hpcio_fclose(fs);
  }
  catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }
}

//YUMENG
void
Analysis::Raw::writeAsText_tracedb(const char* filenm)
{
  if (!filenm) { return; }

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
    ret = trace_hdrs_fread(&x, num_t,fs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("error reading trace hdrs from tracedb file '" << filenm << "'");
    }
    trace_hdrs_fprint(num_t, x, stdout);

    sortTraceHdrs_onStarts(x, num_t); 
    fseek(fs, hdr.trace_hdr_sec_ptr + (MULTIPLE_8(hdr.trace_hdr_sec_size)), SEEK_SET);
    for(uint i = 0; i<num_t; i++){
      uint64_t start = x[i].start;
      uint64_t end = x[i].end;
      hpctrace_fmt_datum_t* trace_data;
      ret = tracedb_data_fread(&trace_data, (end-start)/timepoint_SIZE, {0}, fs);
      if (ret != HPCFMT_OK) {
        DIAG_Throw("error reading trace data from tracedb file '" << filenm << "'");
      }
      tracedb_data_fprint(trace_data, (end-start)/timepoint_SIZE, x[i].prof_info_idx, {0}, stdout);
      tracedb_data_free(&trace_data);
    }

    if(fgetc(fs) == EOF) 
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
            "      (propMetricId: " << inst.propMetricId << ")\n"
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


void
Analysis::Raw::writeAsText_flat(const char* filenm)
{
  if (!filenm) { return; }
  
  Prof::Flat::ProfileData prof;
  try {
    prof.openread(filenm);
  }
  catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }

  prof.dump(std::cout);
}

