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
// Copyright ((c)) 2002-2018, Rice University
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

// This file defines the API for Line Map Info that we get directly
// from libdwarf.
//
// The client (Struct.cpp) should use the classes LineMap and
// LineRange.  Everything else is internal.

//***************************************************************************

#ifndef Banal_Linemap_hpp
#define Banal_Linemap_hpp

#include <include/uint.h>
#include <lib/isa/ISATypes.hpp>
#include <lib/support/StringTable.hpp>

#include "dwarf.h"
#include "libdwarf.h"

#include <map>

class ElfFile;

//----------------------------------------------------------------------

// One segment in the raw line map from libdwarf.
// This is internal, not seen directly by the client.
//
// file is an index into m_str_tab.
// line is the actual line number.
//
class LineMapInfo {
public:
  uint file;
  uint line;

  LineMapInfo(uint f = 0, uint l = 0) {
    file = f;
    line = l;
  }
};

typedef std::map <VMA, LineMapInfo> InternalLineMap;

//----------------------------------------------------------------------

// External classes for Struct.cpp client.

class LineRange {
public:
  VMA  start;
  VMA  end;
  const char *filenm;
  uint lineno;
};

class LineMap {
private:
  InternalLineMap   m_line_map;
  HPC::StringTable  m_str_tab;
  uint  m_empty_index;

  void do_line_map(Dwarf_Debug, Dwarf_Die);
  void do_comp_unit(Dwarf_Debug, int, int, long, long);
  void do_dwarf(ElfFile *elf);

public:
  LineMap();
  void readFile(ElfFile *elf);
  void getLineRange(VMA, LineRange &);
};

#endif
