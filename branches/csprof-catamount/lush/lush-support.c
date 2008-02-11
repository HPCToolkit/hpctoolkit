// -*-Mode: C++;-*- // technically C99
// $Id$

//***************************************************************************
//
// File: 
//    $Source$
//
// Purpose:
//    LUSH: Logical Unwind Support for HPCToolkit
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

#include <dlfcn.h>

//*************************** User Include Files ****************************

#include "lush-support.h"

#include <general.h>
#include <state.h>

//*************************** Forward Declarations **************************

//***************************************************************************
// LUSH Unwind Types
//***************************************************************************

bool 
lush_cursor_is_flag(lush_cursor_t* cursor, lush_cursor_flags_t f)
{ 
  return (cursor->flags & f); 
}


void 
lush_cursor_set_flag(lush_cursor_t* cursor, lush_cursor_flags_t f)
{
  cursor->flags = cursor->flags | f;
}


void 
lush_cursor_unset_flag(lush_cursor_t* cursor, lush_cursor_flags_t f)
{
  cursor->flags = cursor->flags & ~f;
}


lush_assoc_t 
lush_cursor_get_assoc(lush_cursor_t* cursor)
{
  return cursor->assoc;
}


void
lush_cursor_set_assoc(lush_cursor_t* cursor, lush_assoc_t as)
{
  cursor->assoc = as;
}


lush_agentid_t
lush_cursor_get_aid(lush_cursor_t* cursor)
{
  return cursor->aid;
}


void
lush_cursor_set_aid(lush_cursor_t* cursor, lush_agentid_t aid)
{
  cursor->aid = aid;
}


unw_word_t
lush_cursor_get_ip(lush_cursor_t* cursor)
{
  unw_word_t ip = 0;
  if (unw_get_reg(&cursor->pcursor, UNW_REG_IP, &ip) < 0) {
    // FIXME
  }
  return ip;
}


lush_lip_t*
lush_cursor_get_lip(lush_cursor_t* cursor)
{
  return &cursor->lip;
}


unw_cursor_t*
lush_cursor_get_pcursor(lush_cursor_t* cursor)
{
  return &cursor->pcursor;
}


lush_lcursor_t*
lush_cursor_get_lcursor(lush_cursor_t* cursor)
{
  return &cursor->lcursor;
}
