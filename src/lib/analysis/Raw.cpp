// -*-Mode: C++;-*-

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
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

//*************************** User Include Files ****************************

#include "Raw.hpp"
#include "Util.hpp"

#include <lib/prof-juicy/CallPath-Profile.hpp>
#include <lib/prof-juicy/Flat-ProfileData.hpp>
#include <lib/prof-lean/hpcio.h>
#include <lib/prof-lean/hpcfmt.h>
#include <lib/prof-lean/hpcrun-fmt.h>

#include <lib/support/diagnostics.h>

//*************************** Forward Declarations ***************************

//****************************************************************************

void 
Analysis::Raw::writeAsText(/*destination,*/ const char* filenm)
{
  using namespace Analysis::Util;

  ProfType_t ty = getProfileType(filenm);
  if (ty == ProfType_Callpath) {
    writeAsText_callpath(filenm);
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
  else {
    DIAG_Die(DIAG_Unimplemented);
  }
}


void
Analysis::Raw::writeAsText_callpath(const char* filenm)
{
  if (!filenm) { return; }

  Prof::CallPath::Profile* prof = NULL;
  try {
    prof = Prof::CallPath::Profile::make(filenm, 0/*rFlags*/, stdout);
  }
  catch (...) {
    DIAG_EMsg("While reading '" << filenm << "'...");
    throw;
  }
  delete prof;
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
	uint64_t mval_bits = 0;
	int ret = hpcfmt_byte8_fread(&mval_bits, fs);
	if (ret != HPCFMT_OK) {
	  DIAG_Throw("error reading trace file '" << filenm << "'");
	}

	double mval = (double)mval_bits;
	fprintf(stdout, "%14g ", mval);
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

    int ret = hpctrace_fmt_hdr_fread(fs);
    if (ret != HPCFMT_OK) {
      DIAG_Throw("error reading trace file '" << filenm << "'");
    }

    hpctrace_fmt_hdr_fprint(stdout);

    while ( !feof(fs) ) {
      // Read trace record (exit on EOF)
      uint64_t timestamp_bits = 0;
      double   timestamp = 0;
      ret = hpcfmt_byte8_fread(&timestamp_bits, fs);
      timestamp = (double)timestamp_bits;
      if (ret == HPCFMT_EOF) {
	break;
      }
      else if (ret == HPCFMT_ERR) {
	DIAG_Throw("error reading trace file '" << filenm << "'");
      }

      uint cctId;
      ret = hpcfmt_byte4_fread(&cctId, fs);
      if (ret != HPCFMT_OK) {
	DIAG_Throw("error reading trace file '" << filenm << "'");
      }

      fprintf(stdout, "(%f, %u)\n", timestamp, cctId);
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

