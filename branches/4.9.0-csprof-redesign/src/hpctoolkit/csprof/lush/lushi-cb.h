// -*-Mode: C++;-*- // technically C99
// $Id$

//***************************************************************************
//
// File: 
//    $Source$
//
// Purpose:
//    LUSH Interface: Callback Interface for LUSH agents
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

//*************************** Forward Declarations **************************

// **************************************************************************
// A LUSH agent expects the following callbacks:
// **************************************************************************

// ---------------------------------------------------------
// Interface for 'heap memory' allocation
// ---------------------------------------------------------

void* LUSHCB_malloc(size_t size);
void  LUSHCB_free();


// ---------------------------------------------------------
// Facility for unwinding physical stack
// ---------------------------------------------------------

struct LUSHCB_context_t; // a la libunwind
struct LUSHCB_cursor;    // a la libunwind
int LUSHCB_step();       // a la libunwind

int LUSHCB_get_loadmap();


// **************************************************************************
