// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef HPCTOOLKIT_LEVEL0_H
#define HPCTOOLKIT_LEVEL0_H

//******************************************************************************
// forward type declarations
//******************************************************************************

typedef struct gpu_activity_t gpu_activity_t;
typedef struct cct_node_t cct_node_t;

//******************************************************************************
// interface operations
//******************************************************************************

void level0_init();
void level0_fini();
void level0_flush();
int level0_bind();

#endif //HPCTOOLKIT_LEVEL0_H
