// -*-Mode: C++;-*- // technically C99

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
//   Structure-Version.cpp
//
// Purpose:
//   Ensure that a structure file corresponds to the current version
//
//***************************************************************************

//***************************************************************************
// global includes
//***************************************************************************


#include <iostream>
#include <fstream>
#include <regex>

#include "xml.hpp"
#include "static.data.h"


//***************************************************************************
// private operations
//***************************************************************************

//---------------------------------------------------------------------------
// Function: getVersion
//
// Purpose:
//   find and return the version number of an hpcstruct file
//
// Arguments:
//   structFileName: name of a structure file
//
// Return value:
//   version number of an hpcstruct file; empty string otherwise
//---------------------------------------------------------------------------
static std::string
getVersion(const char *structFileName)
{
  std::string result;

  std::ifstream input;
  input.open(structFileName);

  std::string line;
  while(std::getline(input, line)) {
    std::regex version_line("<!--\\s+Version\\s+\\d+\\.\\d+\\s+-->");
    std::smatch match;
    // search for line matching version definition
    if (regex_search(line, match, version_line)) {
      // extract version string from the line
      std::regex version("\\d+.\\d+");
      if (regex_search(line, match, version)) {
        result = match.str();
        break;
      }
    }
  }

  return result;
}


//***************************************************************************
// interface operations
//***************************************************************************


bool StructureFileCheckVersion(const char *structureFileName)
{
  std::string version = getVersion(structureFileName);
  return (version.compare(HPCSTRUCT_DTD_VERSION) == 0);
}
