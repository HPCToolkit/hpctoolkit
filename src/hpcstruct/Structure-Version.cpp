// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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
