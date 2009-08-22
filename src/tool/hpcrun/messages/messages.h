#ifndef messages_h
#define messages_h

//*****************************************************************************
// global includes
//*****************************************************************************
#include <stdarg.h>
#include <stdbool.h>



//*****************************************************************************
// local includes
//*****************************************************************************

#include <messages/debug-flag.h>
#include <messages/fmt.h>


//*****************************************************************************
// macros
//*****************************************************************************

#define PMSG_LIMIT(C) if (csprof_below_pmsg_threshold()) C

#define STDERR_MSG(...) csprof_stderr_log_msg(false,__VA_ARGS__)
#define EMSG            hpcrun_emsg
#define EEMSG(...)      csprof_stderr_log_msg(true,__VA_ARGS__)

#define AMSG           hpcrun_amsg
#define PMSG(f,...)    hpcrun_pmsg(DBG_PREFIX(f), NULL, __VA_ARGS__)
#define TMSG(f,...)    hpcrun_pmsg(DBG_PREFIX(f), #f, __VA_ARGS__)
#define ETMSG(f,...)   hpcrun_pmsg_stderr(true,DBG_PREFIX(f), #f, __VA_ARGS__)
#define NMSG(f,...)    hpcrun_nmsg(DBG_PREFIX(f), #f, __VA_ARGS__)
#define ENMSG(f, ...)  hpcrun_nmsg_stderr(true, DBG_PREFIX(f), #f, __VA_ARGS__)

#define EXIT_ON_ERROR(r,e,...) csprof_exit_on_error(r,e,__VA_ARGS__)

#define csprof_abort(...) csprof_abort_w_info(messages_donothing, __VA_ARGS__)



//*****************************************************************************
// forward declarations
//*****************************************************************************

void messages_init();
void messages_fini(void);

void messages_logfile_create();
int  messages_logfile_fd(void);

void messages_donothing(void);

void hpcrun_amsg(const char *fmt,...);
void hpcrun_emsg(const char *fmt,...);
void hpcrun_emsg_valist(const char *fmt, va_list_box* box);
void hpcrun_nmsg(pmsg_category flag, const char* tag, const char *fmt,...);
void hpcrun_pmsg(pmsg_category flag, const char* tag, const char *fmt,...);

void hpcrun_pmsg_stderr(bool echo_stderr,pmsg_category flag, const char* tag, 
			const char *fmt,...);
void hpcrun_nmsg_stderr(bool echo_stderr,pmsg_category flag, const char* tag, 
			const char *fmt,...);
void csprof_stderr_log_msg(bool copy_to_log, const char *fmt,...);
void csprof_exit_on_error(int ret, int ret_expected, const char *fmt,...);

void csprof_abort_w_info(void (*info)(void),const char *fmt,...);

int csprof_below_pmsg_threshold(void);
void csprof_up_pmsg_count(void);



#endif // messages_h
