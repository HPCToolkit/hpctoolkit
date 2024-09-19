// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#define _GNU_SOURCE

#include "../monitor-exts/openmp.h"
#include "../ompt/ompt-interface.h"
#include "common.h"
#include "ompt-private.h"

const struct hpcrun_foil_hookdispatch_ompt* hpcrun_foil_fetch_hooks_ompt() {
  static const struct hpcrun_foil_hookdispatch_ompt hooks = {
      .ompt_start_tool = hpcrun_ompt_start_tool,
      .mp_init = hpcrun_mp_init,
  };
  return &hooks;
}
