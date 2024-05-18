// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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


#include "lean/hpcrun-fmt.h"

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

static int
  fmt_cct_fread(FILE* infs,
                std::string ctxtStr, FILE* outfs);
};

} // namespace CallPath

} // namespace Prof


//***************************************************************************


//***************************************************************************

#endif /* prof_Prof_CallPath_Profile_hpp */
