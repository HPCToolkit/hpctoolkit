// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_FOIL_HOOK_H
#define HPCRUN_FOIL_FOIL_HOOK_H

#ifdef __cplusplus
extern "C" {
#endif

/// Marker macro for symbols exposed to the application namespace.
/// Note that functions that are exposed via function pointers don't need this marker,
/// only functions exposed by their symbols do.
#define HPCRUN_EXPOSED_API __attribute__((visibility("default"))) extern

/// @name Application dispatch tables
///
/// A word on the design here: communication between the foil and the rest of hpcrun
/// happens through "dispatch tables," a general term for a `struct` of function
/// pointers (similar to a C++ virtual table but for C). The actual type of a dispatch
/// table is an implementation detail of the foil itself and thus can make use of
/// features unique to the language (e.g. C++ `virtual`).
///
/// These dispatch tables expose symbols the application is using, e.g. a particular
/// OpenCL implementation. Generally these tables are only provided as the argument to a
/// foil "hook," foils without hooks often keep the table pointer as an internal
/// implementation detail.
///
/// @{

// These are the forward declarations for the opaque types defined by the foil
// implementations.
struct hpcrun_foil_appdispatch_ga;
struct hpcrun_foil_appdispatch_level0;
struct hpcrun_foil_appdispatch_libc;
struct hpcrun_foil_appdispatch_libc_io;
struct hpcrun_foil_appdispatch_libc_alloc;
struct hpcrun_foil_appdispatch_libc_sync;
struct hpcrun_foil_appdispatch_mpi;
struct hpcrun_foil_appdispatch_nvidia;
struct hpcrun_foil_appdispatch_opencl;
struct hpcrun_foil_appdispatch_rocm_hip;
struct hpcrun_foil_appdispatch_rocm_hsa;
struct hpcrun_foil_appdispatch_rocprofiler;
struct hpcrun_foil_appdispatch_roctracer;
struct hpcrun_foil_appdispatch_gtpin;

/// @}

/// @name Hook dispatch tables
///
/// See Dispatch tables above for some design points. These dispatch tables implement
/// "hooks," i.e. they allow foils to call into hpcrun.
///
/// @{

// These are the forward declarations for the opaque types defined by the foil
// implementations.
struct hpcrun_foil_hookdispatch_client;
struct hpcrun_foil_hookdispatch_libc;
struct hpcrun_foil_hookdispatch_ga;
struct hpcrun_foil_hookdispatch_mpi;
struct hpcrun_foil_hookdispatch_ompt;
struct hpcrun_foil_hookdispatch_opencl;
struct hpcrun_foil_hookdispatch_rocprofiler;
struct hpcrun_foil_hookdispatch_level0;

/// Fetch the dispatch-table of hooks for the client foil.
/// Thread-safe but not async-signal-safe.
/// @returns A pointer to the corresponding dispatch-table in the main hpcrun namespace.
HPCRUN_EXPOSED_API
const struct hpcrun_foil_hookdispatch_client* hpcrun_foil_fetch_hooks_client();

/// Fetch the dispatch-table of hooks for the libc foil(s).
/// Thread-safe but not async-signal-safe.
/// @returns A pointer to the corresponding dispatch-table in the main hpcrun namespace.
HPCRUN_EXPOSED_API
const struct hpcrun_foil_hookdispatch_libc* hpcrun_foil_fetch_hooks_libc();

/// Fetch the dispatch-table of hooks for the GA foil.
/// Thread-safe but not async-signal-safe.
/// @returns A pointer to the corresponding dispatch-table in the main hpcrun namespace.
HPCRUN_EXPOSED_API
const struct hpcrun_foil_hookdispatch_ga* hpcrun_foil_fetch_hooks_ga();

/// Fetch the dispatch-table of hooks for the MPI foil.
/// Thread-safe but not async-signal-safe.
/// @returns A pointer to the corresponding dispatch-table in the main hpcrun namespace.
HPCRUN_EXPOSED_API
const struct hpcrun_foil_hookdispatch_mpi* hpcrun_foil_fetch_hooks_mpi();

/// Fetch the dispatch-table of hooks for the OpenMP Tooling foil.
/// Thread-safe but not async-signal-safe.
/// @returns A pointer to the corresponding dispatch-table in the main hpcrun namespace.
HPCRUN_EXPOSED_API
const struct hpcrun_foil_hookdispatch_ompt* hpcrun_foil_fetch_hooks_ompt();

/// Fetch the dispatch-table of hooks for the OpenCL foil.
/// Thread-safe but not async-signal-safe.
/// @returns A pointer to the corresponding dispatch-table in the main hpcrun namespace.
HPCRUN_EXPOSED_API
const struct hpcrun_foil_hookdispatch_opencl* hpcrun_foil_fetch_hooks_opencl();

/// Fetch the dispatch-table of hooks for the ROCProfiler foil.
/// Thread-safe but not async-signal-safe.
/// @returns A pointer to the corresponding dispatch-table in the main hpcrun namespace.
HPCRUN_EXPOSED_API
const struct hpcrun_foil_hookdispatch_rocprofiler*
hpcrun_foil_fetch_hooks_rocprofiler();

/// Fetch the dispatch-table of hooks for the Level Zero foil.
/// Thread-safe but not async-signal-safe.
/// @returns A pointer to the corresponding dispatch-table in the main hpcrun namespace.
HPCRUN_EXPOSED_API
const struct hpcrun_foil_hookdispatch_level0* hpcrun_foil_fetch_hooks_level0();

/// @}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // HPCRUN_FOIL_FOIL_HOOK_H
