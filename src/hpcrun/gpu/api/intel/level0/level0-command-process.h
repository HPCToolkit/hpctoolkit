// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef level0_command_process_h
#define level0_command_process_h

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stdint.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include "level0-data-node.h"

//*****************************************************************************
// interface operations
//*****************************************************************************

void
level0_command_begin
(
  level0_data_node_t* command_node
);

void
level0_command_end
(
  level0_data_node_t* command_node,
  uint64_t start,
  uint64_t end
);

void
level0_flush_and_wait
(
  void
);

void
level0_wait_for_self_pending_operations
(
  void
);

#endif
