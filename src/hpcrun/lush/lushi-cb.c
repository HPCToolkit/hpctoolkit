// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//***************************************************************************
//
// File:
//   $HeadURL$
//
// Purpose:
//   LUSH Interface Callbacks
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   Nathan Tallent, Rice University.
//
//***************************************************************************

//************************* System Include Files ****************************

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <errno.h>

//*************************** User Include Files ****************************

#include "lushi-cb.h"

#include "../messages/messages.h"

//*************************** Forward Declarations **************************


// **************************************************************************
// Interface for 'heap memory' allocation
// **************************************************************************

// **************************************************************************
// Facility for unwinding physical stack
// **************************************************************************

int
LUSHCB_loadmap_find(void* addr,
                    char* module_name,
                    void** start,
                    void** end)
{
  hpcrun_terminate();
}
