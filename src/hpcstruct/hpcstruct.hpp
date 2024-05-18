// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef hpcstruct_hpp
#define hpcstruct_hpp


#include "Args.hpp"
#include "Structure-Cache.hpp"

void doSingleBinary( Args &args, struct stat *sb);
void doMeasurementsDir ( Args &args, struct stat *sb);

#endif //hpcstruct_hpp
