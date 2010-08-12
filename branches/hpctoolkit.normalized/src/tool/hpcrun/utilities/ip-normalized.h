// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL: https://outreach.scidac.gov/svn/hpctoolkit/trunk/src/tool/hpcrun/unwind/common/unwind-cfg.h $
// $Id: unwind-cfg.h 2775 2010-03-09 03:24:31Z mfagan $
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2010, Rice University 
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

#ifndef ip_normalized_h
#define ip_normalized_h

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include <hpcrun/loadmap.h>

//*************************** Forward Declarations **************************

//***************************************************************************

typedef struct ip_normalized_t {
  
  // ---------------------------------------------------------
  // The load module that a node is from. The addess of the 
  // base of the load module is used as a reference point.
  // ---------------------------------------------------------
  uint16_t lm_id;

  // ---------------------------------------------------------
  // The offset from the address of 'lm' where the instruction
  // is located at.
  // ---------------------------------------------------------
  uintptr_t offset;
} ip_normalized_t;


extern ip_normalized_t ip_normalized_NULL;

static inline bool
ip_normalized_eq(const ip_normalized_t* a, const ip_normalized_t* b)
{
  return ( (a == b) || (a && b
			&& a->lm_id == b->lm_id
			&& a->offset == b->offset) );
}


ip_normalized_t
hpcrun_normalize_ip(void *unnormalized_ip, load_module_t* lm);

//***************************************************************************

#endif // ip_normalized_h
