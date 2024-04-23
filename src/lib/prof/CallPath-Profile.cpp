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
// Copyright ((c)) 2002-2024, Rice University
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

#define __STDC_LIMIT_MACROS /* stdint; locate here for CentOS 5/gcc 4.1.2) */

#include <iostream>
using std::hex;
using std::dec;

#include <typeinfo>

#include <string>
using std::string;

#include <map>
#include <algorithm>
#include <sstream>

#include <cstdio>
#include <cstring> // strcmp
#include <cmath> // abs

#include <stdint.h>
#include <unistd.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <alloca.h>
#include <linux/limits.h>

//*************************** User Include Files ****************************

#include "../../include/gcc-attr.h"

#include "CallPath-Profile.hpp"

#include "../xml/xml.hpp"
#include "../xml/static.data.h"
using namespace xml;

#include "../analysis/Util.hpp"

#include "../prof-lean/hpcfmt.h"
#include "../prof-lean/hpcrun-fmt.h"
#include "../prof-lean/hpcrun-metric.h"

#include "../support/diagnostics.h"
#include "../support/FileUtil.hpp"
#include "../support/Logic.hpp"
#include "../support/RealPathMgr.hpp"
#include "../support/StrUtil.hpp"


//*************************** Forward Declarations **************************

// Old deadweight function that used to be defined elsewhere
static void
prof_abort
(
  int error_code
)
{
  exit(error_code);
}

//***************************************************************************
// macros
//***************************************************************************

#define DBG 0
#define MAX_PREFIX_CHARS 64



//***************************************************************************
// Profile
//***************************************************************************

namespace Prof {

namespace CallPath {

void
Profile::make(const char* fnm, FILE* outfs, bool sm_easyToGrep) //YUMENG: last arg change to a struct of flags?
{
  int ret;

  FILE* fs = hpcio_fopen_r(fnm);

  if (!fs) {
    if (errno == ENOENT)
      fprintf(stderr, "ERROR: measurement file or directory '%s' does not exist\n",
              fnm);
    else if (errno == EACCES)
      fprintf(stderr, "ERROR: failed to open file '%s': file access denied\n",
              fnm);
    else
      fprintf(stderr, "ERROR: failed to open file '%s': system failure\n",
              fnm);
    prof_abort(-1);
  }

  char* fsBuf = new char[HPCIO_RWBufferSz];
  ret = setvbuf(fs, fsBuf, _IOFBF, HPCIO_RWBufferSz);
  DIAG_AssertWarn(ret == 0, "Profile::make: setvbuf!");

  ret = fmt_fread(fs, fnm, fnm, outfs, sm_easyToGrep);

  hpcio_fclose(fs);

  delete[] fsBuf;
}


int
Profile::fmt_fread(FILE* infs,
                   std::string ctxtStr, const char* filename, FILE* outfs, bool sm_easyToGrep)
{
  int ret;


  // ------------------------------------------------------------
  // footer - YUMENG
  // ------------------------------------------------------------
  hpcrun_fmt_footer_t footer;
  fseek(infs, 0, SEEK_END);
  size_t footer_position = ftell(infs) - SF_footer_SIZE;
  fseek(infs, footer_position, SEEK_SET);
  ret = hpcrun_fmt_footer_fread(&footer, infs);
  if(ret != HPCFMT_OK){
    fprintf(stderr, "ERROR: error reading footer section in '%s', maybe it is not complete\n", filename);
    prof_abort(-1);
  }
  if(getc(infs) != EOF){
    fprintf(stderr, "ERROR: data exists after footer section in '%s'\n", filename);
    prof_abort(-1);
  }

  // ------------------------------------------------------------
  // hdr
  // ------------------------------------------------------------
  //YUMENG: file not consecutive anymore after making boundary multiple of 1024
  fseek(infs, footer.hdr_start, SEEK_SET);

  hpcrun_fmt_hdr_t hdr;
  ret = hpcrun_fmt_hdr_fread(&hdr, infs, malloc);
  if (ret != HPCFMT_OK) {
    fprintf(stderr, "ERROR: error reading 'fmt-hdr' in '%s': either the file "
            "is not a profile or it is corrupted\n", filename);
    prof_abort(-1);
  }
  //YUMENG check if the ending position match the recorded in footer
  if((uint64_t)ftell(infs) != footer.hdr_end){
    fprintf(stderr, "ERROR: 'fmt-hdr' is successfully read but the data seems off the recorded location in '%s'\n",
     filename);
     prof_abort(-1);
  }
  if ( !(hdr.version >= HPCRUN_FMT_Version_20) ) {
    DIAG_Throw("unsupported file version '" << hdr.versionStr << "'");
  }

  if (outfs) {
    if (Analysis::Util::option == Analysis::Util::Print_All)
      hpcrun_fmt_hdr_fprint(&hdr, outfs);
  }

  // ------------------------------------------------------------
  // epoch: Read each epoch and merge them to form one Profile
  // ------------------------------------------------------------

  //YUMENG: no epoch info needed
  //unsigned int num_epochs = 0;
  //size_t file_cur = 0;
  //while ( !feof(infs) && (file_cur != footer.footer_offset)) {

    //YUMENG: no epoch info needed
    //string myCtxtStr = "epoch " + StrUtil::toStr(num_epochs + 1);
    //ctxtStr += ": " + myCtxtStr;

    try {
      ret = fmt_epoch_fread(infs, hdr, footer,
                            ctxtStr, filename, outfs, sm_easyToGrep);
      //if (ret == HPCFMT_EOF) {
            //  break;
      // }
    }
    catch (const Diagnostics::Exception& x) {
      DIAG_Throw("error reading " << ctxtStr << ": " << x.what());
    }

   //YUMENG: no epoch info needed
   //num_epochs++;

   //footer print YUMENG
   //file_cur = ftell(infs);
   if(outfs){
     if (Analysis::Util::option == Analysis::Util::Print_All)
       hpcrun_fmt_footer_fprint(&footer, outfs, "  ");
   }

  //}

  // ------------------------------------------------------------
  //
  // ------------------------------------------------------------

  if (outfs) {
    if (Analysis::Util::option == Analysis::Util::Print_All) {
      //YUMENG: no epoch info needed
      //fprintf(outfs, "\n[You look fine today! (num-epochs: %u)]\n", num_epochs);
      fprintf(outfs, "\n[You look fine today!]\n");
    }
  }

  hpcrun_fmt_hdr_free(&hdr, free);

  return HPCFMT_OK;
}


int
Profile::fmt_epoch_fread(FILE* infs,
                         const hpcrun_fmt_hdr_t& hdr, const hpcrun_fmt_footer_t& footer,
                         std::string ctxtStr, const char* filename,
                         FILE* outfs, bool sm_easyToGrep)
{
  using namespace Prof;

  string profFileName = (filename) ? filename : "";

  int ret;

  // ----------------------------------------
  // loadmap
  // ----------------------------------------
  //YUMENG: file not consecutive anymore after making boundary multiple of 1024
  fseek(infs, footer.loadmap_start, SEEK_SET);

  loadmap_t loadmap_tbl;
  ret = hpcrun_fmt_loadmap_fread(&loadmap_tbl, infs, malloc);

  if (ret == HPCFMT_EOF) {
    return HPCFMT_EOF;
  }
  if (ret != HPCFMT_OK) {
    DIAG_Throw("error reading 'loadmap'");
  }
  //YUMENG check if the ending position match the recorded in footer
  if((uint64_t)ftell(infs) != footer.loadmap_end){
    fprintf(stderr, "ERROR: 'loadmap' is successfully read but the data seems off the recorded location in '%s'\n",
     filename);
     prof_abort(-1);
  }
  if (outfs) {
    size_t gpubin_suffix_len = strlen("gpubin");
    if (Analysis::Util::option == Analysis::Util::Print_LoadModule_Only) {
      for (uint32_t i = 0; i < loadmap_tbl.len; i++) {

        loadmap_entry_t* x = &loadmap_tbl.lst[i];

        // make sure we eliminate the <vmlinux> and <vdso> load modules
        // These modules have prefix '<' and hopefully it doesn't change
        if ((x->name != NULL && x->name[0] != '<') &&
            (x->flags & LOADMAP_ENTRY_ANALYZE)) {

          // for any gpubin, erase any kernel name hash following
          // "gpubin" in a load module name
          string name(x->name);
          size_t pos = name.find("gpubin.");
          if (pos != string::npos) name.erase(pos+gpubin_suffix_len);

          fprintf(outfs, "%s\n", name.c_str());
        }
      }
      // hack: case for hpcproftt with --lm option
      // by returning HPCFMT_EOF we force hpcproftt to exit the loop
      return HPCFMT_EOF;
    }
    hpcrun_fmt_loadmap_fprint(&loadmap_tbl, outfs);
  }


  // ------------------------------------------------------------
  // Create Profile
  // ------------------------------------------------------------

  // ----------------------------------------
  // obtain meta information
  // ----------------------------------------

  const char* val;

  // -------------------------
  // program name
  // -------------------------

  string progNm;
  val = hpcfmt_nvpairList_search(&(hdr.nvps), HPCRUN_FMT_NV_prog);
  if (val && strlen(val) > 0) {
    progNm = val;
  }

  // -------------------------
  // parallelism context (mpi rank, thread id)
  // -------------------------
  string mpiRankStr, tidStr;

  // val = hpcfmt_nvpairList_search(&(hdr.nvps), HPCRUN_FMT_NV_jobId);

  val = hpcfmt_nvpairList_search(&(hdr.nvps), HPCRUN_FMT_NV_mpiRank);
  if (val) {
    mpiRankStr = val;
  }

  val = hpcfmt_nvpairList_search(&(hdr.nvps), HPCRUN_FMT_NV_tid);
  if (val) {
    tidStr = val;
  }

  // -------------------------
  // trace information
  // -------------------------

  bool     haveTrace = false;
  string   traceFileName;

  string   traceMinTimeStr, traceMaxTimeStr;
  uint64_t traceMinTime = UINT64_MAX, traceMaxTime = 0;

  val = hpcfmt_nvpairList_search(&(hdr.nvps), HPCRUN_FMT_NV_traceMinTime);
  if (val) {
    traceMinTimeStr = val;
    if (val[0] != '\0') { traceMinTime = StrUtil::toUInt64(traceMinTimeStr); }
  }

  val = hpcfmt_nvpairList_search(&(hdr.nvps), HPCRUN_FMT_NV_traceMaxTime);
  if (val) {
    traceMaxTimeStr = val;
    if (val[0] != '\0') { traceMaxTime = StrUtil::toUInt64(traceMaxTimeStr); }
  }

  haveTrace = (traceMinTime != 0 && traceMaxTime != 0);

  // Note: 'profFileName' can be empty when reading from a memory stream
  if (haveTrace && !profFileName.empty()) {
    // TODO: extract trace file name from profile
    static const string ext_prof = string(".") + HPCRUN_ProfileFnmSfx;
    static const string ext_trace = string(".") + HPCRUN_TraceFnmSfx;

    traceFileName = profFileName;
    size_t ext_pos = traceFileName.find(ext_prof);
    if (ext_pos != string::npos) {
      traceFileName.replace(traceFileName.begin() + ext_pos,
                            traceFileName.end(), ext_trace);
      // DIAG_Assert(FileUtil::isReadable(traceFileName));
    }
  }


  // ------------------------------------------------------------
  // cct
  // ------------------------------------------------------------

  //file not consecutive anymore after making boundary multiple of 1024
  fseek(infs, footer.cct_start, SEEK_SET);
  fmt_cct_fread(infs, ctxtStr, outfs);
  //check if the ending position match the recorded in footer
  if((uint64_t)ftell(infs) != footer.cct_end){
    fprintf(stderr, "ERROR: 'cct' is successfully read but the data seems off the recorded location in '%s'\n",
     filename);
     prof_abort(-1);
  }

  // ----------------------------------------
  // metric-tbl
  // ----------------------------------------
  //YUMENG: file not consecutive anymore after making boundary multiple of 1024
  fseek(infs, footer.met_tbl_start, SEEK_SET);

  metric_tbl_t metricTbl;

  ret = hpcrun_fmt_metricTbl_fread(&metricTbl, infs, hdr.version, malloc);
  if (ret != HPCFMT_OK) {
    DIAG_Throw("error reading 'metric-tbl'");
  }
  //YUMENG check if the ending position match the recorded in footer
  if((uint64_t)ftell(infs) != footer.met_tbl_end){
    fprintf(stderr, "ERROR: 'metric-tbl' is successfully read but the data seems off the recorded location in '%s'\n",
     filename);
     prof_abort(-1);
  }
  if (outfs) {
    hpcrun_fmt_metricTbl_fprint(&metricTbl, outfs);
  }

  // ----------------------------------------
  // id-tuple dictionary
  // ----------------------------------------
  fseek(infs, footer.idtpl_dxnry_start, SEEK_SET);

  hpcrun_fmt_idtuple_dxnry_t idtuple_dxnry;
  ret = hpcrun_fmt_idtuple_dxnry_fread(&idtuple_dxnry, infs, malloc);
  if (ret != HPCFMT_OK) {
    DIAG_Throw("error reading 'id-tuple dxnry'");
  }
  if((uint64_t)ftell(infs) != footer.idtpl_dxnry_end){
    fprintf(stderr, "ERROR: 'id-tuple dxnry' is successfully read but the data seems off the recorded location in '%s'\n",
     filename);
     prof_abort(-1);
  }
  hpcrun_fmt_idtuple_dxnry_fprint(&idtuple_dxnry, outfs);
  hpcrun_fmt_idtuple_dxnry_free(&idtuple_dxnry, free);

  // ----------------------------------------
  // cct_metrics_sparse_values - YUMENG
  // ----------------------------------------
  //YUMENG: file not consecutive anymore after making boundary multiple of 1024
  fseek(infs, footer.sm_start, SEEK_SET);

  hpcrun_fmt_sparse_metrics_t sparse_metrics;
  ret = hpcrun_fmt_sparse_metrics_fread(&sparse_metrics,infs);
  if (ret != HPCFMT_OK) {
    DIAG_Throw("error reading 'metric-tbl'");
  }
  //check if the ending position match the recorded in footer
  if((uint64_t)ftell(infs) != footer.sm_end){
    fprintf(stderr, "ERROR: 'sparse metrics' is successfully read but the data seems off the recorded location in '%s'\n",
     filename);
     prof_abort(-1);
  }
  hpcrun_fmt_sparse_metrics_fprint(&sparse_metrics,outfs, &metricTbl, "  ", sm_easyToGrep);
  hpcrun_fmt_sparse_metrics_free(&sparse_metrics, free);

  //YUMENG: no epoch info
  //hpcrun_fmt_epochHdr_free(&ehdr, free);
  hpcrun_fmt_metricTbl_free(&metricTbl, free);

  return HPCFMT_OK;
}


int
Profile::fmt_cct_fread(FILE* infs,
                       std::string ctxtStr, FILE* outfs)
{

  DIAG_Assert(infs, "Bad file descriptor!");

  int ret = HPCFMT_ERR;

  // ------------------------------------------------------------
  // Read number of cct nodes
  // ------------------------------------------------------------
  uint64_t numNodes = 0;
  hpcfmt_int8_fread(&numNodes, infs);

  // ------------------------------------------------------------
  // Read each CCT node
  // ------------------------------------------------------------
  if (outfs) {
    fprintf(outfs, "[cct: (num-nodes: %" PRIu64 ")\n", numNodes);
  }

  hpcrun_fmt_cct_node_t nodeFmt;

  const epoch_flags_t flags = {.bits=0};

  for (unsigned int i = 0; i < numNodes; ++i) {
    // ----------------------------------------------------------
    // Read the node
    // ----------------------------------------------------------
    ret = hpcrun_fmt_cct_node_fread(&nodeFmt, flags, infs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("Error reading CCT node " << nodeFmt.id);
    }
    if (outfs) {

      hpcrun_fmt_cct_node_fprint(&nodeFmt, outfs, flags,
                                  "  ");
    }
  }

  if (outfs) {
    fprintf(outfs, "]\n");
  }

  return HPCFMT_OK;
}

} // namespace CallPath

} // namespace Prof
