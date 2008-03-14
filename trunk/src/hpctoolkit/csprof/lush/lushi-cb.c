// -*-Mode: C++;-*- // technically C99
// $Id$

//***************************************************************************
//
// File: 
//    $Source$
//
// Purpose:
//    LUSH Interface Callbacks
//
// Description:
//    [The set of functions, macros, etc. defined in the file]
//
// Author:
//    Nathan Tallent, Rice University.
//
//***************************************************************************

//************************* System Include Files ****************************

#include <stdlib.h>
#include <string.h>
#include <errno.h>

//*************************** User Include Files ****************************

#include "lushi-cb.h"

//*************************** Forward Declarations **************************


// **************************************************************************
// Interface for 'heap memory' allocation
// **************************************************************************

// **************************************************************************
// Facility for unwinding physical stack
// **************************************************************************

int
LUSHCB_get_loadmap(LUSHCB_epoch_t** epoch)
{
  *epoch = csprof_get_epoch();
  return 0;
}

