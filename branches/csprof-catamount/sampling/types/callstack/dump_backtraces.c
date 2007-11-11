#include <stddef.h>
#include <assert.h>

#include "backtrace.h"
#include "general.h"
#include "list.h"
#include "libc.h"
#include "util.h"
#include "csprof_csdata.h"

#define DUMP_LIMIT 10

void dump_backtraces(csprof_state_t *state, csprof_frame_t *unwind)
{
  int cnt = 0;
#if 0
  /* according to Froyd's diagram's this is an off by one error - johnmc 8/29 */
  csprof_frame_t *x = state->bufstk - 1;
#else
  csprof_frame_t *x = state->bufstk;
#endif

#if 0
  EMSG("in dump_backtraces(0x%lx, 0x%lx)",(unsigned long int) state, (unsigned long int) unwind);

  EMSG( ""); 
#endif

  if (state->bufstk == state->bufend){
    EMSG( "--------Saved backtrace----------------"); 
    cnt = 0;
    for( ; x != state->bufend; ++x) {
      EMSG( "ip %#lx | sp %#lx", x->ip, x->sp);
      /* MWF: added to prevent long backtrace printout */
      cnt++;
      if (cnt > DUMP_LIMIT) {
        EMSG("!!! Hit Dump Limit !!! ");
        break;
      }
    }
    EMSG("--------End saved backtrace-----------");
  }
  cnt = 0;
  EMSG( ""); /* space */
  if (unwind) {
    x = state->btbuf;

    EMSG( "-----New backtrace-------------------"); 
    for( ; x != unwind; ++x) {
      EMSG( "ip %#lx | sp %#lx", x->ip, x->sp);
      /* MWF: added to prevent long backtrace printout */
      cnt++;
      if (cnt > DUMP_LIMIT) {
        EMSG("!!! Hit Dump Limit !!! ");
        break;
      }
    }
    EMSG( "-----End new backtrace---------------"); 
  } else {
    EMSG( "No unwind backtrace to dump");
  }
#if 0
  EMSG( "");

  EMSG( "other state information "); 
  EMSG( "------------------------"); 
  EMSG( "swizzle_return = 0x%lx",state->swizzle_return);
  EMSG( "last_pc = 0x%lx", state->last_pc);
#endif
}
