#ifndef PMSG_H
#define PMSG_H

#define DBG_PREFIX(s) DBG_##s

#undef E
#define E(s) DBG_PREFIX(s)
typedef enum {
#include "pmsg.src"
} pmsg_category;


extern void pmsg_init(char *exec_name);
extern void pmsg_fini(void);
extern void csprof_emsg(const char *fmt,...);
extern void csprof_pmsg(pmsg_category flag,const char *fmt,...);
extern void csprof_nmsg(pmsg_category flag,const char *fmt,...);

#define EMSG csprof_emsg
#define PMSG(f,...) csprof_pmsg(DBG_PREFIX(f),__VA_ARGS__)
#define TMSG(f,...) csprof_pmsg(DBG_PREFIX(f),#f ": " __VA_ARGS__)
#define NMSG(f,...) csprof_nmsg(DBG_PREFIX(f),#f ": " __VA_ARGS__)

#endif // PMSG_H

