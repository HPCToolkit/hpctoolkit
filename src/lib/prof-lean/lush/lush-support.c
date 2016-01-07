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

//***************************************************************************
//
// File: 
//   $HeadURL$
//
// Purpose:
//   LUSH: Logical Unwind Support for HPCToolkit
//
// Description:
//   [The set of functions, macros, etc. defined in the file]
//
// Author:
//   Nathan Tallent, Rice University.
//
//***************************************************************************

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

//*************************** User Include Files ****************************

#include <include/uint.h>

#include "lush-support.h"

//*************************** Forward Declarations **************************

//***************************************************************************
// 
//***************************************************************************

lush_assoc_info_t lush_assoc_info_NULL = { .bits = 0 };

const char* 
lush_assoc_tostr(lush_assoc_t as)
{
  switch (as) {
    case LUSH_ASSOC_NULL  : return "NULL";
    case LUSH_ASSOC_1_to_0: return "1-to-0";
    case LUSH_ASSOC_1_to_1: return "1-to-1";
    case LUSH_ASSOC_M_to_1: return "M-to-1";
    case LUSH_ASSOC_1_to_M: return "1-to-M";
    case LUSH_ASSOC_0_to_0: return "0-to-0";
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

lush_lip_t lush_lip_NULL = { .data8 = {0, 0} };

const char* 
lush_lip_sprintf(char* str, const lush_lip_t* x)
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
