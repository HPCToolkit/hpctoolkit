// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef HPCRUN_FNBOUNDS_FNBOUNDS_INTERFACE_H
#define HPCRUN_FNBOUNDS_FNBOUNDS_INTERFACE_H

//*********************************************************************
// local includes
//*********************************************************************

#include "../loadmap.h"

//*********************************************************************

int
fnbounds_init(const char *executable_name);

// fnbounds_enclosing_addr(): Given an instruction pointer (IP) 'ip',
// return the bounds [start, end) of the function that contains 'ip'.
// Also return the load module that contains 'ip' to make
// normalization easy.  All IPs are *unnormalized.*
bool
fnbounds_enclosing_addr(void *ip, void **start, void **end, load_module_t **lm);

load_module_t*
fnbounds_map_dso(const char *module_name, void *start, void *end, struct dl_phdr_info*);

void
fnbounds_fini();

void
fnbounds_release_lock(void);


// fnbounds_table_lookup(): Given an instruction pointer (IP) 'ip',
// return the bounds [start, end) of the function that contains 'ip'.
// All IPs are *normalized.*
int
fnbounds_table_lookup(void **table, int length, void *ip,
                      void **start, void **end);


#include "fnbounds_table_interface.h"

#endif  // HPCRUN_FNBOUNDS_FNBOUNDS_INTERFACE_H
