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
// Copyright ((c)) 2002-2020, Rice University
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

/*
 * Interface to unwind recipe map.
 *
 */

#ifndef _UW_RECIPE_MAP_H_
#define _UW_RECIPE_MAP_H_

#include "unwindr_info.h"

typedef struct ilmstat_btuwi_pair_s ilmstat_btuwi_pair_t;

void
uw_recipe_map_init(void);


/*
 * if addr is found in range in the map, return true and
 *   *unwr_info is the ilmstat_btuwi_pair_t ( ([start, end), ldmod, status), btuwi ),
 *   where the root of btuwi is the uwi_t for addr
 * else return false
 * 
 * NOTE: if using on-the-fly binary analysis, this attempts to build recipes for the 
 *       procedure enclosing addr
 */
bool
uw_recipe_map_lookup(void *addr, unwinder_t uw, unwindr_info_t *unwr_info);


bool
uw_recipe_map_lookup_noinsert(void *addr, unwinder_t uw, unwindr_info_t *unwr_info);

#endif  /* !_UW_RECIPE_MAP_H_ */
