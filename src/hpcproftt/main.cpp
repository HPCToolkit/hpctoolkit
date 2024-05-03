// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

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

#include "../common/Util.hpp"

#include "../common/Raw.hpp"

#include "../common/diagnostics.h"
#include "../common/NaN.h"

//************************ Forward Declarations ******************************

static int
realmain(int argc, char* const* argv);

static int
main_rawData(const std::vector<string>& profileFiles, bool sm_easyToGrep);


//****************************************************************************

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

  for (unsigned int i = 0; i < profileFiles.size(); ++i) {
    const char* fnm = profileFiles[i].c_str();

    // generate nice header
    if (Analysis::Util::option == Analysis::Util::Print_All)  {
      os << std::setfill('=') << std::setw(77) << "=" << std::endl;
      os << fnm << std::endl;
      os << std::setfill('=') << std::setw(77) << "=" << std::endl;
    }
    Analysis::Raw::writeAsText(fnm, sm_easyToGrep); // pass os FIXME
  }
  return 0;
}

//****************************************************************************
