// SPDX-FileCopyrightText: 2012-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef FNBOUNDS_TABLE_IFACE
#define FNBOUNDS_TABLE_IFACE
#include <stddef.h>

typedef struct fnbounds_table_t fnbounds_table_t;

struct fnbounds_table_t {
  void** table;
  size_t len;
};

extern fnbounds_table_t fnbounds_fetch_executable_table(void);

#endif // FNBOUNDS_TABLE_IFACE
