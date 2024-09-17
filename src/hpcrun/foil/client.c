// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "../start-stop.h"
#include "client-private.h"
#include "common.h"

const struct hpcrun_foil_hookdispatch_client* hpcrun_foil_fetch_hooks_client() {
  static const struct hpcrun_foil_hookdispatch_client hooks = {
      .sampling_is_active = &hpcrun_sampling_is_active,
      .sampling_start = &hpcrun_sampling_start,
      .sampling_stop = &hpcrun_sampling_stop,
  };
  return &hooks;
}
