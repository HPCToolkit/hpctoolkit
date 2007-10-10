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

#ifndef lush_lush_i
#define lush_lush_i

//************************* System Include Files ****************************

#include <stdlib.h>

//*************************** User Include Files ****************************

// FIXME: Part of the general csprof mess
#ifndef PRIM_UNWIND
# include <libunwind.h>
#else
# include <prim_unw.h>
#endif

//*************************** Forward Declarations **************************

// A macro to automatically declare function pointers types for each
// routine
#define LUSHI_DECL(RET, FN, ARGS)  \
  RET FN ARGS;                     \
  typedef RET (* FN ## _fn_t) ARGS


//***************************************************************************
// LUSH Agents
//***************************************************************************

// ---------------------------------------------------------
// 
// ---------------------------------------------------------

#define lush_agentid_NULL 0
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

// See <lush.h> for struct

//***************************************************************************
// LUSH Unwind Types
//***************************************************************************

// ---------------------------------------------------------
// [physical <-> logical] step associations
//   since each can be one of [0, 1, 2:n] there are 9 possibilities
// ---------------------------------------------------------

typedef enum lush_assoc lush_assoc_t;

enum lush_assoc {
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

typedef enum lush_step lush_step_t;

enum lush_step {
  LUSH_STEP_NULL = 0,
  LUSH_STEP_DONE,
  LUSH_STEP_CONT,
  LUSH_STEP_ERROR
};


// ---------------------------------------------------------
// LUSH LIP: An opaque logical id
// ---------------------------------------------------------

typedef struct lush_lip lush_lip_t;

struct lush_lip {
  unsigned char data1[8];
  unsigned char data2[8];
};


// ---------------------------------------------------------
// LUSH cursor
// ---------------------------------------------------------

typedef enum lush_cursor_flags lush_cursor_flags_t;

enum lush_cursor_flags {
  LUSH_CURSOR_FLAGS_NONE = 0x00000000,

  LUSH_CURSOR_FLAGS_INIT    = 0x00000001, // first use of the bichord
  LUSH_CURSOR_FLAGS_INITP   = 0x00000002, // first use of the pchord
  LUSH_CURSOR_FLAGS_INITL   = 0x00000004, // first use of the lchord
  LUSH_CURSOR_FLAGS_INITALL = 0x00000007, // all together now!

  LUSH_CURSOR_FLAGS_DONEP   = 0x00000010, // pchord notes are completed
  LUSH_CURSOR_FLAGS_DONEL   = 0x00000020, // lchord notes are completed
  LUSH_CURSOR_FLAGS_DONEALL = 0x00000030, // all together now!
};

typedef struct lush_cursor lush_cursor_t;

struct lush_cursor {
  // meta info
  unsigned flags;      // lush_cursor_flags
  lush_assoc_t assoc;  // physical-logial association for this bichord
  lush_agentid_t aid;  // agent id (if any) owning this cursor

  // physical cursor
  unw_cursor_t pcursor;

  // logical cursor
  void* lcursor;
  lush_lip_t lip;
  // active marker FIXME
};


// **************************************************************************

#endif /* lush_lush_i */
