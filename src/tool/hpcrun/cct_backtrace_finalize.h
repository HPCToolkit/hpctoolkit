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
// Copyright ((c)) 2002-2022, Rice University
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

#ifndef __cct_backtrace_finalize_h__
#define __cct_backtrace_finalize_h__

#include "cct/cct.h"
#include "cct/cct_bundle.h"
#include "unwind/common/backtrace_info.h"

typedef void (*cct_backtrace_finalize_fn)(backtrace_info_t* bt, int isSync);

typedef struct cct_backtrace_finalize_entry_s {
  cct_backtrace_finalize_fn fn;
  struct cct_backtrace_finalize_entry_s* next;
} cct_backtrace_finalize_entry_t;

typedef cct_node_t* (*cct_cursor_finalize_fn)(
    cct_bundle_t* cct, backtrace_info_t* bt, cct_node_t* cursor);

extern void cct_backtrace_finalize_register(cct_backtrace_finalize_entry_t* e);

extern void cct_backtrace_finalize(backtrace_info_t* bt, int isSync);

extern void cct_cursor_finalize_register(cct_cursor_finalize_fn fn);

extern cct_node_t* cct_cursor_finalize(cct_bundle_t* cct, backtrace_info_t* bt, cct_node_t* cursor);

#endif
