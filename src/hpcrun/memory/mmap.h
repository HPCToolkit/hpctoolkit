// SPDX-FileCopyrightText: 2011-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MMAP_H
#define MMAP_H

#include <stdlib.h>

void* hpcrun_mmap_anon(size_t size);
void hpcrun_mmap_init(void);

#endif // MMAP_H
