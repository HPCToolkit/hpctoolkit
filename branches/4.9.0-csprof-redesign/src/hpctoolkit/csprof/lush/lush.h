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

//*************************** Forward Declarations **************************

//***************************************************************************
//
//***************************************************************************

// ---------------------------------------------------------
// [physical <-> logical] step associations
//   since each can be one of [0, 1, 2:n] there are 9 possibilities
// ---------------------------------------------------------

enum lush_assoc_t {
  LUSH_ASSOC_NULL = 0,

  LUSH_ASSOC_1_to_1,   //   1 <-> 1

  LUSH_ASSOC_1_n_to_0, // 2:n <-> 0 and 1 <-> 0
  LUSH_ASSOC_2_n_to_1, // 2:n <-> 1

  LUSH_ASSOC_1_to_2_n, //   1 <-> 2:n
  LUSH_ASSOC_0_to_2_n  //   0 <-> 2:n and 0 <-> 1
};


// ---------------------------------------------------------
// 
// ---------------------------------------------------------

enum lush_step_t {
  LUSH_STEP_NULL = 0,
  LUSH_STEP_DONE,
  LUSH_STEP_CONT,
  LUSH_STEP_ERROR
};


// ---------------------------------------------------------
// 
// ---------------------------------------------------------

#define lush_agentid_NULL 0
typedef lush_agentid_t int;


// ---------------------------------------------------------
// LIP: An opaque logical id
// ---------------------------------------------------------

struct lush_lip_t {
  uchar data1[8];
  uchar data2[8];
}


// ---------------------------------------------------------
// 
// ---------------------------------------------------------

struct lush_cursor_t {
  // meta info:
  //   is cursor just initialized
  //   bichord assoc type
  //   is bichord completed
  //   id for agent that provided logical info (or NULL)

  // physical context

  // logical context (opaque)
  // logical IP (opaque)
  // active marker?
};


inline lush_assoc_t 
lush_cursor_get_assoc(lush_cursor_t* cursor);

inline lush_lip_t 
lush_cursor_get_lip(lush_cursor_t* cursor);

inline void* 
lush_cursor_get_pcursor(lush_cursor_t* cursor);

inline void* 
lush_cursor_get_lcursor(lush_cursor_t* cursor);


// **************************************************************************
// 
// **************************************************************************

// Initialize the unwind.  Set a flag indicating initialization.
void lush_init(lush_cursor_t* cursor, TNT_context_t* uc);


// Given a lush_cursor, peek the next bichord.
lush_step_t lush_peek_bichord(lush_cursor_t& cursor);


// Given a lush_cursor, unwind to the next physical chord and locate the
// physical cursor.  Use the appropriate agent or local procedures.
lush_step_t lush_step_pchord(lush_cursor_t* cursor);


// Given a lush_cursor, unwind one pnote/lonote of the pchord/lchord.
lush_step_t lush_step_pnote(lush_cursor_t* cursor);
lush_step_t lush_step_lnote(lush_cursor_t* cursor);


// Given a lush_cursor, forcefully advance to the next pnote (which
// may also be the next pchord)
lush_step_t lush_forcestep_pnote(lush_cursor_t* cursor);

