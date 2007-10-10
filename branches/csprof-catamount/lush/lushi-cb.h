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

#ifndef lush_lush_cb_h
#define lush_lush_cb_h

//************************* System Include Files ****************************

#include <stdlib.h>

//*************************** User Include Files ****************************

#include "lush.i"

#include "epoch.h" // for csprof_epoch_t


//*************************** Forward Declarations **************************

// **************************************************************************
// A LUSH agent expects the following callbacks:
// **************************************************************************

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------
// Interface for 'heap memory' allocation
// ---------------------------------------------------------

LUSHI_DECL(void*, LUSHCB_malloc, (size_t size));
LUSHI_DECL(void,  LUSHCB_free, ());

// ---------------------------------------------------------
// Facility for unwinding physical stack
// ---------------------------------------------------------

typedef unw_cursor_t LUSHCB_cursor_t;

LUSHI_DECL(int, LUSHCB_step, (LUSHCB_cursor_t cursor));


typedef csprof_epoch_t LUSHCB_epoch_t;

LUSHI_DECL(int, LUSHCB_get_loadmap, (LUSHCB_epoch_t** epoch));


#ifdef __cplusplus
}
#endif

// **************************************************************************

#endif /* lush_lush_cb_h */
