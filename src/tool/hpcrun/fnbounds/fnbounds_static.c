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
// Copyright ((c)) 2002-2016, Rice University
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

//*********************************************************************
// system includes
//*********************************************************************

#include <assert.h>

//*********************************************************************
// local includes
//*********************************************************************

#include "fnbounds_interface.h"
#include "fnbounds_file_header.h"

#include <loadmap.h>
#include <files.h>

//*********************************************************************

//-------------------------------------------------------------------------
// the external variables below will be defined in a 
// machine-generated file
//-------------------------------------------------------------------------

// TODO:tallent: abstract with hpcfnbounds (cf. dump_file_symbols())
// and fnbounds_read_nm_file()
extern unsigned long hpcrun_nm_addrs[];
extern unsigned long hpcrun_nm_addrs_len;
extern unsigned long hpcrun_reference_offset;
extern int           hpcrun_is_relocatable;

int
fnbounds_init()
{
  void *lm_beg_fn = (void*)hpcrun_nm_addrs[0];
  void *lm_end_fn = (void*)hpcrun_nm_addrs[hpcrun_nm_addrs_len - 1];
  long lm_size = lm_end_fn - lm_beg_fn;

  struct fnbounds_file_header fh;
  fh.num_entries = hpcrun_nm_addrs_len;
  fh.reference_offset = hpcrun_reference_offset;
  fh.is_relocatable = hpcrun_is_relocatable;
  fh.mmap_size = 0;

  dso_info_t *dso =
    hpcrun_dso_make(hpcrun_files_executable_pathname(), (void*)hpcrun_nm_addrs, 
		    &fh, lm_beg_fn, lm_end_fn, lm_size);
  hpcrun_loadmap_map(dso);

  return 0;
}


fnbounds_table_t
fnbounds_fetch_executable_table(void)
{
  return (fnbounds_table_t)
    { .table = (void**) hpcrun_nm_addrs, .len = hpcrun_nm_addrs_len};
}


int
fnbounds_query(void *pc)
{
  assert(0);
  return 0;
}


int
fnbounds_add(char *module_name, void *start, void *end)
{
  assert(0);
  return 0;
}


bool
fnbounds_enclosing_addr(void *ip, void **start, void **end, load_module_t **lm)
{
  load_module_t* lm_ = hpcrun_getLoadmap()->lm_head;
  int ret = fnbounds_table_lookup((void*)hpcrun_nm_addrs, hpcrun_nm_addrs_len,
				  ip, start, end);
  if (lm) {
    *lm = lm_;
  }
  return (ret == 0);
}


void 
fnbounds_fini()
{
}


void
fnbounds_release_lock(void)
{
}
