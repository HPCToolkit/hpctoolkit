/*
 * Include file for libmonitor clients.
 *
 * $Id$
 */

#ifndef  _MONITOR_H_
#define  _MONITOR_H_

/*
 * Callbacks for the client to override.
 */
#ifdef __cplusplus
extern "C" {
#endif

extern void monitor_init_library(void);
extern void monitor_fini_library(void);
extern void monitor_init_process(char *process, int *argc, char **argv,
				 unsigned pid);
extern void monitor_fini_process(void);
extern void monitor_init_thread_support(void);
extern void *monitor_init_thread(unsigned tid);
extern void monitor_fini_thread(void *user_data);
extern void monitor_dlopen(const char *library);

#ifdef __cplusplus
}
#endif

#endif  /* ! _MONITOR_H_ */
