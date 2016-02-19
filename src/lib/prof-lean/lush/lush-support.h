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

#ifndef lush_support_h
#define lush_support_h

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

//*************************** User Include Files ****************************

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//*************************** Forward Declarations **************************

// ---------------------------------------------------------
// LUSH metric id
// ---------------------------------------------------------

#define lush_metricid_NULL (-1)


//***************************************************************************
// LUSH Agents
//***************************************************************************

// ---------------------------------------------------------
// LUSH agent id
// ---------------------------------------------------------

#define lush_agentid_NULL (0)
typedef int lush_agentid_t;


// ---------------------------------------------------------
// A LUSH agent
// ---------------------------------------------------------

typedef struct lush_agent lush_agent_t;

struct lush_agent {
  lush_agentid_t id;
  char* path;
  void* dlhandle;
};


// ---------------------------------------------------------
// A pool of LUSH agents
// ---------------------------------------------------------

typedef struct lush_agent_pool lush_agent_pool_t;


//***************************************************************************
// LUSH association Types
//***************************************************************************

// ---------------------------------------------------------
// bichord association classification: [p-chord <-> l-chord]
//
// - each association is a member of {0, 1, M} x {0, 1, M}
// - M represents "multi", i.e., { n | n >= 2 }
// ---------------------------------------------------------

enum lush_assoc {
  // Association classes 
#define MKASSOC1(as, c1)      ( ((as) << 4) | (c1) )
#define MKASSOC2(as, c1, c2)  ( ((as) << 4) | (c1) | (c2) )

  LUSH_ASSOC_CLASS_MASK   = 0x0f,
  LUSH_ASSOC_CLASS_NULL   = 0x00,
  LUSH_ASSOC_CLASS_a_to_0 = 0x01, // a-to-0
  LUSH_ASSOC_CLASS_a_to_1 = 0x02, // a-to-1
  LUSH_ASSOC_CLASS_1_to_a = 0x04, // 1-to-a

  // Associations
  LUSH_ASSOC_NULL   = 0,

  LUSH_ASSOC_1_to_0 = MKASSOC1(1, LUSH_ASSOC_CLASS_a_to_0), // 1-to-0
  LUSH_ASSOC_M_to_0 = MKASSOC1(2, LUSH_ASSOC_CLASS_a_to_0), // M-to-0

  LUSH_ASSOC_1_to_1 = MKASSOC2(3, LUSH_ASSOC_CLASS_a_to_1,
			          LUSH_ASSOC_CLASS_1_to_a), // 1-to-1

  LUSH_ASSOC_M_to_1 = MKASSOC1(4, LUSH_ASSOC_CLASS_a_to_1), // M-to-1

  LUSH_ASSOC_1_to_M = MKASSOC1(5, LUSH_ASSOC_CLASS_1_to_a), // 1-to-M

  // A special association for use during unwinding
  LUSH_ASSOC_0_to_0 = MKASSOC1(10, LUSH_ASSOC_CLASS_a_to_0) // 0-to-0

#undef MKASSOC1
#undef MKASSOC2
};

typedef enum lush_assoc lush_assoc_t;


typedef union lush_assoc_info_u lush_assoc_info_t;

union lush_assoc_info_u {
  uint32_t bits;
  
  struct lush_assoc_info_s {
    lush_assoc_t as  : 8;
    uint32_t     len : 24; // (inclusive) length of path to root note: >= 1
  } u;
};

// 
// lush assoc_info selectors, manipulators, constants
//

extern lush_assoc_info_t lush_assoc_info_NULL;

static inline unsigned
lush_assoc_class(lush_assoc_t as) 
{
  return ((as) & LUSH_ASSOC_CLASS_MASK);
}

static inline bool 
lush_assoc_is_a_to_0(lush_assoc_t as) 
{
  return (lush_assoc_class(as) & LUSH_ASSOC_CLASS_a_to_0);
}

static inline bool 
lush_assoc_is_1_to_a(lush_assoc_t as) 
{
  return (lush_assoc_class(as) & LUSH_ASSOC_CLASS_1_to_a);
}

static inline bool
lush_assoc_is_a_to_1(lush_assoc_t as) 
{
  return (lush_assoc_class(as) & LUSH_ASSOC_CLASS_a_to_1);
}

static inline uint32_t
lush_assoc_info__get_path_len(lush_assoc_info_t x)
{
  return (x).u.len;
}

static inline lush_assoc_t 
lush_assoc_info__get_assoc(lush_assoc_info_t x)
{
  return x.u.as;
}

//
// comparison ops, (mainly) for cct sibling splay tree operations
//
static inline bool
lush_assoc_info__path_len_eq(lush_assoc_info_t x, lush_assoc_info_t y)
{
  return lush_assoc_info__get_path_len(x) == lush_assoc_info__get_path_len(y);
}

static inline bool 
lush_assoc_class_eq(lush_assoc_t x, lush_assoc_t y)
{
  return ( ((x) == (y)) /* handles x == y == LUSH_ASSOC_NULL */
	   || (lush_assoc_class(x) & lush_assoc_class(y)) );
}

static inline bool
lush_assoc_info_eq(lush_assoc_info_t x, lush_assoc_info_t y)
{
  return lush_assoc_class_eq(x.u.as, y.u.as) && lush_assoc_info__path_len_eq(x, y);
}

static inline bool
lush_assoc_info_lt(lush_assoc_info_t x, lush_assoc_info_t y)
{
  if (x.u.len < y.u.len) return true;
  if (x.u.len > y.u.len) return false;
  if (lush_assoc_class_eq(x.u.as, y.u.as)) return false;
  return (x.u.as > y.u.as);
}

static inline bool
lush_assoc_info_gt(lush_assoc_info_t x, lush_assoc_info_t y)
{
  return lush_assoc_info_lt(y, x);
}

#if 0
static inline void
lush_assoc_info__set_assoc(lush_assoc_info_t& x, lush_assoc_t new_as)
{
  x->u.as = (new_as);
}
#else
#define lush_assoc_info__set_assoc(/*lush_assoc_info_t*/ x,	    \
				   /*lush_assoc_t*/ new_as)	    \
  (x).u.as = (new_as)
#endif

#if 0
static inline void
lush_assoc_info__set_path_len(lush_assoc_info_t& x, uint32_t new_len)
{
  x->.u.len = (new_len);
}
#else
#define lush_assoc_info__set_path_len(/*lush_assoc_info_t*/ x,	\
				      /*uint32_t*/ new_len)	\
  (x).u.len = (new_len)
#endif


static inline bool
lush_assoc_info_is_root_note(lush_assoc_info_t x)
{
  return (((x).u.as != LUSH_ASSOC_NULL) && ((x).u.len == 1));
}


#define LUSH_ASSOC_STR_MAX_LEN 6

const char* 
lush_assoc_tostr(lush_assoc_t as);


#define LUSH_ASSOC_INFO_STR_MIN_LEN (LUSH_ASSOC_STR_MAX_LEN + 26)

const char* 
lush_assoc_info_sprintf(char* str, lush_assoc_info_t as_info);


// ---------------------------------------------------------
// LUSH LIP: An opaque logical id
// ---------------------------------------------------------

typedef union lush_lip lush_lip_t;

union lush_lip {
#define LUSH_LIP_DATA1_SZ 16
#define LUSH_LIP_DATA8_SZ (LUSH_LIP_DATA1_SZ / 8)

  unsigned char data1[LUSH_LIP_DATA1_SZ];
  uint64_t      data8[LUSH_LIP_DATA8_SZ];
};

extern lush_lip_t lush_lip_NULL;


static inline void
lush_lip_init(lush_lip_t* x)
{
  //memset(x, 0, sizeof(*x));
  *x = lush_lip_NULL;
}


static inline bool
lush_lip_eq(const lush_lip_t* x, const lush_lip_t* y)
{
  return ((x == y) || (x && y 
		       && x->data8[0] == y->data8[0]
		       && x->data8[1] == y->data8[1])); 
}

static inline bool
lush_lip_lt(const lush_lip_t* x, const lush_lip_t* y)
{
  if (x == y) {
    return false;
  }
  if (! x) x = &lush_lip_NULL;
  if (! y) y = &lush_lip_NULL;

  if (x->data8[0] < y->data8[0]) return true;
  if (x->data8[0] > y->data8[0]) return false;
  if (x->data8[1] < y->data8[1]) return true;
  if (x->data8[1] > y->data8[1]) return false;
  return false;
}

static inline bool
lush_lip_gt(const lush_lip_t* x, const lush_lip_t* y)
{
  return lush_lip_lt(y, x);
}

#define LUSH_LIP_STR_MIN_LEN (20 * LUSH_LIP_DATA8_SZ) /* 0x + 16 + space */

const char* 
lush_lip_sprintf(char* str, const lush_lip_t* x);


// ---------------------------------------------------------
// Temporary interpreters for current set of agents
// ---------------------------------------------------------

static inline uint16_t
lush_lip_getLMId(const lush_lip_t* x)
{
  return (uint16_t)x->data8[0];
}


static inline void
lush_lip_setLMId(lush_lip_t* x, uint16_t lmId)
{
  x->data8[0] = (uint64_t)lmId;
}


static inline uint64_t
lush_lip_getLMIP(const lush_lip_t* x)
{
  return (uint64_t)x->data8[1];
}


static inline void
lush_lip_setLMIP(lush_lip_t* x, uint64_t lm_ip)
{
  x->data8[1] = lm_ip;
}


// **************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* lush_support_h */
