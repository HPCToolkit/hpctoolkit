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
// Copyright ((c)) 2002-2015, Rice University
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

// This file defines the API between symtabAPI and the banal code for
// the static inline support.
//
// For each VM address that came from inlined code, symtab provides a
// C++ list representing the sequence of inlining steps.  (The list
// front is outermost, back is innermost.)  An empty list means not
// inlined code.
//
// Each node in the list contains: (1) the file name and (2) line
// number in the source code, and (3) the procedure name at the call
// site.

//***************************************************************************

#ifndef Banal_Struct_Inline_hpp
#define Banal_Struct_Inline_hpp

#include <list>
#include <lib/isa/ISATypes.hpp>
#include <lib/support/SrcFile.hpp>

#include <Symtab.h>
#include <Function.h>

namespace Inline {

class InlineNode {

private:
  std::string  m_filenm;
  std::string  m_procnm;
  SrcFile::ln  m_lineno;

public:
  InlineNode(std::string &file, std::string &proc, SrcFile::ln line) {
    m_filenm = file;
    m_procnm = proc;
    m_lineno = line;
  }

  std::string & getFileName() { return m_filenm; }
  std::string & getProcName() { return m_procnm; }
  SrcFile::ln getLineNum() { return m_lineno; }

};  // class InlineNode

typedef list <InlineNode> InlineSeqn;

bool openSymtab(std::string filename);
bool closeSymtab();
bool analyzeAddr(InlineSeqn &nodelist, VMA addr);

}  // namespace Inline

#endif
