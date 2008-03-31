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

#ifndef lush_support_h
#define lush_support_h

//************************* System Include Files ****************************

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

//*************************** User Include Files ****************************


//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//*************************** Forward Declarations **************************

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

  LUSH_ASSOC_1_to_M = MKASSOC1(5, LUSH_ASSOC_CLASS_1_to_a)  // 1-to-M

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

extern lush_assoc_info_t lush_assoc_info_NULL;

static inline unsigned
lush_assoc_class(lush_assoc_t as) 
{
  return ((as) & LUSH_ASSOC_CLASS_MASK);
}

static inline bool 
lush_assoc_class_eq(lush_assoc_t x, lush_assoc_t y)
{
  return ( ((x) == (y)) /* handles x == y == LUSH_ASSOC_NULL */
	   || (lush_assoc_class(x) & lush_assoc_class(y)) );
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


static inline lush_assoc_t 
lush_assoc_info__get_assoc(lush_assoc_info_t x)
{
  return x.u.as;
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

static inline uint32_t
lush_assoc_info__get_path_len(lush_assoc_info_t x)
{
  return (x).u.len;
}

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


static inline bool
lush_lip_eq(lush_lip_t* x, lush_lip_t* y)
{
  return ((x == y) || (x && y 
		       && x->data8[0] == y->data8[0]
		       && x->data8[1] == y->data8[1])); 
}

// **************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* lush_support_h */
