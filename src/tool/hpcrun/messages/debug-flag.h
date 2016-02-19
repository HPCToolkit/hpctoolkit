// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

#ifndef debug_flag_h
#define debug_flag_h

//*****************************************************************************
// File: debug-flag.c
//
// Description:
//   debug flags management for hpcrun
//
// History:
//   23 July 2009 - John Mellor-Crummey
//     created by splitting off from messages.h, messages-sync.c
//
//*****************************************************************************


//*****************************************************************************
// macros
//*****************************************************************************

#define DBG_PREFIX(s) DBG_##s
#define CTL_PREFIX(s) CTL_##s

#define DBG(f)            debug_flag_get(DBG_PREFIX(f))
#define SET(f,v)          debug_flag_set(DBG_PREFIX(f), v)

#define ENABLE(f)         SET(f,1)
#define DISABLE(f)        SET(f,0)

#define ENABLED(f)        DBG(f)
#define DISABLED(f)       (! DBG(f))

#define IF_ENABLED(f)     if ( ENABLED(f) )
#define IF_DISABLED(f)    if ( ! ENABLED(f) )



//*****************************************************************************
// type declarations
//*****************************************************************************


typedef enum {

#undef E
#define E(s) DBG_PREFIX(s)

#include "messages.flag-defns"

#undef E

} pmsg_category;

typedef pmsg_category dbg_category;

//*****************************************************************************
// forward declarations
//*****************************************************************************

void debug_flag_init();

int  debug_flag_get(dbg_category flag);
void debug_flag_set(dbg_category flag, int v);

void debug_flag_dump();

#endif // debug_flag_h
