#ifndef debug_flag_h
#define debug_flag_h

//*****************************************************************************
// File: debug-flag.c
//
// Description:
//   debug flags management for hpcrun
//
// History:
//   23 July 2009 - John Mellor-Crummey
//     created by splitting off from messages.h, messages-sync.c
//
//*****************************************************************************


//*****************************************************************************
// macros
//*****************************************************************************

#define DBG(f)            debug_flag_get(DBG_PREFIX(f))
#define SET(f,v)          debug_flag_set(DBG_PREFIX(f), v)

#define ENABLE(f)         SET(f,1)
#define DISABLE(f)        SET(f,0)

#define ENABLED(f)        DBG(f)

#define IF_ENABLED(f)     if ( ENABLED(f) )
#define IF_DISABLED(f)    if ( ! ENABLED(f) )



//*****************************************************************************
// type declarations
//*****************************************************************************


#define DBG_PREFIX(s) DBG_##s
#define CTL_PREFIX(s) CTL_##s

typedef enum {

#undef E
#define E(s) DBG_PREFIX(s)

#include "messages.flag-defns"

#undef E

} pmsg_category;

typedef pmsg_category dbg_category;

//*****************************************************************************
// forward declarations
//*****************************************************************************

void debug_flag_init();

int  debug_flag_get(dbg_category flag);
void debug_flag_set(dbg_category flag, int v);



#endif // debug_flag_h
