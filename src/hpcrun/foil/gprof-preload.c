// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#include "common.h"

// stubs to override gprof support to avoid conflicts with hpcrun
// NOTE: avoids core dump on ppc64le

HPCRUN_EXPOSED_API void __monstartup() {}

HPCRUN_EXPOSED_API void _mcleanup() {}

HPCRUN_EXPOSED_API void mcount() {}

HPCRUN_EXPOSED_API void _mcount() {}
