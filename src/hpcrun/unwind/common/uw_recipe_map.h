// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

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
