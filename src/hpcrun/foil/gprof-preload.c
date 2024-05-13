// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#define _GNU_SOURCE

#include "foil.h"

// stubs to override gprof support to avoid conflicts with hpcrun
// NOTE: avoids core dump on ppc64le

HPCRUN_EXPOSED void __monstartup() {}
HPCRUN_EXPOSED void _mcleanup() {}
HPCRUN_EXPOSED void mcount() {}
HPCRUN_EXPOSED void _mcount() {}
