#ifndef PMSG_H
#define PMSG_H

#include <stdarg.h>

#define DBG_PREFIX(s) DBG_##s

#undef E
#define E(s) DBG_PREFIX(s)
#undef D
#define D(s) E(s)
typedef enum {
#include "pmsg.src"
} pmsg_category;
#undef E
#undef D

extern void pmsg_init();
extern void pmsg_fini(void);
extern void csprof_emsg(const char *fmt,...);
extern void csprof_emsg_valist(const char *fmt, va_list args);
extern void csprof_amsg(const char *fmt,...);
extern void csprof_pmsg(pmsg_category flag,const char *fmt,...);
extern void csprof_nmsg(pmsg_category flag,const char *fmt,...);
extern void csprof_exit_on_error(int ret, int ret_expected, const char *fmt,...);
extern int  csprof_dbg(pmsg_category flag);
extern int  csprof_logfile_fd(void);

#define EMSG csprof_emsg
#define AMSG csprof_amsg
#define PMSG(f,...) csprof_pmsg(DBG_PREFIX(f),__VA_ARGS__)
#define TMSG(f,...) csprof_pmsg(DBG_PREFIX(f),#f ": " __VA_ARGS__)
#define NMSG(f,...) csprof_nmsg(DBG_PREFIX(f),#f ": " __VA_ARGS__)
#define EXIT_ON_ERROR(r,e,...) csprof_exit_on_error(r,e,__VA_ARGS__)
#define DBG(f)      csprof_dbg(DBG_PREFIX(f))
#define IF_NOT_DISABLED(f) if ( ! DBG(f) )
#define IF_ENABLED(f)      if ( DBG(f) )

#endif // PMSG_H

