// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//*****************************************************************************
// File: disabled.c
//
// Description:
//   interface to support disabling hpcrun and checking for disabled status.
//
// History:
//   19 July 2009 - John Mellor-Crummey - created
//
//*****************************************************************************



//*****************************************************************************
// local includes
//*****************************************************************************

#define _GNU_SOURCE

#include "disabled.h"



//*****************************************************************************
// global variables
//*****************************************************************************

static bool hpcrun_is_disabled = false;


//*****************************************************************************
// interface operations
//*****************************************************************************

bool
hpcrun_get_disabled(void)
{
  return hpcrun_is_disabled;
}


void
hpcrun_set_disabled(void)
{
  hpcrun_is_disabled = true;
}
