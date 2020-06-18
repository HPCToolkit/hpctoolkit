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
#include <lib/prof/tms-format.h>
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

//YUMENG
bool 
Analysis::Raw::profileInfoOffsets_sorter(tms_profile_info_t const& lhs, tms_profile_info_t const& rhs) {
    return lhs.offset< rhs.offset;
}

//YUMENG
void
Analysis::Raw::sortProfileInfo_onOffsets(tms_profile_info_t* x, uint32_t num_prof)
{
    std::sort(x,x+num_prof,&profileInfoOffsets_sorter);
}

//YUMENG
void
Analysis::Raw::writeAsText_sparseDBthread(const char* filenm, bool easygrep)
{
  if (!filenm) { return; }

  try {
    FILE* fs = hpcio_fopen_r(filenm);
    FILE* ofs = hpcio_fopen_w("readable_thread_major_sparse.db",1);
    if (!fs) {
      DIAG_Throw("error opening thread sparse file '" << filenm << "'");
    }
    uint32_t num_prof;
    tms_profile_info_t* x;
    int ret = tms_profile_info_fread(&x, &num_prof,fs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("error reading profile information from sparse metrics file '" << filenm << "'");
    }
    tms_profile_info_fprint(num_prof,x,ofs);
    sortProfileInfo_onOffsets(x,num_prof);

    for(uint i = 0; i<num_prof; i++){
      hpcrun_fmt_sparse_metrics_t sm;
      sm.tid = x[i].tid;
      sm.num_vals = x[i].num_val;
      sm.num_nz_cct = x[i].num_nzcct;
      ret = tms_sparse_metrics_fread(&sm,fs);
      if (ret != HPCFMT_OK) {
        DIAG_Throw("error reading sparse metrics data from sparse metrics file '" << filenm << "'");
      }
      tms_sparse_metrics_fprint(&sm,ofs,NULL, "  ", easygrep);
      tms_sparse_metrics_free(&sm);
    }

    tms_profile_info_free(&x);
    
    hpcio_fclose(fs);
    hpcio_fclose(ofs);
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
    FILE* ofs = hpcio_fopen_w("readable_cct_major_sparse.db",1);
    if (!fs) {
      DIAG_Throw("error opening cct sparse file '" << filenm << "'");
    }
    uint32_t num_cct;
    cms_cct_info_t* x;
    int ret = cms_cct_info_fread(&x, &num_cct,fs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("error reading cct information from sparse metrics file '" << filenm << "'");
    }
    cms_cct_info_fprint(num_cct,x,ofs);

    for(uint i = 0; i<num_cct; i++){
      if(x[i].num_val != 0){
        cct_sparse_metrics_t csm;
        csm.cct_node_id = x[i].cct_id;
        csm.num_vals = x[i].num_val;
        csm.num_nzmid = x[i].num_nzmid;
        ret = cms_sparse_metrics_fread(&csm,fs);
        if (ret != HPCFMT_OK) {
          DIAG_Throw("error reading cct data from sparse metrics file '" << filenm << "'");
        }
        cms_sparse_metrics_fprint(&csm,ofs, "  ", easygrep);
        cms_sparse_metrics_free(&csm);
      }
      
    }

    cms_cct_info_free(&x);
   
    hpcio_fclose(fs);
    hpcio_fclose(ofs);
  }
  catch (...) {
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

