// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

/******************************************************************************
 * include files
 *****************************************************************************/

#define _GNU_SOURCE

#include "x86-decoder.h"



/******************************************************************************
 * global variables
 *****************************************************************************/
xed_control_t x86_decoder_settings;



/******************************************************************************
 * local variables
 *****************************************************************************/

static xed_state_t xed_machine_state =
#if defined (HOST_CPU_x86_64)
 { XED_MACHINE_MODE_LONG_64,
   XED_ADDRESS_WIDTH_64b };
#else
 { XED_MACHINE_MODE_LONG_COMPAT_32,
   XED_ADDRESS_WIDTH_32b };
#endif


/******************************************************************************
 * interface operations
 *****************************************************************************/

void x86_family_decoder_init()
{
  x86_decoder_settings.xed_settings = xed_machine_state;

  xed_tables_init();
}
