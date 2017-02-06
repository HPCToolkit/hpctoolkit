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
// Copyright ((c)) 2002-2017, Rice University
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

// This file writes an hpcstruct file directly from the internal
// TreeNode format (Struct-Inline.hpp) to an output stream without
// using the Prof::Struct objects (prof/Struct-Tree.hpp).
//
// Notes:
// 1. Opening the file and creating the ostream is handled in
// tool/hpcstruct/main.c and lib/support/IOUtil.hpp.
//
// 2. We allow outFile = NULL to mean that we don't want output.

//***************************************************************************

#include <ostream>

#include "Struct-Inline.hpp"
#include "Struct-Output.hpp"

using namespace std;

static const char * hpcstruct_xml_head =
#include <lib/xml/hpc-structure.dtd.h>
  ;

//----------------------------------------------------------------------

namespace BAnal {
namespace Output {

void
printStructBegin(ostream *outFile)
{
  if (outFile == NULL) {
    return;
  }

  *outFile << "<?xml version=\"1.0\"?>\n"
	   << "<!DOCTYPE HPCToolkitStructure [\n"
	   << hpcstruct_xml_head
	   << "]>\n"
	   << "<HPCToolkitStructure i=\"0\" version=\"4.6\" n=\"\">\n";

  *outFile << "\n\n";
}


void printStructEnd(ostream *outFile)
{
  if (outFile == NULL) {
    return;
  }

  *outFile << "</HPCToolkitStructure>\n";
}

}  // namespace Output
}  // namespace BAnal
