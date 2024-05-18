// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef WRITE_DATA_H
#define WRITE_DATA_H

#include "epoch.h"
#include "core_profile_trace_data.h"


extern int hpcrun_write_profile_data(core_profile_trace_data_t * cptd);
extern void hpcrun_flush_epochs(core_profile_trace_data_t * cptd);

#endif // WRITE_DATA_H
