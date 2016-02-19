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

#ifndef UNWIND_CURSOR_H
#define UNWIND_CURSOR_H

//************************* System Include Files ****************************

#include <inttypes.h>

//*************************** User Include Files ****************************

#include "unwind-cfg.h"
#include <unwind/common/fence_enum.h>
#include <utilities/ip-normalized.h>

//*************************** Forward Declarations **************************

// HPC_UNW_LITE: It is not safe to have a pointer to the interval
// since we cannot use dynamic storage.
#if (HPC_UNW_LITE)

   // there should probably have a check to ensure this is big enough
   typedef struct { char data[128]; } unw_interval_opaque_t;
#  define UNW_CURSOR_INTERVAL_t unw_interval_opaque_t

#else

#  include "splay-interval.h"
#  include <hpcrun/utilities/ip-normalized.h>
#  define UNW_CURSOR_INTERVAL_t splay_interval_t*

#endif

//***************************************************************************

typedef struct hpcrun_unw_cursor_t {

  // ------------------------------------------------------------
  // common state
  // ------------------------------------------------------------
  void *pc_unnorm; //only place where un-normalized pc will exist
  void **bp;
  void **sp;
  void *ra;

  void *ra_loc;  // for trampolines

  fence_enum_t fence; // Details on which fence stopped an unwind

  UNW_CURSOR_INTERVAL_t intvl;

  ip_normalized_t the_function; // (normalized) ip for function

  //NOTE: will fail if HPC_UWN_LITE defined
  ip_normalized_t pc_norm;

  // ------------------------------------------------------------
  // unwind-provider-specific state
  // ------------------------------------------------------------
  int32_t flags;

} hpcrun_unw_cursor_t;


//***************************************************************************

#endif
