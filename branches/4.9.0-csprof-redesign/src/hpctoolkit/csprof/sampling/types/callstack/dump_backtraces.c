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

  MSG(4,"in dump_backtraces(0x%lx, 0x%lx)",(unsigned long int) state, (unsigned long int) unwind);

  MSG(4, ""); 

  if (state->bufstk == state->bufend){
    MSG(4, "Saved backtrace ...");
    MSG(4, "------------------------"); 
    cnt = 0;
    for( ; x != state->bufend; ++x) {
      MSG(4, "ip %#lx | sp %#lx", x->ip, x->sp);
      /* MWF: added to prevent long backtrace printout */
      cnt++;
      if (cnt > DUMP_LIMIT) {
        MSG(4,"!!! Hit Dump Limit !!! ");
        break;
      }
    }
  }
  cnt = 0;
  MSG(4, ""); /* space */
  if (unwind) {
    x = state->btbuf;

    MSG(4, "New unwind backtrace ...");
    MSG(4, "------------------------"); 
    for( ; x != unwind; ++x) {
      MSG(4, "ip %#lx | sp %#lx", x->ip, x->sp);
      /* MWF: added to prevent long backtrace printout */
      cnt++;
      if (cnt > DUMP_LIMIT) {
        MSG(4,"!!! Hit Dump Limit !!! ");
        break;
      }
    }
  } else {
    MSG(4, "No unwind backtrace to dump");
  }
  MSG(4, "");

  MSG(4, "other state information "); 
  MSG(4, "------------------------"); 
  MSG(4, "swizzle_return = 0x%lx",state->swizzle_return);
  MSG(4, "last_pc = 0x%lx", state->last_pc);
}
