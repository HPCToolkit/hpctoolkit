#include <sys/types.h>
#include <stddef.h>
#include <ucontext.h>
#include <signal.h>
#include <dlfcn.h>
#include <setjmp.h>

#include "bad_unwind.h"
#include "general.h"

#ifdef NO
#include "state.h"
#include "structs.h"
#include "util.h"
#include "metrics.h"
#include "backtrace.h"
#include "libstubs.h"
#endif

extern int (*csprof_sigaction)(int sig, const struct sigaction *act,
                             struct sigaction *oact);

void (*oth_segv_handler)(int);

static char *_rnames[] = 
{
  "REG_R8",
  "REG_R9",
  "REG_R10",
  "REG_R11",
  "REG_R12",
  "REG_R13",
  "REG_R14",
  "REG_R15",
  "REG_RDI",
  "REG_RSI",
  "REG_RBP",
  "REG_RBX",
  "REG_RDX",
  "REG_RAX",
  "REG_RCX",
  "REG_RSP",
  "REG_RIP",
  "REG_EFL",
  "REG_CSGSFS",
  "REG_ERR",
  "REG_TRAPNO",
  "REG_OLDMASK",
  "REG_CR2"
};

static struct sigaction previous_sigsegv_handler;

static int catching_sigsegv = 0;

/* FIXME: this is a hack to ensure that when we are profiling malloc,
   certain uses within csprof itself don't get profiled (and lead to
   crashes) */
#ifdef NO
void *(*csprof_xmalloc)(size_t);
static void *(*csprof_xrealloc)(void *, size_t);
static void (*csprof_xfree)(void *);
#endif

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

#include "thread_data.h"
/* catch SIGSEGVs */

int segv_count = 0;

void
polite_segv_handler(int sig, siginfo_t *siginfo, void *context)
{
    extern void (*csprof_longjmp)(jmp_buf j,int v);
    _jb *it = get_bad_unwind();

#ifdef CSPROF_THREADS
    extern pthread_key_t k;
#endif

    struct ucontext *ctx = (struct ucontext *)context;

#ifdef NO
    csprof_state_t *state = csprof_get_state();
#endif

    int i;

    segv_count++;
#ifdef CSPROF_THREADS
    thread_data_t *td = (thread_data_t *)pthread_getspecific(k);
#endif

    if(!catching_sigsegv) {
        MSG(1,"Received SIGSEGV at %s,%d", __FILE__, __LINE__);
	/* !!!! MWF GIANT HACK !!!!!
           sc_pc is unknown, so put something in that will compile
           !!!!!!
	MSG(1,"SIGSEGV address %lx\n",ctx->uc_mcontext.sc_pc);
	*/
	MSG(1,"SIGSEGV address %lx\n",ctx->uc_mcontext.gregs[REG_RIP]);
	for (i=REG_R8;i<=REG_CR2;i++) {
	  MSG(1,"reg[%s] = %lx",
	      _rnames[i],
	      ctx->uc_mcontext.gregs[i]);
	}
#ifdef NO
	csprof_dump_loaded_modules();
        catching_sigsegv = 1;
#endif
        /* make sure we don't hose ourselves by doing re-entrant backtracing */
        siglongjmp(it->jb,9);
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

static void
csprof_sigsegv_signal_handler(int sig, siginfo_t *siginfo, void *context)
{
    extern void (*csprof_longjmp)(jmp_buf j,int v);
    _jb *it = get_bad_unwind();
#ifdef CSPROF_THREADS
    extern pthread_key_t k;
#endif

    struct ucontext *ctx = (struct ucontext *)context;

#ifdef NO
    csprof_state_t *state = csprof_get_state();
#endif

    int i;

    segv_count++;
#ifdef CSPROF_THREADS
    thread_data_t *td = (thread_data_t *)pthread_getspecific(k);
#endif

    if(!catching_sigsegv) {
        MSG(1,"Received SIGSEGV at %s,%d", __FILE__, __LINE__);
	/* !!!! MWF GIANT HACK !!!!!
           sc_pc is unknown, so put something in that will compile
           !!!!!!
	MSG(1,"SIGSEGV address %lx\n",ctx->uc_mcontext.sc_pc);
	*/
	MSG(1,"SIGSEGV address %lx\n",ctx->uc_mcontext.gregs[REG_RIP]);
	for (i=REG_R8;i<=REG_CR2;i++) {
	  MSG(1,"reg[%s] = %lx",
	      _rnames[i],
	      ctx->uc_mcontext.gregs[i]);
	}
#ifdef NO
	csprof_dump_loaded_modules();
        catching_sigsegv = 1;
#endif
        /* make sure we don't hose ourselves by doing re-entrant backtracing */
        siglongjmp(it->jb,9);
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

int
setup_segv(void){
  struct sigaction sa;
  int ret;

#ifdef STATIC_ONLY
  csprof_set_up_alternate_signal_stack();
#endif

  return 0;

  /* set up handler for catching sigsegv */
  sa.sa_sigaction = csprof_sigsegv_signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;

  ret = (*csprof_sigaction)(SIGSEGV, &sa, &previous_sigsegv_handler);

  if(ret != 0) {
    ERRMSG("Unable to install SIGSEGV handler", __FILE__, __LINE__);
  }
  return ret;
}
