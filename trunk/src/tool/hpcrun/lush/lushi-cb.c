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
#include "pmsg.h"

#include "dylib.h"

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
  int r = 0;
#ifdef HPCRUN_STATIC_LINK
  EMSG("LUSHCB_loadmap_find: HPCRUN_STATIC_LINK not implemented");
#else
  r = dylib_find_module_containing_addr(addr, module_name, start, end);
#endif
  return r;
}

