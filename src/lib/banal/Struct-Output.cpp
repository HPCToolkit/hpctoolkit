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
// 1. All output to the hpcstruct file should come from this file.
//
// 2. Opening the file and creating the ostream is handled in
// tool/hpcstruct/main.c and lib/support/IOUtil.hpp.
//
// 3. We allow ostream = NULL to mean that we don't want output.

// FIXME and TODO:
//
// 4. Add vma ranges v="{[0x400c2d-0x400c32) [0x400c8c-0x400c94)}"
// with multiple ranges.
//
// 5. Add full vma ranges for <P> proc tags.
//
// 7. Fix targ401420 names in <P> tags.
//
// 8. Fill in proc body with <L>, <A> and <S> tags (duh).

//***************************************************************************

#include <ostream>
#include <string>

#include <CFG.h>

#include <lib/xml/xml.hpp>
#include "Struct-Inline.hpp"
#include "Struct-Output.hpp"
#include "Struct-Skel.hpp"

using namespace Dyninst;
using namespace Inline;
using namespace std;

#define INDENT  "  "
#define INIT_LM_INDEX  2

static long next_index;

static const char * hpcstruct_xml_head =
#include <lib/xml/hpc-structure.dtd.h>
  ;

//----------------------------------------------------------------------

// Helpers to generate fields inside tags.  The macros are designed to
// fit within << operators.

// this generates pre-order
#define INDEX  \
  " i=\"" << next_index++ << "\""

#define NUMBER(label, num)  \
  " " << label << "=\"" << num << "\""

#define STRING(label, str)  \
  " " << label << "=\"" << xml::EscapeStr(str) << "\""

#define VRANGE(vma, len)  \
  " v=\"{[0x" << hex << vma << "-0x" << vma + len << dec << ")}\""

static void
doIndent(ostream * os, int depth)
{
  for (int n = 1; n <= depth; n++) {
    *os << INDENT;
  }
}

//----------------------------------------------------------------------

namespace BAnal {
namespace Output {

// DOCTYPE header and <HPCToolkitStructure> tag.
void
printStructFileBegin(ostream * os)
{
  if (os == NULL) {
    return;
  }

  *os << "<?xml version=\"1.0\"?>\n"
      << "<!DOCTYPE HPCToolkitStructure [\n"
      << hpcstruct_xml_head
      << "]>\n"
      << "<HPCToolkitStructure i=\"0\" version=\"4.6\" n=\"\">\n";
}

// Closing tag.
void
printStructFileEnd(ostream * os)
{
  if (os == NULL) {
    return;
  }

  *os << "</HPCToolkitStructure>\n";
  os->flush();
}

//----------------------------------------------------------------------

// Begin <LM> load module tag.
void
printLoadModuleBegin(ostream * os, string lmName)
{
  if (os == NULL) {
    return;
  }

  next_index = INIT_LM_INDEX;

  *os << "<LM"
      << INDEX
      << STRING("n", lmName)
      << " v=\"{}\">\n";
}

// Closing </LM> tag.
void
printLoadModuleEnd(ostream * os)
{
  if (os == NULL) {
    return;
  }

  *os << "</LM>\n";
}

//----------------------------------------------------------------------

// Begin <F> file tag.
void
printFileBegin(ostream * os, FileInfo * finfo)
{
  if (os == NULL || finfo == NULL) {
    return;
  }

  doIndent(os, 1);
  *os << "<F"
      << INDEX
      << STRING("n", finfo->name)
      << ">\n";
}

// Closing </F> tag.
void
printFileEnd(ostream * os, FileInfo * finfo)
{
  if (os == NULL || finfo == NULL) {
    return;
  }

  doIndent(os, 1);
  *os << "</F>\n";
}

//----------------------------------------------------------------------

// Entry point for <P> proc tag and its subtree.
void
printProc(ostream * os, ProcInfo * pinfo)
{
  if (os == NULL || pinfo == NULL) {
    return;
  }

  ParseAPI::Function * func = pinfo->func;
  TreeNode * root = pinfo->root;

  doIndent(os, 2);
  *os << "<P"
      << INDEX
      << STRING("n", pinfo->prettyName);

  if (pinfo->linkName != pinfo->prettyName) {
    *os << STRING("ln", pinfo->linkName);
  }

  *os << VRANGE(func->addr(), 1)
      << ">\n";

  // temp placeholder for proc body
  if (root != NULL && ! root->stmtMap.empty()) {
    StmtInfo * sinfo = root->stmtMap.begin()->second;

    doIndent(os, 3);
    *os << "<S"
	<< INDEX
	<< NUMBER("l", sinfo->line_num)
	<< VRANGE(sinfo->vma, sinfo->len)
	<< "/>\n";
  }

  doIndent(os, 2);
  *os << "</P>\n";
}

}  // namespace Output
}  // namespace BAnal
