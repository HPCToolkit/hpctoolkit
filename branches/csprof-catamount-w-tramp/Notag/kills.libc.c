/***** MWF set brackets for backtrace ****/
void backtrace_set_brackets(void (*a)(void),void (*b)(void),
                            void *__unbounded s);

/***** MWF added grab start_main for fenceposting stack ****/
#define PARAMS_START_MAIN (int (*main) (int, char **, char **),              \
			   int argc,                                         \
			   char *__unbounded *__unbounded ubp_av,            \
			   void (*init) (void),                              \
                           void (*fini) (void),                              \
			   void (*rtld_fini) (void),                         \
			   void *__unbounded stack_end)

typedef int (*libc_start_main_fptr_t) PARAMS_START_MAIN;
typedef void (*libc_start_main_fini_fptr_t) (void);

/* libc intercepted routines */
static int  csprof_libc_start_main PARAMS_START_MAIN;
/* static void hpcrun_libc_start_main_fini(); */

/* Intercepted libc routines: set when the library is
   initialized */
static libc_start_main_fptr_t      real_start_main;


/* FIXME: grotty FIXED_LIBCALLS stuff */

/* libc functions which we need to override */
#ifdef CSPROF_FIXED_LIBCALLS
/* these are hacks and need to be changed depending on the libc version */
static void (*csprof_longjmp)(jmp_buf, int)
    = 0x3ff800f8550;
static void (*csprof_siglongjmp)(jmp_buf, int)
    = 0x3ff801b2a40;
static void (*csprof__longjmp)(jmp_buf, int)
    = 0x3ff801b2d40;
static old_sig_handler_func_t (*csprof_signal)(int, old_sig_handler_func_t)
    = 0x3ff800e0b50;
static void *(*csprof_dlopen)(const char *, int)
    = 0x3ff800e2bb0;
int (*csprof_setitimer)(int, struct itimerval *, struct itimerval *)
    = 0x3ff800d55f0;
int (*csprof_sigprocmask)(int, const sigset_t *, sigset_t *)
    = 0x3ff800d7d30;
static void (*csprof_exit)(int)
    = 0x3ff800dde90;
#else
#endif
    /* init_library_stubs */
#ifndef CSPROF_FIXED_LIBCALLS
    /**** MWF grab libc_start_main, lifted from hpcrun code ****/
    fprintf(stderr,"attempting go grab libc_start_main\n");
    real_start_main = 
    (libc_start_main_fptr_t)dlsym(RTLD_NEXT, "__libc_start_main");
    if (!real_start_main) {
      real_start_main = 
        (libc_start_main_fptr_t)dlsym(RTLD_NEXT, "__BP___libc_start_main");
    }
    if (!real_start_main) {
      fprintf(stderr,"!! Failed to grab libc_start_main !!\n");
    }
  /*** MWF END of grab libc_start_main ***/

#endif

/*
 *** MWF grabbed from hpcrun ***
 *  Intercept the start routine: this is from glibc and can be one of
 *  two different names depending on how the macro BP_SYM is defined.
 *    glibc-x/sysdeps/generic/libc-start.c
 */

/** MWF NOTE the _gnxl0, _gnxl1 null fencepost routines here **/

extern void _gnxl0(void){int _dc; _dc = 0;}
extern int 
__libc_start_main PARAMS_START_MAIN
{
  csprof_libc_start_main(main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  return 0; /* never reached */
}
extern void _gnxl1(void){int _dc; _dc = 1;}

extern int 
__BP___libc_start_main PARAMS_START_MAIN
{
  csprof_libc_start_main(main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  return 0; /* never reached */
}

static int (*the_main)(int, char **, char **);

static int faux_main(int n, char **argv, char **env){
  MSG(1,"calling regular main f faux main");
  return (*the_main)(n,argv,env);
}

static int 
csprof_libc_start_main PARAMS_START_MAIN
{
  /* launch the process */
  fprintf(stderr, "intercepted start main");
  fprintf(stderr,"_libc_start_main arguments: main: %p, argc: %d, upb_ac: %p,"
     "init: %p, fini: %p, rtld: %p, stack_end: %p\n",
     main, argc, ubp_av, init, fini, rtld_fini, stack_end);

  fprintf(stderr,"**rb = %lx, **lb = %lx, libc_sm = %lx\n",
          &_gnxl0,&_gnxl1,&__libc_start_main);
  backtrace_set_brackets(&_gnxl0,&_gnxl1,stack_end);
  MSG(1,"using faux_main now");
  the_main = main;
  (*real_start_main)(faux_main, argc, ubp_av, init, fini, rtld_fini, stack_end);
  return 0; /* never reached */
}

#if 0
/* somehow we need to expose this to the main library */
static old_sig_handler_func_t oldprof_func;

/* attempt to stop uninformed people from yanking out our profiler */
old_sig_handler_func_t
signal(int sig, old_sig_handler_func_t func)
{
    MAYBE_INIT_STUBS();

    if(sig != SIGPROF) {
        return libcall2(csprof_signal, sig, func);
    }
    else {
        return func;
    }
}
#endif


/* this override appears to be problematic */
#if 0
int
sigprocmask(int mode, const sigset_t *inset, sigset_t *outset)
{
    /* FIXME: duplication between pthread_sigmask and this function.
       Should have a `csprof_sanitize_sigset' somewhere. */
    sigset_t safe_inset;

    MAYBE_INIT_STUBS();

    if(inset != NULL) {
        /* sanitize */
        memcpy(&safe_inset, inset, sizeof(sigset_t));

        if(sigismember(&safe_inset, SIGPROF)) {
            sigdelset(&safe_inset, SIGPROF);
        }
    }

    return libcall3(csprof_sigprocmask, mode, &safe_inset, outset);
}
#endif


void
exit(int status)
{
    MAYBE_INIT_STUBS();

    MSG(CSPROF_MSG_SHUTDOWN, "Exiting...");

    libcall1(csprof_exit, status);
}

void
_exit(int status)
{
    MAYBE_INIT_STUBS();

    MSG(CSPROF_MSG_SHUTDOWN, "Exiting via the _exit call");

#ifdef CSPROF_THREADS
    MSG(CSPROF_MSG_SHUTDOWN, "Thread %ld calling...", pthread_self());
#endif

    libcall1(csprof__exit, status);
}


