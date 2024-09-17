// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_FOIL_PRELOAD_H
#define HPCRUN_FOIL_FOIL_PRELOAD_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Fetch the given symbol from the calling namespace.
/// Roughly identical to `dlsym(RTLD_NEXT, symbol)` but uses dl_iterate_phdr to find
/// symbols that are not directly accessible.
/// @returns The symbol if found, otherwise `NULL`.
void* foil_dlsym(const char* symbol);

/// Fetch the given symbol from the calling namespace with the given version.
/// Roughly identical to `dlvsym(RTLD_NEXT, symbol, version)` but falls back to
/// ::foil_dlsym on error.
/// @returns The symbol if found, otherwise `NULL`.
void* foil_dlvsym(const char* restrict symbol, const char* restrict version);

/// @name Hook dispatch tables for LD_PRELOAD foils
///
/// LD_PRELOAD foils don't link against the core foil to avoid exposing the internal
/// symbols to the entire application namespace. Instead, they use these functions,
/// which internally use dlopen and dlsym to access the symbols without exposing them
/// completely.
///
/// @{

/// Fetch the dispatch-table of hooks for the client foil without symbol exposure.
/// See ::hpcrun_foil_fetch_hooks_client.
const struct hpcrun_foil_hookdispatch_client* hpcrun_foil_fetch_hooks_client_dl();

/// Fetch the dispatch-table of hooks for the libc foil(s) without symbol exposure.
/// See ::hpcrun_foil_fetch_hooks_libc.
const struct hpcrun_foil_hookdispatch_libc* hpcrun_foil_fetch_hooks_libc_dl();

/// Fetch the dispatch-table of hooks for the GA foil without symbol exposure.
/// See ::hpcrun_foil_fetch_hooks_ga.
const struct hpcrun_foil_hookdispatch_ga* hpcrun_foil_fetch_hooks_ga_dl();

/// Fetch the dispatch-table of hooks for the MPI foil without symbol exposure.
/// See ::hpcrun_foil_fetch_hooks_mpi.
const struct hpcrun_foil_hookdispatch_mpi* hpcrun_foil_fetch_hooks_mpi_dl();

/// Fetch the dispatch-table of hooks for the OpenMP Tooling foil without symbol
/// exposure. See ::hpcrun_foil_fetch_hooks_ompt.
const struct hpcrun_foil_hookdispatch_ompt* hpcrun_foil_fetch_hooks_ompt_dl();

/// Fetch the dispatch-table of hooks for the OpenCL foil without symbol exposure.
/// See ::hpcrun_foil_fetch_hooks_opencl.
const struct hpcrun_foil_hookdispatch_opencl* hpcrun_foil_fetch_hooks_opencl_dl();

/// Fetch the dispatch-table of hooks for the Level Zero foil without symbol exposure.
/// See ::hpcrun_foil_fetch_hooks_level0.
const struct hpcrun_foil_hookdispatch_level0* hpcrun_foil_fetch_hooks_level0_dl();

/// @}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // HPCRUN_FOIL_FOIL_PRELOAD_H
