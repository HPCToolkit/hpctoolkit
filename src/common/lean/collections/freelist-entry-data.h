// SPDX-FileCopyrightText: 2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef FREELIST_ENTRY_DATA_H
#define FREELIST_ENTRY_DATA_H

#define FREELIST_ENTRY_DATA(ENTRY_TYPE)     \
struct {                                    \
  ENTRY_TYPE *next;                         \
}

#endif
