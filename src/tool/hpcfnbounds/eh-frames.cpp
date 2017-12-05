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

#define DWARF_OK(e) (DW_DLV_OK == (e))

using namespace std;

static int verbose = 0;

//----------------------------------------------------------------------

// collect function start addresses from eh_frame info
// (part of the DWARF info)
// enter these start addresses into the reachable function
// data structure

void
seed_dwarf_info(int dwarf_fd)
{
  Dwarf_Debug dbg = NULL;
  Dwarf_Error err;
  Dwarf_Handler errhand = NULL;
  Dwarf_Ptr errarg = NULL;

  // Unless disabled, add eh_frame info to function entries
  if(getenv("EH_NO")) {
    close(dwarf_fd);
    return;
  }

  if ( ! DWARF_OK(dwarf_init(dwarf_fd, DW_DLC_READ,
                             errhand, errarg,
                             &dbg, &err))) {
    if (verbose) fprintf(stderr, "dwarf init failed !!\n");
    return;
  }

  Dwarf_Cie* cie_data = NULL;
  Dwarf_Signed cie_element_count = 0;
  Dwarf_Fde* fde_data = NULL;
  Dwarf_Signed fde_element_count = 0;

  int fres =
    dwarf_get_fde_list_eh(dbg, &cie_data,
                          &cie_element_count, &fde_data,
                          &fde_element_count, &err);
  if ( ! DWARF_OK(fres)) {
    if (verbose) fprintf(stderr, "failed to get eh_frame element from DWARF\n");
    return;
  }

  for (int i = 0; i < fde_element_count; i++) {
    Dwarf_Addr low_pc = 0;
    Dwarf_Unsigned func_length = 0;
    Dwarf_Ptr fde_bytes = NULL;
    Dwarf_Unsigned fde_bytes_length = 0;
    Dwarf_Off cie_offset = 0;
    Dwarf_Signed cie_index = 0;
    Dwarf_Off fde_offset = 0;

    int fres = dwarf_get_fde_range(fde_data[i],
                                   &low_pc, &func_length,
                                   &fde_bytes,
                                   &fde_bytes_length,
                                   &cie_offset, &cie_index,
                                   &fde_offset, &err);
    if (fres == DW_DLV_ERROR) {
      fprintf(stderr, " error on dwarf_get_fde_range\n");
      return;
    }
    if (fres == DW_DLV_NO_ENTRY) {
      fprintf(stderr, " NO_ENTRY error on dwarf_get_fde_range\n");
      return;
    }
    if(getenv("EH_SHOW")) {
      fprintf(stderr, " ---potential fn start = %p\n", reinterpret_cast<void*>(low_pc));
    }

    if (low_pc != (Dwarf_Addr) 0) {
      add_frame_addr((void *) low_pc);
    }
  }
  if ( ! DWARF_OK(dwarf_finish(dbg, &err))) {
    fprintf(stderr, "dwarf finish fails ???\n");
  }
  close(dwarf_fd);
}
