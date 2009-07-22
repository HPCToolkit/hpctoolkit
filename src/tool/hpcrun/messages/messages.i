#ifndef pmsg_i
#define pmsg_i

//*****************************************************************************
// File: pmsg.i
//
// Description:
//   definitions shared between the synchronous and asynchronous halves of the 
//   messaging system.
//
// History:
//   19 July 2009 
//     partition pmsg.c into messages-sync.c and messages-async.c c
//
//*****************************************************************************



//*****************************************************************************
// global includes
//*****************************************************************************

#include <lib/prof-lean/spinlock.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <lib/prof-lean/spinlock.h>



//*****************************************************************************
// macros
//*****************************************************************************

#define MSG_BUF_SIZE  4096



//*****************************************************************************
// global variables
//*****************************************************************************

extern FILE *log_file;
extern spinlock_t pmsg_lock;



//*****************************************************************************
// interface functions (within messages subsystem)
//*****************************************************************************

void hpcrun_write_msg_to_log(bool echo_stderr, bool add_thread_id, 
                             const char *tag, const char *fmt, va_list args);


#endif
