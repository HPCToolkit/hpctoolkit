#include <sys/types.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <ucontext.h>
#include <signal.h>
#include <dlfcn.h>
#include <setjmp.h>
#include <pthread.h>

pthread_key_t k;
typedef struct _td_t {
  int id;
  sigjmp_buf bad_unwind;
} thread_data_t;

#define PRINT(buf) fwrite_unlocked(buf,1,n,stderr);
#define PSTR(str) do { \
size_t i = strlen(str); \
fwrite_unlocked(str, 1, i, stderr); \
} while(0)

static inline void MSG(int level, char *format, ...)
{
    va_list args;
    char buf[512];

    va_start(args, format);
    int n;

    flockfile(stderr);
    n = sprintf(buf, "csprof msg [%d][%lx]: ", level, pthread_self());
    PRINT(buf);

    n = vsprintf(buf, format, args);
    PRINT(buf);
    PSTR("\n");
    fflush_unlocked(stderr);
    funlockfile(stderr);

    va_end(args);
}

static inline void ERRMSGa(char *format, char *file, int line, va_list args) 
{ 
    char buf[512];
    int n;
    flockfile(stderr);
    n = sprintf(buf, "csprof error [%s:%d]:", file, line);
    PRINT(buf);
    n = vsprintf(buf, format, args);
    PRINT(buf);
    PSTR("\n");
    fflush_unlocked(stderr);
    funlockfile(stderr);
}

static inline void ERRMSG(char *format, char *file, int line, ...) 
#if 0
{
}
#else
{
    va_list args;
    va_start(args, line);
    ERRMSGa(format, file, line, args);
    va_end(args);
}
#endif

/* WHY do I need to copy this ???? */

enum
{
  REG_R8 = 0,
# define REG_R8		REG_R8
  REG_R9,
# define REG_R9		REG_R9
  REG_R10,
# define REG_R10	REG_R10
  REG_R11,
# define REG_R11	REG_R11
  REG_R12,
# define REG_R12	REG_R12
  REG_R13,
# define REG_R13	REG_R13
  REG_R14,
# define REG_R14	REG_R14
  REG_R15,
# define REG_R15	REG_R15
  REG_RDI,
# define REG_RDI	REG_RDI
  REG_RSI,
# define REG_RSI	REG_RSI
  REG_RBP,
# define REG_RBP	REG_RBP
  REG_RBX,
# define REG_RBX	REG_RBX
  REG_RDX,
# define REG_RDX	REG_RDX
  REG_RAX,
# define REG_RAX	REG_RAX
  REG_RCX,
# define REG_RCX	REG_RCX
  REG_RSP,
# define REG_RSP	REG_RSP
  REG_RIP,
# define REG_RIP	REG_RIP
  REG_EFL,
# define REG_EFL	REG_EFL
  REG_CSGSFS,		/* Actually short cs, gs, fs, __pad0.  */
# define REG_CSGSFS	REG_CSGSFS
  REG_ERR,
# define REG_ERR	REG_ERR
  REG_TRAPNO,
# define REG_TRAPNO	REG_TRAPNO
  REG_OLDMASK,
# define REG_OLDMASK	REG_OLDMASK
  REG_CR2
# define REG_CR2	REG_CR2
};

static struct sigaction previous_sigsegv_handler;

static int catching_sigsegv = 0;

/* FIXME: this is a hack to ensure that when we are profiling malloc,
   certain uses within csprof itself don't get profiled (and lead to
   crashes) */

#define CSPROF_SIGSEGV_ALTERNATE_STACK_SIZE 16 * 1024

static unsigned char sigsegv_alternate_stack[CSPROF_SIGSEGV_ALTERNATE_STACK_SIZE];

static void
csprof_set_up_alternate_signal_stack()
{
    stack_t sigsegv_stack;
    int ret;

    /* set up alternate stack for signals so that if/when we receive a
       SIGSEGV, we still have stack in which to execute the handler. */
    sigsegv_stack.ss_sp = (void *)&sigsegv_alternate_stack[0];
    sigsegv_stack.ss_flags = 0;
    sigsegv_stack.ss_size = CSPROF_SIGSEGV_ALTERNATE_STACK_SIZE;

    ret = sigaltstack(&sigsegv_stack, NULL);

    if(ret != 0) {
        ERRMSG("Unable to setup alternate stack for SIGSEGV", __FILE__, __LINE__);
    }
}

extern jmp_buf bad_unwind;

/* catch SIGSEGVs */
static void
csprof_sigsegv_signal_handler(int sig, siginfo_t *siginfo, void *context)
{
  /*    extern void (*csprof_longjmp)(jmp_buf j); */
    thread_data_t *td = (thread_data_t *)pthread_getspecific(k);

    struct ucontext *ctx = (struct ucontext *)context;
    int i;

    if(!catching_sigsegv) {
        MSG(1,"Received SIGSEGV [%s:%d]", __FILE__, __LINE__);
	/* !!!! MWF GIANT HACK !!!!!
           sc_pc is unknown, so put something in that will compile
           !!!!!!
	printf("SIGSEGV address %lx\n",ctx->uc_mcontext.sc_pc);
	*/
	MSG(1,"SIGSEGV address %lx\n",ctx->uc_mcontext.gregs[REG_RIP]);
#ifdef NO
	for (i=REG_R8;i<=REG_CR2;i++) {
	  MSG(1,"reg[%s] = %lx",
	      _rnames[i],
	      ctx->uc_mcontext.gregs[i]);
	}
	csprof_dump_loaded_modules();
        catching_sigsegv = 1;
#endif
        /* make sure we don't hose ourselves by doing re-entrant backtracing */
        siglongjmp(td->bad_unwind,9);
        /* let the previous handler do its job */
        if(previous_sigsegv_handler.sa_flags & SA_SIGINFO) {
          /* new-style handler */
          (*previous_sigsegv_handler.sa_sigaction)(sig, siginfo, context);
        }
        else {
          /* old-style handler; must check for SIG_DFL or SIG_IGN */
          if(previous_sigsegv_handler.sa_handler == SIG_DFL) {
            abort();
          }
          else if(previous_sigsegv_handler.sa_handler == SIG_IGN) {
          }
          else {
            (*previous_sigsegv_handler.sa_handler)(sig);
          }
        }
    }
}

void
setup_segv(void){
  struct sigaction sa;
  int ret;

  csprof_set_up_alternate_signal_stack();

  /* set up handler for catching sigsegv */
  sa.sa_sigaction = csprof_sigsegv_signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;

  ret = sigaction(SIGSEGV, &sa, &previous_sigsegv_handler);

  if(ret != 0) {
    ERRMSG("Unable to install SIGSEGV handler", __FILE__, __LINE__);
  }
}
