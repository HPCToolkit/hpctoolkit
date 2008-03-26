#include <stddef.h>
#include <assert.h>

#include "backtrace.h"
#include "general.h"
#include "list.h"
#include "libc.h"
#include "csprof_csdata.h"
#include "pmsg.h"

#define DUMP_LIMIT 100


void dump_backtraces(csprof_state_t *state, csprof_frame_t* unwind)
{
  int cnt = 0;
  csprof_frame_t* x;
  char as_str[LUSH_ASSOC_INFO_STR_MIN_LEN];

#if 0
  EMSG("in dump_backtraces(0x%lx, 0x%lx)",(unsigned long int) state, (unsigned long int) unwind);

  EMSG( ""); 
#endif

  if (state->bufstk != state->bufend) {
    EMSG( "--------Cached backtrace (innermost first)-----"); 
    cnt = 0;
    for (x = state->bufstk; x != state->bufend; ++x) {
      lush_assoc_info_sprintf(as_str, x->as_info);
      EMSG( "%s: ip %p | lip %p | sp %p", as_str, x->ip, x->lip, x->sp);
      /* MWF: added to prevent long backtrace printout */
      cnt++;
      if (cnt > DUMP_LIMIT) {
        EMSG("!!! Hit Dump Limit !!! ");
        break;
      }
    }
    EMSG("--------End cached backtrace-----------");
  }
  
  cnt = 0;
  EMSG(""); /* space */
  if (unwind) {

    EMSG( "-----New backtrace (innermost first)-----");
    for (x = state->btbuf; x != unwind; ++x) {
      lush_assoc_info_sprintf(as_str, x->as_info);
      EMSG( "%s: ip %p | lip %p | sp %p", as_str, x->ip, x->lip, x->sp);
      /* MWF: added to prevent long backtrace printout */
      cnt++;
      if (cnt > DUMP_LIMIT) {
        EMSG("!!! Hit Dump Limit !!! ");
        break;
      }
    }
    EMSG( "-----End new backtrace---------------"); 
  }
  else {
    EMSG( "No bt_end backtrace to dump");
  }

#if 0
  EMSG( "");

  EMSG( "other state information "); 
  EMSG( "------------------------"); 
  EMSG( "swizzle_return = 0x%lx",state->swizzle_return);
  EMSG( "last_pc = 0x%lx", state->last_pc);
#endif
}
