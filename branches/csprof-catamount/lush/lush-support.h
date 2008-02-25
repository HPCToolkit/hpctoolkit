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
#include <stdbool.h>
#include <stdint.h>

//*************************** User Include Files ****************************

#include <prim_unw.h>

//*************************** Forward Declarations **************************

// A macro to automatically declare function pointers types for each
// routine
#define LUSHI_DECL(RET, FN, ARGS)  \
  RET FN ARGS;                     \
  typedef RET (* FN ## _fn_t) ARGS


//*************************** Forward Declarations **************************

// From <lush.h>
#define LUSH_AGENTID_XXX_t      int
#define LUSH_AGENT_POOL_XXX_t   struct lush_agent_pool

//***************************************************************************
// LUSH Unwind Types
//***************************************************************************

// ---------------------------------------------------------
// bichord association classification: [p-chord <-> l-chord]
//
// - each association is a member of {0, 1, M} x {0, 1, M}
// - M represents "multi", i.e., { n | n >= 2 }
// ---------------------------------------------------------

typedef enum lush_assoc lush_assoc_t;

enum lush_assoc {
  LUSH_ASSOC_NULL   = 0x00,

  LUSH_ASSOC_1_to_1, // 1 <-> 1

  LUSH_ASSOC_1_to_0, // 1 <-> 0
  LUSH_ASSOC_M_to_0, // M <-> 0

  LUSH_ASSOC_M_to_1, // M <-> 1

  LUSH_ASSOC_1_to_M  // 1 <-> M
};


typedef struct lush_assoc_info_s lush_assoc_info_t;

struct lush_assoc_info_s {
  lush_assoc_t as    : 8;
  uint32_t     M_val : 24; // the value of M (if applicable)
};



#define lush_assoc_is_1_to_x(/*lush_assoc*/ as) \
  ((as) == LUSH_ASSOC_1_to_1 || (as) == LUSH_ASSOC_1_to_M)

#define lush_assoc_is_x_to_1(/*lush_assoc*/ as) \
  ((as) == LUSH_ASSOC_1_to_1 || (as) == LUSH_ASSOC_M_to_1)



#define lush_assoc_info__get_assoc(/*lush_assoc_info_t*/ x) \
  x.as

#define lush_assoc_info__set_assoc(/*lush_assoc_info_t*/ x, \
				   /*lush_assoc_t*/ as)	    \
  x.as = as

#define lush_assoc_info__get_M(/*lush_assoc_info_t*/ x) \
  x.M_val

#define lush_assoc_info__set_M(/*lush_assoc_info_t*/ x, \
			       /*lush_assoc_t*/ m_val)	\
  x.M_val = m_val





// ---------------------------------------------------------
// LUSH step
// ---------------------------------------------------------

typedef enum lush_step lush_step_t;

enum lush_step {
  LUSH_STEP_NULL = 0,

  LUSH_STEP_CONT,      // cursor represents valid chord/note
  LUSH_STEP_END_CHORD, // prev note  was the end of the chord
  LUSH_STEP_END_PROJ,  // prev chord was the end of the projection
  LUSH_STEP_ERROR      // error during the step
};


// ---------------------------------------------------------
// LUSH LIP: An opaque logical id
// ---------------------------------------------------------

typedef struct lush_lip lush_lip_t;

struct lush_lip {
  unsigned char data[16];
};


/*inline*/ lush_lip_t*
lush_lip_clone(lush_lip_t* x);

// ---------------------------------------------------------
// LUSH l-cursor: space for a logical cursor
// ---------------------------------------------------------

typedef struct lush_lcursor lush_lcursor_t;

struct lush_lcursor {
  unsigned char data[32];
};


// ---------------------------------------------------------
// LUSH cursor
// ---------------------------------------------------------

typedef enum lush_cursor_flags lush_cursor_flags_t;

enum lush_cursor_flags {
  LUSH_CURSOR_FLAGS_NONE = 0x00000000,
  
  //fixme: do we want this?
  //LUSH_CURSOR_FLAGS_MASK       = 0x00000000,
  //LUSH_CURSOR_FLAGS_MASK_AGENT = 0x00000000,

  // projections
  LUSH_CURSOR_FLAGS_BEG_PPROJ  = 0x00000001, // cursor @ beg of p-projection
  LUSH_CURSOR_FLAGS_END_PPROJ  = 0x00000002, // cursor @ end of p-projection
  LUSH_CURSOR_FLAGS_END_LPROJ  = 0x00000004, // cursor @ end of l-projection

  // chords
  LUSH_CURSOR_FLAGS_BEG_PCHORD = 0x00000010, // cursor @ beg of p-chord
  LUSH_CURSOR_FLAGS_END_PCHORD = 0x00000020, // cursor @ end of p-chord
  LUSH_CURSOR_FLAGS_END_LCHORD = 0x00000040  // cursor @ end of l-chord
};


typedef struct lush_cursor lush_cursor_t;

struct lush_cursor {
  // meta info
  unsigned flags;               // lush_cursor_flags
  lush_assoc_info_t as_info;    // bichord's physical-logical association
  LUSH_AGENTID_XXX_t     aid;   // agent id (if any) owning this cursor
  LUSH_AGENT_POOL_XXX_t* apool; // agent pool

  // physical cursor
  unw_cursor_t pcursor;

  // logical cursor
  lush_lcursor_t lcursor;
  lush_lip_t     lip;
  // active marker FIXME
};

// FIXME: inline
/*inline*/ bool 
lush_cursor_is_flag(lush_cursor_t* cursor, lush_cursor_flags_t f);

/*inline*/ void 
lush_cursor_set_flag(lush_cursor_t* cursor, lush_cursor_flags_t f);

/*inline*/ void 
lush_cursor_unset_flag(lush_cursor_t* cursor, lush_cursor_flags_t f);

/*inline*/ lush_assoc_t
lush_cursor_get_assoc(lush_cursor_t* cursor);

/*inline*/ void
lush_cursor_set_assoc(lush_cursor_t* cursor, lush_assoc_t as);

/*inline*/ LUSH_AGENTID_XXX_t
lush_cursor_get_aid(lush_cursor_t* cursor);

/*inline*/ void
lush_cursor_set_aid(lush_cursor_t* cursor, LUSH_AGENTID_XXX_t aid);

/*inline*/ unw_word_t
lush_cursor_get_ip(lush_cursor_t* cursor);

/*inline*/ lush_lip_t*
lush_cursor_get_lip(lush_cursor_t* cursor);

/*inline*/ unw_cursor_t*
lush_cursor_get_pcursor(lush_cursor_t* cursor);

/*inline*/ lush_lcursor_t*
lush_cursor_get_lcursor(lush_cursor_t* cursor);


// **************************************************************************

#undef LUSH_AGENTID_t
#undef LUSH_AGENT_POOL_t


#endif /* lush_support_h */
