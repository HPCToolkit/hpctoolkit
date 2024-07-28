// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*-

// fnbounds.h - program to print the function list from its load-object arguments

#ifndef _FNBOUNDS_FNBOUNDS_H_
#define _FNBOUNDS_FNBOUNDS_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Local typedefs
typedef struct FnboundsResponse {
  void** entries;
  size_t num_entries;  // size
  size_t max_entries;  // allocation size
  bool is_relocatable;
  uint64_t reference_offset;
} FnboundsResponse;

// prototypes
FnboundsResponse fnb_get_funclist(const char *);
void    fnb_add_function(FnboundsResponse*, void*);

#endif  // _FNBOUNDS_FNBOUNDS_H_
