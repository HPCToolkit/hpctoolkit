/*
 * Include file for libmonitor clients.
 *
 * $Id: monitor.h,v 1.1 2007/06/07 19:47:55 krentel Exp krentel $
 */

#ifndef  _MONITOR_H_
#define  _MONITOR_H_

/*
 * Arguments to libc_start_main, passed to monitor_init_process().
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

/*
 * Callbacks for the client to override.
 */
#ifdef __cplusplus
extern "C" {
#endif
extern void monitor_init_library();
extern void monitor_fini_library();
extern void monitor_init_process(struct monitor_start_main_args *main_args);
extern void monitor_fini_process();
extern void monitor_init_thread_support();
extern void *monitor_init_thread(unsigned num);
extern void monitor_fini_thread(void *user_data);
extern void monitor_dlopen(const char *library);
#ifdef __cplusplus
}
#endif

#endif  /* ! _MONITOR_H_ */
