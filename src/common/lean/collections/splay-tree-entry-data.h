// SPDX-FileCopyrightText: 2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef GENERIC_SPLAY_TREE_ENTRY_DATA_H
#define GENERIC_SPLAY_TREE_ENTRY_DATA_H 1

#define SPLAY_TREE_ENTRY_DATA(ENTRY_TYPE)       \
struct {                                        \
  ENTRY_TYPE *left;                             \
  ENTRY_TYPE *right;                            \
}

#endif
