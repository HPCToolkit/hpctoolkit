// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once
#ifndef HPCRUN_FOIL_GTPIN_PRIVATE_H
#define HPCRUN_FOIL_GTPIN_PRIVATE_H

#include "common.h"

#include <api/gtpin_api.h>

struct hpcrun_foil_appdispatch_gtpin {
  gtpin::IGtCore* (*GTPin_GetCore)() = &gtpin::GTPin_GetCore;
};

extern "C" {
HPCRUN_EXPOSED_API const hpcrun_foil_appdispatch_gtpin hpcrun_dispatch_gtpin;
}

#endif // HPCRUN_FOIL_GTPIN_PRIVATE_H
