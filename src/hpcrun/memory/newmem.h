// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//
// The new memory allocator's internal data.  This should be used ONLY
// inside the memory allocator and in the thread data struct.
//
// $Id$
//

#ifndef _HPCRUN_NEWMEM_H_
#define _HPCRUN_NEWMEM_H_

struct hpcrun_meminfo {
  void *mi_start;
  void *mi_low;
  void *mi_high;
  long  mi_size;
};

typedef struct hpcrun_meminfo hpcrun_meminfo_t;

void hpcrun_make_memstore(hpcrun_meminfo_t *mi);
int get_mem_low(void);

#endif
