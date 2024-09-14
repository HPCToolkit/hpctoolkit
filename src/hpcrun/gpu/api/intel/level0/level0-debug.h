// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef level0_debug_h
#define level0_debug_h

//*****************************************************************************
// level zero includes
//*****************************************************************************

#include <ze_api.h>



//*****************************************************************************
// interface functions
//*****************************************************************************

const char *
ze_result_to_string
(
  ze_result_t result
);

#endif
