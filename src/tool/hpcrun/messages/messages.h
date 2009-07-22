#ifndef messages_h
#define messages_h

#include <stdarg.h>
#include <stdbool.h>

#define DBG_PREFIX(s) DBG_##s
#define CTL_PREFIX(s) CTL_##s

#undef E
#define E(s) DBG_PREFIX(s)
typedef enum {
#include "messages.flag-defns"
} pmsg_category;

typedef pmsg_category dbg_category;

#if 0
#undef D
#define D(s) CTL_PREFIX(s)
typedef enum {
#include "ctl.src"
} ctl_category;
#endif

extern void pmsg_init();
extern void pmsg_fini(void);

void hpcrun_amsg(const char *fmt,...);
void hpcrun_emsg(const char *fmt,...);
void hpcrun_emsg_valist(const char *fmt, va_list args);
void hpcrun_nmsg(pmsg_category flag,const char *fmt,...);
void hpcrun_pmsg(pmsg_category flag, const char* tag, const char *fmt,...);

extern void csprof_pmsg_stderr(bool echo_stderr,pmsg_category flag, const char* tag, const char *fmt,...);
extern void csprof_stderr_log_msg(bool copy_to_log, const char *fmt,...);
extern void csprof_exit_on_error(int ret, int ret_expected, const char *fmt,...);
extern int  hpcrun_dbg_get_flag(dbg_category flag);
extern void hpcrun_dbg_set_flag(dbg_category flag, int v);

extern int  csprof_logfile_fd(void);

extern void csprof_abort_w_info(void (*info)(void),const char *fmt,...);
extern void __csprof_dc(void);

extern int csprof_below_pmsg_threshold(void);
extern void csprof_up_pmsg_count(void);

#define PMSG_LIMIT(C) if (csprof_below_pmsg_threshold()) C

#define STDERR_MSG(...) csprof_stderr_log_msg(false,__VA_ARGS__)
#define EMSG            hpcrun_emsg
#define EEMSG(...)      csprof_stderr_log_msg(true,__VA_ARGS__)

#define AMSG         hpcrun_amsg
#define PMSG(f,...)  hpcrun_pmsg(DBG_PREFIX(f), NULL, __VA_ARGS__)
#define TMSG(f,...)  hpcrun_pmsg(DBG_PREFIX(f), #f, __VA_ARGS__)
#define ETMSG(f,...) hpcrun_pmsg_stderr(true,DBG_PREFIX(f), #f, __VA_ARGS__)
#define NMSG(f,...)  hpcrun_nmsg(DBG_PREFIX(f), #f, __VA_ARGS__)

#define EXIT_ON_ERROR(r,e,...) csprof_exit_on_error(r,e,__VA_ARGS__)

#define DBG(f)      hpcrun_dbg_get_flag(DBG_PREFIX(f))
#define SET(f,v)    hpcrun_dbg_set_flag(DBG_PREFIX(f), v)

#define ENABLE(f) SET(f,1)
#define DISABLE(f) SET(f,0)

#define ENABLED(f)         DBG(f)

#define IF_ENABLED(f)      if ( ENABLED(f) )
#define IF_DISABLED(f) if ( ! ENABLED(f) )

#define csprof_abort(...) csprof_abort_w_info(__csprof_dc,__VA_ARGS__)

#endif // messages_h
