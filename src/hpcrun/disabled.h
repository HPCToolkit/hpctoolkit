// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef disabled_h
#define disabled_h

//*****************************************************************************
// File: disabled.h
//
// Description:
//   interface to support disabling hpcrun and checking for disabled status.
//
// History:
//   19 July 2009 - John Mellor-Crummey - created
//
//*****************************************************************************



//*****************************************************************************
// global includes
//*****************************************************************************

#include <stdbool.h>



//*****************************************************************************
// interface operations
//*****************************************************************************

bool hpcrun_get_disabled();

void hpcrun_set_disabled();



#endif  // ! disabled_h
