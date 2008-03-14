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

//*************************** User Include Files ****************************

//*************************** Forward Declarations **************************

#if defined(__cplusplus)
extern "C" {
#endif

//***************************************************************************
// LUSH association Types
//***************************************************************************

// ---------------------------------------------------------
// bichord association classification: [p-chord <-> l-chord]
//
// - each association is a member of {0, 1, M} x {0, 1, M}
// - M represents "multi", i.e., { n | n >= 2 }
// ---------------------------------------------------------

typedef enum lush_assoc lush_assoc_t;

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


typedef union lush_assoc_info_u lush_assoc_info_t;

union lush_assoc_info_u {
  uint32_t bits;
  
  struct lush_assoc_info_s {
    lush_assoc_t as  : 8;
    uint32_t     len : 24; // (inclusive) length of path to root note: >= 1
  } u;
};



#define lush_assoc_class(/*lush_assoc*/ as) \
  ((as) & LUSH_ASSOC_CLASS_MASK)

#define lush_assoc_class_eq(/*lush_assoc*/ x, /*lush_assoc*/ y) \
  ( ((x) == (y)) /* handles x == y == LUSH_ASSOC_NULL */	\
    || lush_assoc_class(x) & lush_assoc_class(y) )


#define lush_assoc_is_a_to_0(/*lush_assoc*/ as) \
  (lush_assoc_class(as) & LUSH_ASSOC_CLASS_a_to_0)

#define lush_assoc_is_1_to_a(/*lush_assoc*/ as) \
  (lush_assoc_class(as) & LUSH_ASSOC_CLASS_1_to_a)

#define lush_assoc_is_a_to_1(/*lush_assoc*/ as) \
  (lush_assoc_class(as) & LUSH_ASSOC_CLASS_a_to_1)


#define lush_assoc_info__get_assoc(/*lush_assoc_info_t*/ x) \
  (x).u.as

#define lush_assoc_info__set_assoc(/*lush_assoc_info_t*/ x,	    \
				   /*lush_assoc_t*/ new_as)	    \
  (x).u.as = (new_as)


#define lush_assoc_info__get_path_len(/*lush_assoc_info_t*/ x)   \
  (x).u.len

#define lush_assoc_info__set_path_len(/*lush_assoc_info_t*/ x,	 \
				      /*lush_assoc_t*/ new_len)  \
  (x).u.len = (new_len)

#define lush_assoc_info_is_root_note(/*lush_assoc_info_t*/ x)    \
  ((x).u.len == 1)



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


// **************************************************************************

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* lush_support_h */
