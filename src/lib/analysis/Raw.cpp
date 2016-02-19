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
// Copyright ((c)) 2002-2016, Rice University
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

