// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_OMPT_PRIVATE_H
#define HPCRUN_FOIL_OMPT_PRIVATE_H

#ifdef __cplusplus
#error This is a C-only header
#endif

#include "../ompt/omp-tools.h"

typedef const struct hpcrun_foil_HooksDispatchOmpt_s* hpcrun_foil_HooksDispatchOmpt;

struct hpcrun_foil_hookdispatch_ompt {
  ompt_start_tool_result_t* (*ompt_start_tool)(unsigned int omp_version,
                                               const char* runtime_version);
  void (*mp_init)();
};

#endif // HPCRUN_FOIL_OMPT_PRIVATE_H
