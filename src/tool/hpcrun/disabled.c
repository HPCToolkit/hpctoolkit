// -*-Mode: C++;-*- // technically C99
// $Id$

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

#include "disabled.h"



//*****************************************************************************
// global variables
//*****************************************************************************

static bool hpcrun_is_disabled = false;


//*****************************************************************************
// interface operations
//*****************************************************************************

bool 
hpcrun_get_disabled()
{
  return hpcrun_is_disabled;
}


void 
hpcrun_set_disabled()
{
  hpcrun_is_disabled = true;
}
