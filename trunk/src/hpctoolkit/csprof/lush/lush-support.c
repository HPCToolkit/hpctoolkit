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

//*************************** User Include Files ****************************

#include "lush-support.h"

#include <general.h>

//*************************** Forward Declarations **************************

//***************************************************************************
// 
//***************************************************************************

const char* 
lush_assoc_tostr(lush_assoc_t as)
{
  switch (as) {
    case LUSH_ASSOC_NULL  : return "NULL";
    case LUSH_ASSOC_1_to_0: return "1-to-0";
    case LUSH_ASSOC_1_to_1: return "1-to-1";
    case LUSH_ASSOC_M_to_1: return "M-to-1";
    case LUSH_ASSOC_1_to_M: return "1-to-M";
    default:                return "ERROR!"; // FIXME: DIAG_assert
  }
}


const char* 
lush_assoc_info_sprintf(char* str, lush_assoc_info_t as_info)
{
  // INVARIANT: str must have at least LUSH_ASSOC_INFO_STR_MIN_LEN slots

  lush_assoc_t as = as_info.u.as;
  unsigned len = as_info.u.len;

  const char* as_str = lush_assoc_tostr(as);  
  snprintf(str, LUSH_ASSOC_INFO_STR_MIN_LEN, "%s (%u)", as_str, len);
  str[LUSH_ASSOC_INFO_STR_MIN_LEN - 1] = '\0';
  
  return str;
}

