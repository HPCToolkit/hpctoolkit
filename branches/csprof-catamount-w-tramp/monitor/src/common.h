/*
 *  Internal common declarations.
 *
 *  $Id$
 */

#ifndef _MONITOR_COMMON_H_
#define _MONITOR_COMMON_H_

#include "config.h"
#ifdef MONITOR_DYNAMIC
#include <dlfcn.h>
#endif
#include <err.h>
#include <stdio.h>

/*
 *  Exactly one of MONITOR_STATIC and MONITOR_DYNAMIC must be defined,
 *  preferably on the compile line (to allow for multiple builds of
 *  the same file).
 */
#if !defined(MONITOR_STATIC) && !defined(MONITOR_DYNAMIC)
#error Must define MONITOR_STATIC or MONITOR_DYNAMIC.
#endif
#if defined(MONITOR_STATIC) && defined(MONITOR_DYNAMIC)
#error Must not define both MONITOR_STATIC and MONITOR_DYNAMIC.
#endif

#ifndef RTLD_NEXT
#define RTLD_NEXT  ((void *) -1l)
#endif

/*
 *  Format (fmt) must be string constant in these macros.
 *  Some compilers don't accept ##__VA_ARGS__ syntax, so split the
 *  macros into two.
 */
#define MONITOR_DEBUG_ARGS(fmt, ...)  do {				\
    if (monitor_debug) {						\
	fprintf(stderr, "monitor debug>> %s: " fmt , __VA_ARGS__ );	\
    }									\
} while (0)

#define MONITOR_DEBUG1(fmt)      MONITOR_DEBUG_ARGS(fmt, __func__)
#define MONITOR_DEBUG(fmt, ...)  MONITOR_DEBUG_ARGS(fmt, __func__, __VA_ARGS__)

#define MONITOR_WARN_ARGS(fmt, ...)  do {				\
    fprintf(stderr, "monitor warning>> %s: " fmt , __VA_ARGS__ );	\
} while (0)

#define MONITOR_WARN1(fmt)      MONITOR_WARN_ARGS(fmt, __func__)
#define MONITOR_WARN(fmt, ...)  MONITOR_WARN_ARGS(fmt, __func__, __VA_ARGS__)

#define MONITOR_ERROR_ARGS(fmt, ...)  do {				\
    fprintf(stderr, "monitor error>> %s: " fmt , __VA_ARGS__ );		\
    errx(1, "%s:" fmt , __VA_ARGS__ );					\
} while (0)

#define MONITOR_ERROR1(fmt)      MONITOR_ERROR_ARGS(fmt, __func__)
#define MONITOR_ERROR(fmt, ...)  MONITOR_ERROR_ARGS(fmt, __func__, __VA_ARGS__)

#define MONITOR_REQUIRE_DLSYM(var, name)  do {		\
    if (var == NULL) {					\
	const char *err_str;				\
	dlerror();					\
	var = dlsym(RTLD_NEXT, (name));			\
	err_str = dlerror();				\
	if (var == NULL) {				\
	    MONITOR_ERROR("dlsym(%s) failed: %s\n",	\
			 (name), err_str);		\
	}						\
	MONITOR_DEBUG("%s() = %p\n", (name), var);	\
    }							\
} while (0)

#ifdef MONITOR_STATIC
#define MONITOR_WRAP_NAME(name)  __wrap_##name
#define MONITOR_GET_REAL_NAME(var, name)  do {		\
    var = &name;					\
    MONITOR_DEBUG("%s() = %p\n", #name , var);		\
} while (0)
#define MONITOR_GET_REAL_NAME_WRAP(var, name)  do {		\
    var = &__real_##name;					\
    MONITOR_DEBUG("%s() = %p\n", "__real_" #name , var);	\
} while (0)
#else
#define MONITOR_WRAP_NAME(name)  name
#define MONITOR_GET_REAL_NAME(var, name)  MONITOR_REQUIRE_DLSYM(var, #name )
#define MONITOR_GET_REAL_NAME_WRAP(var, name)  MONITOR_REQUIRE_DLSYM(var, #name )
#endif

#define MONITOR_RUN_ONCE(var)				\
    static char monitor_has_run_##var = 0;		\
    if ( monitor_has_run_##var ) { return; }		\
    monitor_has_run_##var = 1

extern int monitor_debug;

void monitor_early_init(void);
void monitor_begin_process_fcn(void);
void monitor_thread_release(void);
void monitor_thread_shootdown(void);

#endif  /* ! _MONITOR_COMMON_H_ */
