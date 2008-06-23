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
  int r = dylib_find_module_containing_addr((unsigned long long)addr, 
					    module_name, 
					    (unsigned long long *)start, 
					    (unsigned long long *)end);
  return r;
}

