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

// This file uses libdwarf directly to compute the exception handler
// (eh) frame addresses and adds them to the list of function addrs.
// Dyninst and libdw don't support this, so we use libdwarf directly.
//
// Note: new dyninst uses libdw, and libdwarf and libdw don't play
// together well (overlap of function names, header files).  So, we
// move this to its own file, compile it separately and access
// libdwarf via dlopen() and dlsym().
//
// Note: in order to isolate libdwarf as much as possible, for each
// file, we re-dlopen libdwarf.so, save the addrs in a set, dlclose
// libdwarf, and only then return the set of addrs (via a callback
// function).  This way, nothing in the rest of fnbounds runs while
// libdwarf is open.  We probably can relax this to open libdwarf.so
// just once.

//***************************************************************************

#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <boost/tokenizer.hpp>
#include <libdwarf.h>

#include <include/hpctoolkit-config.h>
#include "eh-frames.h"

#include <set>
#include <string>

using namespace std;

typedef set <void *> AddrSet;

static void dwarf_frame_info_help(int, AddrSet &);

#define EXT_LIBS_DIR_STR  "HPCTOOLKIT_EXT_LIBS_DIR"
#define LD_LIBRARY_PATH   "LD_LIBRARY_PATH"

#define LIBDWARF_NAME  "libdwarf.so"
#define DLOPEN_OPTS    (RTLD_NOW | RTLD_LOCAL)

#define KEEP_LIBDWARF_OPEN  0

//----------------------------------------------------------------------

#ifdef DYNINST_USE_LIBDW
//
// get libdwarf.so functions via dlopen() and dlsym()
//

typedef int dwarf_init_fcn_t
  (int, Dwarf_Unsigned, Dwarf_Handler, Dwarf_Ptr, Dwarf_Debug *, Dwarf_Error *);

typedef int dwarf_finish_fcn_t (Dwarf_Debug, Dwarf_Error *);

typedef void dwarf_dealloc_fcn_t (Dwarf_Debug, void *, Dwarf_Unsigned);

typedef int dwarf_get_fde_list_eh_fcn_t
  (Dwarf_Debug, Dwarf_Cie **, Dwarf_Signed *, Dwarf_Fde **, Dwarf_Signed *, Dwarf_Error *);

typedef int dwarf_get_fde_range_fcn_t
  (Dwarf_Fde, Dwarf_Addr *, Dwarf_Unsigned *, Dwarf_Ptr *, Dwarf_Unsigned *,
   Dwarf_Off *, Dwarf_Signed *, Dwarf_Off *, Dwarf_Error *);

typedef void dwarf_fde_cie_list_dealloc_fcn_t
  (Dwarf_Debug, Dwarf_Cie *, Dwarf_Signed, Dwarf_Fde *, Dwarf_Signed);

static dwarf_init_fcn_t * real_dwarf_init = NULL;
static dwarf_finish_fcn_t * real_dwarf_finish = NULL;
static dwarf_dealloc_fcn_t * real_dwarf_dealloc = NULL;
static dwarf_get_fde_list_eh_fcn_t * real_dwarf_get_fde_list_eh = NULL;
static dwarf_get_fde_range_fcn_t * real_dwarf_get_fde_range = NULL;
static dwarf_fde_cie_list_dealloc_fcn_t * real_dwarf_fde_cie_list_dealloc = NULL;

#define DWARF_INIT     (* real_dwarf_init)
#define DWARF_FINISH   (* real_dwarf_finish)
#define DWARF_DEALLOC  (* real_dwarf_dealloc)
#define DWARF_GET_FDE_LIST_EH  (* real_dwarf_get_fde_list_eh)
#define DWARF_GET_FDE_RANGE    (* real_dwarf_get_fde_range)
#define DWARF_FDE_CIE_LIST_DEALLOC  (* real_dwarf_fde_cie_list_dealloc)

static string library_file;
static void * libdwarf_handle = NULL;

static int found_library = 0;
static int dlopen_done = 0;
static int dlsym_done = 0;

#else
//
// direct function calls, no dlopen
//

#define DWARF_INIT     dwarf_init
#define DWARF_FINISH   dwarf_finish
#define DWARF_DEALLOC  dwarf_dealloc
#define DWARF_GET_FDE_LIST_EH  dwarf_get_fde_list_eh
#define DWARF_GET_FDE_RANGE    dwarf_get_fde_range
#define DWARF_FDE_CIE_LIST_DEALLOC  dwarf_fde_cie_list_dealloc

#endif

//----------------------------------------------------------------------

// In the libdw case, we use dlopen() and dlsym() to access the
// libdwarf functions.
//
// Returns: 0 on success, 1 on failure.
//
static int
get_libdwarf_functions(void)
{
#ifdef DYNINST_USE_LIBDW

  // step 1 -- find libdwarf.so file, this only happens once.
  if (! found_library) {
    //
    // try HPCTOOLKIT_EXT_LIBS_DIR first, this is set in the launch
    // script.
    //
    char *str = getenv(EXT_LIBS_DIR_STR);

    if (str != NULL) {
      library_file = string(str) + "/" + LIBDWARF_NAME;
      libdwarf_handle = dlopen(library_file.c_str(), DLOPEN_OPTS);

      if (libdwarf_handle != NULL) {
	found_library = 1;
	dlopen_done = 1;
      }
      else {
	warnx("unable to open %s in %s = %s", LIBDWARF_NAME, EXT_LIBS_DIR_STR, str);
      }
    }
  }

  if (! found_library) {
    //
    // try LD_LIBRARY_PATH, in case running the binary directly.  as
    // long as make install copied libdwarf.so to ext-libs, this
    // method pretty much has to work.
    //
    // boost::tokenizer and algorithm::split generate a lot of code
    // bloat.  we could rewrite this with string::find_first_of().
    //
    char *str = getenv(LD_LIBRARY_PATH);

    if (str != NULL) {
      string path = str;
      boost::char_separator <char> sep (":;", "", boost::drop_empty_tokens);
      boost::tokenizer <boost::char_separator <char>> token (path, sep);

      for (auto it = token.begin(); it != token.end(); ++it) {
	library_file = string(*it) + "/" + LIBDWARF_NAME;
	libdwarf_handle = dlopen(library_file.c_str(), DLOPEN_OPTS);

	if (libdwarf_handle != NULL) {
	  found_library = 1;
	  dlopen_done = 1;
	  break;
	}
      }
      if (! found_library) {
	warnx("unable to find %s in %s = %s", LIBDWARF_NAME, LD_LIBRARY_PATH, str);
      }
    }
  }

  if (! found_library) {
    return 1;
  }

  // step 2 -- dlopen()
  if (! dlopen_done) {
    libdwarf_handle = dlopen(library_file.c_str(), DLOPEN_OPTS);

    if (libdwarf_handle == NULL) {
      warnx("unable to open: %s", library_file.c_str());
      return 1;
    }

    dlopen_done = 1;
  }

  // step 3 -- dlsym() and check that the functions exist
  if (! dlsym_done) {
    real_dwarf_init = (dwarf_init_fcn_t *) dlsym(libdwarf_handle, "dwarf_init");
    real_dwarf_finish = (dwarf_finish_fcn_t *) dlsym(libdwarf_handle, "dwarf_finish");
    real_dwarf_dealloc = (dwarf_dealloc_fcn_t *) dlsym(libdwarf_handle, "dwarf_dealloc");
    real_dwarf_get_fde_list_eh = (dwarf_get_fde_list_eh_fcn_t *)
	dlsym(libdwarf_handle, "dwarf_get_fde_list_eh");
    real_dwarf_get_fde_range = (dwarf_get_fde_range_fcn_t *)
	dlsym(libdwarf_handle, "dwarf_get_fde_range");
    real_dwarf_fde_cie_list_dealloc = (dwarf_fde_cie_list_dealloc_fcn_t *)
	dlsym(libdwarf_handle, "dwarf_fde_cie_list_dealloc");

    if (real_dwarf_init == NULL
	|| real_dwarf_finish == NULL
	|| real_dwarf_dealloc == NULL
	|| real_dwarf_get_fde_list_eh == NULL
	|| real_dwarf_get_fde_range == NULL
	|| real_dwarf_fde_cie_list_dealloc == NULL)
    {
      warnx("dlsym(%s) failed", LIBDWARF_NAME);
      return 1;
    }

    dlsym_done = 1;
  }
#endif

  return 0;
}

// In the libdw case, dlclose() libdwarf after each file (for now).
//
static void
release_libdwarf_functions(void)
{
#if defined(DYNINST_USE_LIBDW) && ! KEEP_LIBDWARF_OPEN

  dlclose(libdwarf_handle);
  real_dwarf_init = NULL;
  real_dwarf_finish = NULL;

  dlopen_done = 0;
  dlsym_done = 0;

#endif
}

//----------------------------------------------------------------------

// Entry point from main.cpp.  We return the list of addresses via the
// add_frame_addr() callback function.
//
void
dwarf_eh_frame_info(int fd)
{
  static int open_failed = 0;

  if (fd < 0 || open_failed) {
    return;
  }

  if (get_libdwarf_functions() != 0) {
    warnx("unable to open %s", LIBDWARF_NAME);
    open_failed = 1;
    return;
  }

  AddrSet addrSet;
  dwarf_frame_info_help(fd, addrSet);

  release_libdwarf_functions();

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
