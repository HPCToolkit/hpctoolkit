/*
 * Internal common declarations.
 *
 * $Id$
 */

#ifndef  _MONITOR_DECLS_H_
#define  _MONITOR_DECLS_H_

#ifndef RTLD_NEXT
#define RTLD_NEXT  ((void *) -1l)
#endif

/*
 * One of MONITOR_STATIC and MONITOR_DYNAMIC must be set.
 */
#if !defined(MONITOR_STATIC) && !defined(MONITOR_DYNAMIC)
#error Must define MONITOR_STATIC or MONITOR_DYNAMIC
#endif

/*
 * Format (fmt) must be string constant in these macros.
 */
#define MONITOR_DEBUG(fmt, ...)  do {			\
    if (monitor_opt_debug) {				\
	fprintf(stderr, "monitor debug>> %s: " fmt,	\
		__func__, ##__VA_ARGS__);		\
    }							\
} while (0)

#define MONITOR_WARN(fmt, ...)  do {			\
    fprintf(stderr, "monitor warning>> %s: " fmt,      	\
	    __func__, ##__VA_ARGS__);			\
} while (0)

#define MONITOR_ERROR(fmt, ...)  do {			\
    fprintf(stderr, "monitor error>> %s: " fmt,      	\
	    __func__, ##__VA_ARGS__);			\
    errx(1, fmt, ##__VA_ARGS__);			\
} while (0)

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
#define MONITOR_GET_REAL_NAME(var, name)  var = & __real_##name
#else
#define MONITOR_GET_REAL_NAME(var, name)  MONITOR_REQUIRE_DLSYM(var, #name )
#endif

#define MONITOR_RUN_ONCE(var)				\
    static char monitor_has_run_##var = 0;		\
    if ( monitor_has_run_##var ) { return; }		\
    monitor_has_run_##var = 1

#ifdef MONITOR_STATIC
#define MONITOR_WRAP_NAME(name)  __wrap_##name
#else
#define MONITOR_WRAP_NAME(name)  name
#endif

#define START_MAIN_PARAM_LIST 			\
    int (*main) (int, char **, char **),	\
    int argc,					\
    char **argv,				\
    void (*init) (void),			\
    void (*fini) (void),			\
    void (*rtld_fini) (void),			\
    void *stack_end

#define START_MAIN_ARG_LIST  \
    main, argc, argv, init, fini, rtld_fini, stack_end

struct monitor_main_args {
    int     argc;
    char  **argv;
    char  **envp;
};

typedef int start_main_fcn_t ( START_MAIN_PARAM_LIST );
typedef int main_fcn_t (int, char **, char **);
typedef void exit_fcn_t (int);

#endif  /* ! _MONITOR_DECLS_H_ */
