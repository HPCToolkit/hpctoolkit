// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_kernel_table_h
#define gpu_kernel_table_h

#include "../../../utilities/ip-normalized.h"
#include "../../../logical/common.h"

//******************************************************************************
// interface operations
//******************************************************************************

/// Initialize the GPU kernel names string table.
/// Idempotent but not thread-safe.
void gpu_kernel_table_init();

/// Fetch the unique normalized IP for a kernel with the given `name`.
///
/// \param kernel_name Name of the kernel to record.
/// \param mangling Mangling used for the `kernel_name`.
/// \returns A unique normalized IP for the given kernel.
ip_normalized_t gpu_kernel_table_get(const char* kernel_name,
    enum logical_mangling mangling);

#endif
