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

// This file uses libdwarf directly to compute the exception handler
// (eh) frame addresses and adds them to the list of function addrs.
// Dyninst and libdw don't support this, so we use libdwarf directly.
//
// Note: new dyninst uses libdw, and libdwarf and libdw don't play
// together well (overlap of function names, header files).  So, we
// move this to its own file, compile it separately and access
// libdwarf via dlopen() and dlsym().

//***************************************************************************

#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libdwarf.h>
#include "eh-frames.h"

#include <set>

typedef std::set <void *> AddrSet;

static void dwarf_frame_info_help(int, AddrSet &);

#define DWARF_INIT    dwarf_init
#define DWARF_FINISH  dwarf_finish
#define DWARF_DEALLOC  dwarf_dealloc
#define DWARF_GET_FDE_LIST_EH  dwarf_get_fde_list_eh
#define DWARF_GET_FDE_RANGE    dwarf_get_fde_range
#define DWARF_FDE_CIE_LIST_DEALLOC  dwarf_fde_cie_list_dealloc

//----------------------------------------------------------------------

void
dwarf_eh_frame_info(int fd)
{
  if (fd < 0) {
    return;
  }

  AddrSet addrSet;

  dwarf_frame_info_help(fd, addrSet);

  for (auto it = addrSet.begin(); it != addrSet.end(); ++it) {
    add_frame_addr(*it);
  }
}

//----------------------------------------------------------------------

// Read the eh frame info (exception handler) from the FDE records
// (frame description entry) and put the list of addresses (low_pc)
// into addrSet.
//
static void
dwarf_frame_info_help(int fd, AddrSet & addrSet)
{
  Dwarf_Debug dbg;
  Dwarf_Error err;
  int ret;

  ret = DWARF_INIT (fd, DW_DLC_READ, NULL, NULL, &dbg, &err);
  if (ret == DW_DLV_ERROR) { DWARF_DEALLOC (dbg, err, DW_DLA_ERROR); }

  if (ret != DW_DLV_OK) {
    return;
  }

  Dwarf_Cie * cie_data = NULL;
  Dwarf_Fde * fde_data = NULL;
  Dwarf_Signed cie_count = 0;
  Dwarf_Signed fde_count = 0;

  ret = DWARF_GET_FDE_LIST_EH (dbg, &cie_data, &cie_count, &fde_data, &fde_count, &err);
  if (ret == DW_DLV_ERROR) { DWARF_DEALLOC (dbg, err, DW_DLA_ERROR); }

  if (ret == DW_DLV_OK ) {
    for (long i = 0; i < fde_count; i++) {
      Dwarf_Addr low_pc = 0;

      ret = DWARF_GET_FDE_RANGE (fde_data[i], &low_pc, NULL, NULL, NULL,
				 NULL, NULL, NULL, &err);
      if (ret == DW_DLV_ERROR) { DWARF_DEALLOC (dbg, err, DW_DLA_ERROR); }

      if (ret == DW_DLV_OK && low_pc != 0) {
	addrSet.insert((void *) low_pc);
      }
    }

    DWARF_FDE_CIE_LIST_DEALLOC (dbg, cie_data, cie_count, fde_data, fde_count);
  }

  ret = DWARF_FINISH (dbg, &err);
  if (ret == DW_DLV_ERROR) { DWARF_DEALLOC (dbg, err, DW_DLA_ERROR); }
}
