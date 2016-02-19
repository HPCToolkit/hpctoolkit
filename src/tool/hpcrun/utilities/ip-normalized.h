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

#ifndef ip_normalized_h
#define ip_normalized_h

//************************* System Include Files ****************************

//*************************** User Include Files ****************************

#include <hpcrun/loadmap.h>

#include <lib/prof-lean/hpcrun-fmt.h>


//*************************** Forward Declarations **************************

//***************************************************************************

// ---------------------------------------------------------
// ip_normalized_t: A normalized instruction pointer. Since this is a
//   small struct, it can be passed by value.
// ---------------------------------------------------------

typedef struct ip_normalized_t {
  
  // ---------------------------------------------------------
  // id of load module
  // ---------------------------------------------------------
  uint16_t lm_id;

  // ---------------------------------------------------------
  // static instruction pointer address within the load module
  // ---------------------------------------------------------
  uintptr_t lm_ip;

} ip_normalized_t;


#define ip_normalized_NULL \
  { .lm_id = HPCRUN_FMT_LMId_NULL, .lm_ip = HPCRUN_FMT_LMIp_NULL }

extern const ip_normalized_t ip_normalized_NULL_lval;


// ---------------------------------------------------------
// comparison operations, mainly for cct sibling splay operations
// ---------------------------------------------------------

static inline bool
ip_normalized_eq(const ip_normalized_t* a, const ip_normalized_t* b)
{
  return ( (a == b) || (a && b
			&& a->lm_id == b->lm_id
			&& a->lm_ip == b->lm_ip) );
}


static inline bool
ip_normalized_lt(const ip_normalized_t* a, const ip_normalized_t* b)
{
  if (a == b) {
    return false;
  }
  if (! a) a = &ip_normalized_NULL_lval;
  if (! b) b = &ip_normalized_NULL_lval;

  if (a->lm_id < b->lm_id) return true;
  if (a->lm_id > b->lm_id) return false;
  if (a->lm_ip < b->lm_ip) return true;
  if (a->lm_ip > b->lm_ip) return false;
  return false;
}


static inline bool
ip_normalized_gt(const ip_normalized_t* a, const ip_normalized_t* b)
{
  return ip_normalized_lt(b, a);
}


// Converts an ip into a normalized ip using 'lm'. If 'lm' is NULL 
// the function attempts to find the load module that 'unnormalized_ip'
// belongs to. If no load module is found or the load module does not
// have a valid 'dso_info' field, ip_normalized_NULL is returned; 
// otherwise, the properly normalized ip is returned.
ip_normalized_t
hpcrun_normalize_ip(void* unnormalized_ip, load_module_t* lm);

void *hpcrun_denormalize_ip(ip_normalized_t *normalized_ip);

//***************************************************************************

#endif // ip_normalized_h
