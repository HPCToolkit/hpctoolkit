#include <dlfcn.h>
/****************************************************************************
 * Library initialization and finalization
 ****************************************************************************/

/* Var and name can't be expressions here.
 */
#define MONITOR_TRY_SYMBOL(var, name)  do {		\
    if (var == NULL) {					\
	const char *err_str;				\
	dlerror();					\
	var = dlsym(RTLD_NEXT, (name));			\
	err_str = dlerror();				\
	if (err_str != NULL) {				\
	    MONITOR_WARN("dlsym(%s) failed: %s\n",	\
			 (name), err_str);		\
	}						\
	MONITOR_DEBUG("%s() = %p\n", (name), var);	\
    }							\
} while (0)

#define MONITOR_REQUIRE_SYMBOL(var, name)  do {		\
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
