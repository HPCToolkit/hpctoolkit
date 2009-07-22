//*****************************************************************************
// File: messages-async.c 
//
// Description:
//   This half of the messaging system must be async-signal safe.
//   Namely, it must be safe to call these operations from within signal 
//   handlers. Operations in this file may not directly or indirectly 
//   allocate memory using malloc. They may not call functions that
//   acquire locks that may be already held by the application code or 
//   functions that may be called during synchronous profiler operations
//   that may be interrupted by asynchronous profiler operations.
//
// History:
//   19 July 2009 
//     created by partitioning pmsg.c into messages-sync.c and 
//     messages-async.c
//
//*****************************************************************************



//*****************************************************************************
// global includes 
//*****************************************************************************

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>



//*****************************************************************************
// local includes 
//*****************************************************************************

#include "disabled.h"
#include "messages.h"
#include "messages.i"
#include "thread_data.h"
#include "thread_use.h"



//*****************************************************************************
// macros
//*****************************************************************************

#define DEBUG_PMSG_ASYNC 0



//*****************************************************************************
// global variables
//*****************************************************************************

spinlock_t pmsg_lock = SPINLOCK_UNLOCKED;



//*****************************************************************************
// forward declarations
//*****************************************************************************

static void create_msg(char *buf, size_t buflen, bool add_thread_id, 
		       const char *tag, const char *fmt, va_list args);


//*****************************************************************************
// interface operations
//*****************************************************************************

void
hpcrun_emsg_valist(const char *fmt, va_list args)
{
  hpcrun_write_msg_to_log(false, false, NULL, fmt, args);
}


void
hpcrun_emsg(const char *fmt,...)
{
  va_list args;
  va_start(args,fmt);
  hpcrun_write_msg_to_log(false, true, NULL, fmt, args);
}


void
hpcrun_pmsg(pmsg_category flag, const char *tag, const char *fmt,...)
{
  if (! hpcrun_dbg_get_flag(flag)){
#if DEBUG_PMSG_ASYNC
    hpcrun_emsg("PMSG flag in = %d (%s), flag ctl = %d --> NOPRINT",
		flag, tbl[flag], hpcrun_dbg_get_flag(flag));
#endif
    return;
  }
  va_list args;
  va_start(args,fmt);
  hpcrun_write_msg_to_log(false, true, tag, fmt, args);
}


// like pmsg, except echo message to stderr when flag is set
void
hpcrun_pmsg_stderr(bool echo_stderr, pmsg_category flag, const char *tag, 
		   const char *fmt,...)
{
  if (! hpcrun_dbg_get_flag(flag)){
    return;
  }
  va_list args;
  va_start(args,fmt);
  hpcrun_write_msg_to_log(echo_stderr, true, tag, fmt, args);
}


void
hpcrun_nmsg(pmsg_category flag, const char *fmt, ...)
{
  if (! hpcrun_dbg_get_flag(flag)){
    return;
  }
  va_list args;
  va_start(args,fmt);
  hpcrun_write_msg_to_log(false, false, NULL, fmt, args);
}


void
hpcrun_amsg(const char *fmt,...)
{
  va_list args;
  va_start(args,fmt);
  hpcrun_write_msg_to_log(false, false, NULL, fmt, args);
}



//*****************************************************************************
// interface operations (within message subsystem) 
//*****************************************************************************

//
// TODO -- factor out message composition from writing
//
void
hpcrun_write_msg_to_log(bool echo_stderr, bool add_thread_id, const char *tag,
			const char *fmt, va_list args)
{
  char buf[MSG_BUF_SIZE];

  if (hpcrun_get_disabled() && (! echo_stderr)){
    return;
  }

  create_msg(buf, sizeof(buf), add_thread_id, tag, fmt, args);
  va_end(args);

  if (echo_stderr){
    fprintf(stderr,"%s",buf);
  }

  if (hpcrun_get_disabled()) return;

  spinlock_lock(&pmsg_lock);
  fprintf(log_file, "%s", buf);
  fflush(log_file);
  spinlock_unlock(&pmsg_lock);
}



//*****************************************************************************
// private operations
//*****************************************************************************

static void
create_msg(char *buf, size_t buflen, bool add_thread_id, const char *tag, 
	   const char *fmt, va_list args)
{
  char fstr[MSG_BUF_SIZE];

  fstr[0] = '\0';

  if (add_thread_id) {
    if (csprof_using_threads_p()) {
      sprintf(fstr, "[%d]: ", TD_GET(id));
    }
  }
  if (ENABLED(PID)) {
    sprintf(fstr, "[%d]: ", getpid());
  }

  if (tag) {
    char* fstr_end = fstr + strlen(fstr);
    sprintf(fstr_end, "%-5s: ", tag);
  }

  strncat(fstr, fmt, MSG_BUF_SIZE - strlen(fstr) - 5);
  strcat(fstr,"\n");

  vsnprintf(buf, buflen - 2, fstr, args);
}

