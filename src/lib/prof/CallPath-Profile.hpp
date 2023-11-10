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

#ifndef prof_Prof_CallPath_Profile_hpp
#define prof_Prof_CallPath_Profile_hpp

//************************* System Include Files ****************************

#include <iostream>
#include <cstdio>

#include <vector>
#include <set>
#include <string>


//*************************** User Include Files ****************************


#include "../prof-lean/hpcrun-fmt.h"

//*************************** Forward Declarations ***************************

//***************************************************************************
// Profile
//***************************************************************************


namespace Prof {

namespace CallPath {


class Profile
{
public:
  static void
  make(const char* fnm, FILE* outfs, bool sm_easyToGrep);


  // fmt_*_fread(): Reads the appropriate hpcrun_fmt object from the
  // file stream 'infs', checking for errors, and constructs
  // appropriate Prof::Profile::CallPath objects.  If 'outfs' is
  // non-null, a textual form of the data is echoed to 'outfs' for
  // human inspection.


  static int
  fmt_fread(FILE* infs,
            std::string ctxtStr, const char* filename, FILE* outfs, bool sm_easyToGrep);

  static int
  fmt_epoch_fread(FILE* infs,
                  const hpcrun_fmt_hdr_t& hdr, const hpcrun_fmt_footer_t& footer,
                  std::string ctxtStr, const char* filename, FILE* outfs, bool sm_easyToGrep);

#if 0
  static int
  fmt_cct_fread(FILE* infs, unsigned int rFlags,
                const metric_tbl_t& metricTbl,
                std::string ctxtStr, FILE* outfs);
#else
//YUMENG: No need to parse metricTbl for sparse format
static int
  fmt_cct_fread(FILE* infs,
                std::string ctxtStr, FILE* outfs);
#endif
};

} // namespace CallPath

} // namespace Prof


//***************************************************************************


//***************************************************************************

#endif /* prof_Prof_CallPath_Profile_hpp */
