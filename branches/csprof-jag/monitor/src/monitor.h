/* $Id: monitor.h,v 1.2 2007/05/22 21:16:18 krentel Exp krentel $ */

#ifndef libmonitor_h
#define libmonitor_h

#include <sys/types.h>

/* Arguments to libc_start_main.
 */
struct monitor_start_main_args {
    char   *program_name;
    int   (*main) (int, char **, char **);
    int     argc;
    char  **argv;
    void  (*init) (void);
    void  (*fini) (void);
    void  (*rtld_fini) (void);
    void   *stack_end;
};

#ifdef __cplusplus
extern "C" {
#endif

/* Defined by user code */

extern void monitor_init_library();
extern void monitor_fini_library();
extern void monitor_init_process(struct monitor_start_main_args *main_args);
extern void monitor_fini_process();
extern void monitor_init_thread_support();
extern void *monitor_init_thread(unsigned tid);
extern void monitor_fini_thread(void *init_thread_data);
extern void monitor_dlopen(const char *library);

/* Defined by libmonitor, they can be weak for clients */

extern unsigned long __attribute__((weak)) monitor_gettid();
extern void __attribute__((weak)) monitor_force_fini_process();
extern void __attribute__((weak)) __attribute__((noreturn)) monitor_real_exit(int);
extern int __attribute__((weak)) monitor_real_execve(const char *filename, char *const argv [], char *const envp[]);
extern pid_t __attribute__((weak)) monitor_real_fork(void);
extern void * __attribute__((weak)) monitor_real_dlopen(const char *, int);
extern int __attribute__((weak)) monitor_opt_debug;
extern int __attribute__((weak)) monitor_opt_error;

/* Flags for monitor_opt_error that say whether monitor should handle
   abnormal termination. */

#define MONITOR_NONZERO_EXIT 	0x01
#define MONITOR_SIGINT 		0x02
#define MONITOR_SIGABRT 	0x04
#define MONITOR_SIGNALS 	(MONITOR_SIGINT|MONITOR_SIGABRT)

#ifdef __cplusplus
}
#endif

#endif
