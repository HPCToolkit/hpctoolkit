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

// This file reads libdwarf directly and builds a single map of line
// map info for queries in Struct.cpp.
//
// Note: in the parallel case, readFile() is serial code and should be
// called first.  Then, can make parallel queries to getLineRange().
//
// FIXME and TODO:
//
// 1. fix error/warning handling.
//
// 2. read debug info from alternate file.
//
// 3. handle cases of line map entries that come out of order.
//
// 4. use map insert hint when building line map.
//
// 5. add cached values for libdwarf case in Struct.cpp.
//
// 6. replace binutils line map in makeStructureSimple.

//***************************************************************************

#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <ostream>
#include <string>
#include <map>
#include <vector>

#include <include/uint.h>
#include <lib/isa/ISATypes.hpp>
#include <lib/support/StringTable.hpp>

#include "Linemap.hpp"
#include "ElfHelper.hpp"

#include "dwarf.h"
#include "libdwarf.h"

using namespace std;

#define DEBUG_RAW_LINE_MAP   0
#define DEBUG_FULL_LINE_MAP  0

//----------------------------------------------------------------------

// Add line map info for one comp unit.
//
void
LineMap::do_line_map(Dwarf_Debug dw_dbg, Dwarf_Die dw_die)
{
  Dwarf_Line *linebuf;
  Dwarf_Signed count;
  Dwarf_Signed file_count;
  Dwarf_Error dw_error;
  char **file_names;
  int ret, n;

  ret = dwarf_srclines(dw_die, &linebuf, &count, &dw_error);
  if (ret != DW_DLV_OK) {
    // warnx("dwarf_srclines failed");
    return;
  }

  ret = dwarf_srcfiles(dw_die, &file_names, &file_count, &dw_error);
  if (ret != DW_DLV_OK) {
    // warnx("dwarf_srcfiles failed");
    return;
  }

  // make map of dwarf src file index to m_str_tab index
  vector <uint> fileVec;
  fileVec.resize(file_count);

  for (n = 0; n < file_count; n++) {
    fileVec[n] = m_str_tab.str2index(file_names[n]);
  }

  // create unsigned copy of count to avoid type errors in the loop
  Dwarf_Unsigned ufile_count = (Dwarf_Unsigned) file_count; 

  for (n = 0; n < count; n++) {
    Dwarf_Addr addr;
    Dwarf_Unsigned lineno = 0;
    Dwarf_Unsigned fileno;
    Dwarf_Bool is_end = 0;
    uint file_index = m_empty_index;
    int ret;

    ret = dwarf_lineaddr(linebuf[n], &addr, &dw_error);
    if (ret != DW_DLV_OK) {
      // warnx("dwarf_lineaddr failed");
    }

    ret = dwarf_lineno(linebuf[n], &lineno, &dw_error);
    if (ret != DW_DLV_OK) {
      // warnx("dwarf_lineno failed");
    }

    ret = dwarf_line_srcfileno(linebuf[n], &fileno, &dw_error);
    if (ret != DW_DLV_OK) {
      // warnx("dwarf_line_srcfileno failed");
    }

    if (fileno > 0 && fileno <= ufile_count && lineno > 0) {
      file_index = fileVec[fileno - 1];
    }

    ret = dwarf_lineendsequence(linebuf[n], &is_end, &dw_error);
    if (ret != DW_DLV_OK) {
      // warnx("dwarf_lineendsequence failed");
    }

#if DEBUG_RAW_LINE_MAP
    cout << "0x" << hex << addr << dec;
    if (is_end) {
      cout << "    (end)\n";
    }
    else {
      cout << "  " << setw(6) << lineno
	   << "    " << m_str_tab.index2str(file_index) << "\n";
    }
#endif

    if (n == count - 1) {
      is_end = 1;
    }

    if (lineno > 0 && ! is_end) {
      // always insert a valid entry
      m_line_map[addr] = LineMapInfo(file_index, lineno);
    }
    else {
      // don't overwrite a valid entry with empty info
      auto it = m_line_map.find(addr);
      if (it == m_line_map.end()) {
	m_line_map[addr] = LineMapInfo(m_empty_index, 0);
      }
    }
  }

  // free src files and line map info
  for (n = 0; n < file_count; n++) {
    dwarf_dealloc(dw_dbg, file_names[n], DW_DLA_STRING);
  }
  dwarf_dealloc(dw_dbg, file_names, DW_DLA_LIST);
  dwarf_srclines_dealloc(dw_dbg, linebuf, count);
}

//----------------------------------------------------------------------

void
LineMap::do_comp_unit(Dwarf_Debug dw_dbg, int num, int vers, long hdr_len, long hdr_off)
{
  Dwarf_Die dw_die;
  Dwarf_Error dw_error;
  int ret;

  ret = dwarf_siblingof_b(dw_dbg, NULL, 1, &dw_die, &dw_error);

  if (ret == DW_DLV_OK) {
    do_line_map(dw_dbg, dw_die);
    dwarf_dealloc(dw_dbg, dw_die, DW_DLA_DIE);
  }
}

//----------------------------------------------------------------------

void
LineMap::do_dwarf(ElfFile *elfFile)
{
  Dwarf_Debug dw_dbg;
  Dwarf_Error dw_error;
  Dwarf_Unsigned cu_hdr_len;
  Dwarf_Half     cu_vers;
  Dwarf_Unsigned cu_hdr_off;
  int num, ret;

  ret = dwarf_elf_init(elfFile->getElf(), DW_DLC_READ, NULL, NULL, &dw_dbg, &dw_error);
  if (ret == DW_DLV_NO_ENTRY) {
    errx(1, "no debug section in file: %s", elfFile->getFileName().c_str());
  }
  else if (ret != DW_DLV_OK) {
    errx(1, "dwarf_init failed");
  }

  // iterate over comp units
  for (num = 0;; num++) {
    ret = dwarf_next_cu_header_d(dw_dbg, 1, &cu_hdr_len, &cu_vers, NULL, NULL,
		NULL, NULL, NULL, NULL, &cu_hdr_off, NULL, &dw_error);

    if (ret == DW_DLV_NO_ENTRY) {
      break;
    }
    else if (ret !=  DW_DLV_OK) {
      errx(1, "dwarf_next_cu_header_d failed");
    }

#if DEBUG_RAW_LINE_MAP
    cout << "\ncomp unit # " << num << ":\n";
#endif

    do_comp_unit(dw_dbg, num, cu_vers, cu_hdr_len, cu_hdr_off);
  }

  ret = dwarf_finish(dw_dbg, &dw_error);
  if (ret != DW_DLV_OK) {
    // warnx("dwarf_finish failed");
  }
}

//----------------------------------------------------------------------

// Initialize empty line map.
//
LineMap::LineMap()
{
  // put sentinels at each end, so we don't have to deal with
  // map.begin() and end().
  m_empty_index = m_str_tab.str2index("");
  m_line_map[0] = LineMapInfo(m_empty_index, 0);
  m_line_map[VMA_MAX] = LineMapInfo(m_empty_index, 0);
}


// Read one file and put into InternalLineMap.
//
void
LineMap::readFile(ElfFile *elfFile)
{
  do_dwarf(elfFile);

#if DEBUG_FULL_LINE_MAP
  cout << "\nfull line map:\n\n";

  for (auto it = m_line_map.begin(); it != m_line_map.end(); ++it) {
    cout << "0x" << hex << it->first << dec
	 << "  " << setw(6) << it->second.line
	 << "    " << m_str_tab.index2str(it->second.file) << "\n";
  }
#endif
}


// Fill in LineRange object for one VMA.
//
void
LineMap::getLineRange(VMA vma, LineRange & lr)
{
  // using sentinels, we know both it and --it are real objects, not
  // map.begin() or end().
  auto it = m_line_map.upper_bound(vma);
  lr.end = it->first;
  --it;
  lr.start = it->first;
  lr.filenm = m_str_tab.index2str(it->second.file).c_str();
  lr.lineno = it->second.line;
}
