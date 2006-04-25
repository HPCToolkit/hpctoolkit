/* unw_backtrace.c -- unwinding the stack with HP's libunwind */

#include <libunwind.h>

#include "backtrace.h"

// csprof_backtrace: If buffer is NULL, simply return the length of
// the backtrace.  Otherwise, copy frames [beg, end] (inclusive, 1
// based) into 'buffer'.  The number of frame IPs copied will be
// returned.  (Thus, [1, size] will gather the whole stack.)
//
// Note: based on backtrace() from libunwind.
static unsigned int
csprof_backtrace(void **buffer, int beg, int end)
{
  static unw_cursor_t cursor;
  static unw_context_t uc;
  unw_word_t ip;
  unsigned int n = 0;      // element number, 1 based
  unsigned int stored = 0; // num of elements stored in buffer

  int count = (buffer) ? 0 : 1;
  
  unw_getcontext(&uc);
  if (unw_init_local(&cursor, &uc) < 0) {
    return 0;
  }

  while (unw_step (&cursor) > 0)
    {
      // Do we have a valid frame?
      if (unw_get_reg(&cursor, UNW_REG_IP, &ip) < 0) {
	break;
      }
      n++; // Count this valid frame
      
      // Check for the frames we are interested in
      if (!count && (n < beg)) {
	continue;
      }
      if (!count && (n > end)) {
	break;
      }
      
      // Store, if necessary (beg <= n <= end)
      if (!count) { 
	buffer[n - beg] = (void *)ip;
	stored++;
      }
    }

  if (count) {
    return n;
  } else {
    return stored;
  }
}

// csprof_sample_callstack: save callstack data.
// effects: 
// returns: standard CSPROF return values
int 
csprof_sample_callstack(csprof_state_t *state, unsigned int sample_count)
{
  unsigned int n, bt_len_max = csprof_backtrace_len();

  // 1. Find the size of the callstack and resize the backtrace array
  // if necessary.
  if (CSPROF_CS_CHOP >= bt_len_max) {
    ERRMSG("callstack appears to be empty", __FILE__, __LINE__);
    csprof_backtrace_len(); // FIXME (remove)
    return CSPROF_ERR;
  }
  
  state->bt_len = bt_len_max - CSPROF_CS_CHOP;
  if (state->bt_len > state->bt_len_max) {
    csprof_state__cache_resize(&ST, state->bt_len);
  }
  
  // 2. Collect the backtrace.  bt[0] will contain the instruction
  // pointer at the *top* of the call stack
  n = csprof_backtrace(state->bt, CSPROF_CS_CHOP + 1, bt_len_max);
  if (n != state->bt_len) {
    ERRMSG("callstack length has problematically changed", __FILE__, __LINE__);
    return CSPROF_ERR;
  }
  
  // 3. Record the backtrace
  if (csprof_csdata__insert_bt(&state->csdata, state->bt,
                               state->bt_len, sample_count) != CSPROF_OK) {
    ERRMSG("failure recording callstack sample", __FILE__, __LINE__);
    return CSPROF_ERR;
  }
  
  return CSPROF_OK;
}
