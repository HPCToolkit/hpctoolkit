/*
 * Macros for extending monitor's overrides.
 *
 * $Id$
 */

#ifndef _MONITOR_EXT_H_
#define _MONITOR_EXT_H_

#include "pmsg.h"

#ifdef HPCRUN_STATIC_LINK

#define MONITOR_WRAP_NAME(name)   __wrap_ ## name
#define MONITOR_GET_NAME(var, name)  var = & name
#define MONITOR_GET_NAME_WRAP(var, name)  var = & __real_ ## name

#else  /* MONITOR DYNAMIC */

#include <dlfcn.h>

#ifndef RTLD_NEXT
#define RTLD_NEXT  ((void *) -1L)
#endif

#define MONITOR_GET_DLSYM(var, name)  do {		\
    if (var == NULL) {					\
	const char *err_str;				\
	dlerror();					\
	var = dlsym(RTLD_NEXT, #name );			\
	err_str = dlerror();				\
	if (var == NULL) {				\
	    csprof_abort("dlsym(%s) failed: %s", #name , err_str); \
	}						\
	TMSG(MONITOR_EXTS, "%s() = %p", #name , var);	\
    }							\
} while (0)

#define MONITOR_WRAP_NAME(name)  name
#define MONITOR_GET_NAME(var, name)  MONITOR_GET_DLSYM(var, name)
#define MONITOR_GET_NAME_WRAP(var, name)  MONITOR_GET_DLSYM(var, name)

#endif  /* MONITOR_STATIC */

#endif  /* ! _MONITOR_EXT_H_ */
