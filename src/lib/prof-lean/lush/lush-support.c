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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h> /* commonly available, unlike <stdint.h> */

//*************************** User Include Files ****************************

#include <include/general.h> /* special printf format strings */

#include "lush-support.h"

//*************************** Forward Declarations **************************

//***************************************************************************
// 
//***************************************************************************

lush_assoc_info_t lush_assoc_info_NULL = { 0 };

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

//***************************************************************************
// 
//***************************************************************************

const char* 
lush_lip_sprintf(char* str, lush_lip_t* x)
{
  str[0] = '\0';

  if (x) {
    int num;
    char* p = str;
    for (int i = 0; i < LUSH_LIP_DATA8_SZ; ++i) {
      if (i != 0) {
	sprintf(p, " ");
	p++;
      }
      num = sprintf(p, "0x%"PRIx64, x->data8[i]);
      p += num;
    }
  }

  return str;
}

#if 0
void 
lush_lip_fprint(FILE* fs, lush_lip_t* x);
{
  if (x) {
    for (int i = 0; i < LUSH_LIP_DATA8_SZ; ++i) {
      fprintf(fs, " %"PRIx64, x->data8[i]);
    }
  }
}
#endif
