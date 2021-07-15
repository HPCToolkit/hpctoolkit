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
// Copyright ((c)) 2002-2021, Rice University
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

//************************ System Include Files ******************************

#include <iostream>
#include <fstream>
#include <iomanip>

#include <string>
using std::string;

#include <exception>

//*********************** Xerces Include Files *******************************

//************************* User Include Files *******************************

#include "Args.hpp"

#include <lib/analysis/Flat-SrcCorrelation.hpp>
#include <lib/analysis/Flat-ObjCorrelation.hpp>
#include <lib/analysis/Raw.hpp>

#include <lib/support/diagnostics.h>
#include <lib/support/NaN.h>

//************************ Forward Declarations ******************************

static int
realmain(int argc, char* const* argv);

static int
main_rawData(const std::vector<string>& profileFiles, bool sm_easyToGrep);


//****************************************************************************

void 
prof_abort
(
  int error_code
)
{
  exit(error_code);
}


int 
main(int argc, char* const* argv) 
{
  int ret;

  try {
    ret = realmain(argc, argv);
  }
  catch (const Diagnostics::Exception& x) {
    DIAG_EMsg(x.message());
    exit(1);
  } 
  catch (const std::bad_alloc& x) {
    DIAG_EMsg("[std::bad_alloc] " << x.what());
    exit(1);
  }
  catch (const std::exception& x) {
    DIAG_EMsg("[std::exception] " << x.what());
    exit(1);
  }
  catch (...) {
    DIAG_EMsg("Unknown exception encountered!");
    exit(2);
  }

  return ret;
}


static int 
realmain(int argc, char* const* argv) 
{
  Args args(argc, argv);  // exits if error on command line
  return main_rawData(args.profileFiles, args.sm_easyToGrep); 
}


//****************************************************************************
//
//****************************************************************************

//YUMENG: second argument: if more flags are needed, maybe build a struct to include all flags and pass the struct around
static int
main_rawData(const std::vector<string>& profileFiles, bool sm_easyToGrep)
{
  std::ostream& os = std::cout;

  for (uint i = 0; i < profileFiles.size(); ++i) {
    const char* fnm = profileFiles[i].c_str();

    // generate nice header
    os << std::setfill('=') << std::setw(77) << "=" << std::endl;
    os << fnm << std::endl;
    os << std::setfill('=') << std::setw(77) << "=" << std::endl;

    Analysis::Raw::writeAsText(fnm, sm_easyToGrep); // pass os FIXME
  }
  return 0;
}

//****************************************************************************
