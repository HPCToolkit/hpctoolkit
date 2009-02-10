#include <stddef.h>
#include <assert.h>

#include "backtrace.h"
#include "general.h"
#include "list.h"
#include "csprof_csdata.h"
#include "pmsg.h"

#include <lush/lush-support.h>

#define DUMP_LIMIT 100


void dump_backtraces(csprof_state_t *state, csprof_frame_t* unwind)
{
  int cnt = 0;
  csprof_frame_t* x;
  char as_str[LUSH_ASSOC_INFO_STR_MIN_LEN];
  char lip_str[LUSH_LIP_STR_MIN_LEN];

  if (state->bufstk != state->bufend) {
    PMSG_LIMIT(EMSG( "--------Cached backtrace (innermost first)-----")); 
    cnt = 0;
    for (x = state->bufstk; x < state->bufend; ++x) {
      lush_assoc_info_sprintf(as_str, x->as_info);
      lush_lip_sprintf(lip_str, x->lip);
      PMSG_LIMIT(EMSG( "%s: ip %p | lip %s | sp %p", as_str, x->ip, lip_str, x->sp));

      /* MWF: added to prevent long backtrace printout */
      cnt++;
      if (cnt > DUMP_LIMIT) {
        PMSG_LIMIT(EMSG("!!! Hit Dump Limit !!! "));
        break;
      }
    }
    PMSG_LIMIT(EMSG("--------End cached backtrace-----------"));
  }

  cnt = 0;
  PMSG_LIMIT(EMSG("")); /* space */
  if (unwind) {

    PMSG_LIMIT(EMSG( "-----New backtrace (innermost first)-----"));
    for (x = state->btbuf; x < unwind; ++x) {
      lush_assoc_info_sprintf(as_str, x->as_info);
      lush_lip_sprintf(lip_str, x->lip);
      PMSG_LIMIT(EMSG( "%s: ip %p | lip %s | sp %p", as_str, x->ip, lip_str, x->sp));
      /* MWF: added to prevent long backtrace printout */
      cnt++;
      if (cnt > DUMP_LIMIT) {
        PMSG_LIMIT(EMSG("!!! Hit Dump Limit !!! "));
        break;
      }
    }
    PMSG_LIMIT(EMSG( "-----End new backtrace---------------"));
  }
  else {
    PMSG_LIMIT(EMSG( "No bt_end backtrace to dump"));
  }
}
