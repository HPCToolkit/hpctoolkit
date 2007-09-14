#include <sys/types.h>
#include <stddef.h>
#include <ucontext.h>
#include <signal.h>
#include <dlfcn.h>

#include "general.h"
#include "state.h"
#include "structs.h"
#include "util.h"
#include "metrics.h"
#include "backtrace.h"
#include "libstubs.h"

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

#if 0
#define TRACE_INTERFACE 1
#endif

/* we are probably hosed if this happens */
#ifndef RTLD_NEXT
#define RTLD_NEXT -1
#endif

/* flag for malloc wrapper */
static int take_malloc_samples = 0;

static struct sigaction previous_sigsegv_handler;

static int catching_sigsegv = 0;

/* FIXME: this is a hack to ensure that when we are profiling malloc,
   certain uses within csprof itself don't get profiled (and lead to
   crashes) */
void *(*csprof_xmalloc)(size_t);
static void *(*csprof_xrealloc)(void *, size_t);
static void (*csprof_xfree)(void *);

/* catch SIGSEGVs politely (use other segv handler) */
static void
csprof_sigsegv_signal_handler(int sig, siginfo_t *siginfo, void *context)
{
    struct ucontext *ctx = (struct ucontext *)context;
    csprof_state_t *state = csprof_get_state();
    int i;

    /* make sure we don't take samples during any of this */
    take_malloc_samples = 0;

    if(!catching_sigsegv) {
        ERRMSG("Received SIGSEGV", __FILE__, __LINE__);
	/* !!!! MWF GIANT HACK !!!!!
           sc_pc is unknown, so put something in that will compile
           !!!!!!
	printf("SIGSEGV address %lx\n",ctx->uc_mcontext.sc_pc);
	*/
	MSG(1,"SIGSEGV address %lx\n",ctx->uc_mcontext.gregs[REG_RIP]);
	for (i=REG_R8;i<=REG_CR2;i++) {
	  MSG(1,"reg[%s] = %lx",
	      _rnames[i],
	      ctx->uc_mcontext.gregs[i]);
	}
	csprof_dump_loaded_modules();

        /* make sure we don't hose ourselves by doing re-entrant backtracing */
        catching_sigsegv = 1;

        /* force insertion from the root */
        state->treenode = NULL;
        state->bufstk = state->bufend;
        csprof_sample_callstack(state, 0, 0, &(ctx->uc_mcontext));

        /* note that this is the 'death' path */
        {
            csprof_cct_node_t *node = (csprof_cct_node_t *)state->treenode;
            if(node != NULL) {
                node->metrics[2] = 1;
            }
        }

        csprof_write_profile_data(state);
    }

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

/* catch SIGSEGVs */
static void
csprof_sigsegv_signal_handler(int sig, siginfo_t *siginfo, void *context)
{
    struct ucontext *ctx = (struct ucontext *)context;
    csprof_state_t *state = csprof_get_state();
    int i;

    /* make sure we don't take samples during any of this */
    take_malloc_samples = 0;

    if(!catching_sigsegv) {
        ERRMSG("Received SIGSEGV", __FILE__, __LINE__);
	/* !!!! MWF GIANT HACK !!!!!
           sc_pc is unknown, so put something in that will compile
           !!!!!!
	printf("SIGSEGV address %lx\n",ctx->uc_mcontext.sc_pc);
	*/
	MSG(1,"SIGSEGV address %lx\n",ctx->uc_mcontext.gregs[REG_RIP]);
	for (i=REG_R8;i<=REG_CR2;i++) {
	  MSG(1,"reg[%s] = %lx",
	      _rnames[i],
	      ctx->uc_mcontext.gregs[i]);
	}
	csprof_dump_loaded_modules();

        /* make sure we don't hose ourselves by doing re-entrant backtracing */
        catching_sigsegv = 1;

        /* force insertion from the root */
        state->treenode = NULL;
        state->bufstk = state->bufend;
        csprof_sample_callstack(state, 0, 0, &(ctx->uc_mcontext));

        /* note that this is the 'death' path */
        {
            csprof_cct_node_t *node = (csprof_cct_node_t *)state->treenode;
            if(node != NULL) {
                node->metrics[2] = 1;
            }
        }

        csprof_write_profile_data(state);
    }

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

static int library_stubs_initialized = 0;

static void
init_library_stubs()
{
    CSPROF_GRAB_FUNCPTR(xmalloc, malloc);
    CSPROF_GRAB_FUNCPTR(xrealloc, realloc);
    CSPROF_GRAB_FUNCPTR(xfree, free);

    library_stubs_initialized = 1;
}

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

void csprof_driver_init(csprof_state_t *state, csprof_options_t *options){
  struct sigaction sa;
  int ret;

  /* compensate */
  options->sample_period = 1;

    /* install metrics */
  csprof_set_max_metrics(3);
  csprof_set_metric_info_and_period(csprof_new_metric(),
                                    "# bytes allocated",
                                    CSPROF_METRIC_FLAGS_NIL, 1);
  csprof_set_metric_info_and_period(csprof_new_metric(),
                                    "# bytes freed",
                                    CSPROF_METRIC_FLAGS_NIL, 1);
  csprof_set_metric_info_and_period(csprof_new_metric(),
                                    "# SIGSEGVs",
                                    CSPROF_METRIC_FLAGS_NIL, 1);

  MAYBE_INIT_STUBS();
  csprof_set_up_alternate_signal_stack();

  /* set up handler for catching sigsegv */
  sa.sa_sigaction = csprof_sigsegv_signal_handler;
  sigemptyset(&sa.sa_mask);
 sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;

 ret = sigaction(SIGSEGV, &sa, &previous_sigsegv_handler);

 if(ret != 0) {
   ERRMSG("Unable to install SIGSEGV handler", __FILE__, __LINE__);
 }

 take_malloc_samples = 1;
}

void
csprof_driver_fini(csprof_state_t *state, csprof_options_t *options)
{
    take_malloc_samples = 0;
}

#ifdef CSPROF_THREADS
void
csprof_driver_thread_init(csprof_state_t *state)
{
    /* no support required */
}

void
csprof_driver_thread_fini(csprof_state_t *state)
{
    /* no support required */
}
#endif

void
csprof_driver_suspend(csprof_state_t *state)
{
    /* no support required */
}

void
csprof_driver_resume(csprof_state_t *state)
{
    /* no support required */
}


/* profiling malloc and free */

struct csprof_malloc_chunk {
    csprof_cct_node_t *allocating_node;
    size_t bytes;
};

#define POINTER_TO_CHUNK(p) (((struct csprof_malloc_chunk *)(p)) - 1)
#define CHUNK_TO_POINTER(c) ((void *)(c + 1));

#define CSPROF_SAMPLE_CHUNK_UPPER_BITS (0xDEADL << 48)
#define CSPROF_LONELY_CHUNK_UPPER_BITS (0xCAFEL << 48)

volatile int in_debugger_p = 0;

void *
malloc(size_t n_bytes)
{
  csprof_state_t *state;
  unsigned long id_bits;
  void *retval = NULL;

#ifdef TRACE_INTERFACE
  printf("entering malloc\n");
#endif

  if(n_bytes != 0) {

    if(take_malloc_samples) {
      id_bits = CSPROF_SAMPLE_CHUNK_UPPER_BITS;
    }
    else {
      id_bits = CSPROF_LONELY_CHUNK_UPPER_BITS;
    }

    /* sometimes malloc gets called before our initialization is called */
    MAYBE_INIT_STUBS();

    if(take_malloc_samples) {
      state = csprof_get_state();
      size_t samples = n_bytes;

      assert(state);

      /* force insertion from the root */
      state->treenode = NULL;
      state->bufstk = state->bufend;
      state = csprof_check_for_new_epoch(state);

#ifdef NO
      csprof_record_metric_with_unwind(0, samples, 2);
#endif
      /*** MWF try this with 1 unwind ***/
      csprof_record_metric_with_unwind(0, samples, 1);
    }
    else {
      state = NULL;
    }

    /* allocate extra space for extra information:
       1. the leaf of the tree where we took this sample;
       2. the size of this block (for later use with free()).

       Point two could be obtained by groveling inside malloc's data
       structures, but would not be portable *at all*. */
    {
      struct csprof_malloc_chunk *chunk;
      /* modify the size only if we're not reallocing...but we have to
	 check to see if we actually have a state from which to check
	 that fact. */
      size_t addon =
	state ? (csprof_state_flag_isset(state,
					 CSPROF_MALLOCING_DURING_REALLOC) ? 0 : sizeof(struct csprof_malloc_chunk)) : sizeof(struct csprof_malloc_chunk);
      size_t real_amount = n_bytes + addon;
      void *node = state ? state->treenode : NULL;

      chunk = (*csprof_xmalloc)(real_amount);

      DBGMSG_PUB(CSPROF_DBG_MALLOCPROF, "Allocated chunk(%ld) = %#lx, id=%d",
		 real_amount, chunk, id_bits >> 48);
      if(state != NULL)
	DBGMSG_PUB(CSPROF_DBG_MALLOCPROF, "Realloc'ing? %s",
		   csprof_state_flag_isset(state, CSPROF_MALLOCING_DURING_REALLOC) ? "yes" : "no");

      if(chunk == NULL) {
	/* don't mess with the erroneous return value */
	retval = chunk;
      }
      else {
	DBGMSG_PUB(CSPROF_DBG_MALLOCPROF, "Chunk data before %#lx, %#lx",
		   chunk->bytes, chunk->allocating_node);
	chunk->bytes = n_bytes;
	chunk->allocating_node = (void *)(((unsigned long)node) | id_bits);

	retval = CHUNK_TO_POINTER(chunk);
      }
    }
  }

#ifdef TRACE_INTERFACE
    printf("leaving malloc\n");
#endif

  return retval;
}

/* I think we need to override this.  Why?  Imagine a freelist-based
   implementation of malloc, where requests are rounded up to the next
   power of two.  So for non power-of-two sized requests, there's extra
   space hanging off of the end of the chunk.  On a realloc, what I would
   do is check first to see whether I can satisfy the request by eating
   some of that space, adjusting some data structures, and returning.
   Ergo, realloc probably expects to be able to look *backwards* from the
   passed in pointer to examine some things.  With our new malloc, the
   information there is not what it expects to find.

   So, what do we need to do?

   Well, we need to adjust the pointer to point at the beginning of the
   "real" chunk that malloc allocated.  And we need to adjust the request
   to take into account the fact that we want space for our extra
   information at the beginning.

   There's a problem here, though.  What happens when the system realloc
   decides that it needs to call malloc for a new chunk?  Well, malloc
   feels like it needs to tack on extra space for our extra information,
   too.  malloc adjusts the passed-back chunk and then realloc adjusts
   the passed back chunk.  This doesn't seem like it should cause problems,
   but it does waste space (and can conceivably cause a non-failing program
   to fail).  And it *does* cause seem to cause problems with at least
   one of the applications from the SPEC CPU2000 suite.

   Our solution at the moment is to set a flag in the state (not a global
   variable, since we have to be thread-safe) indicating that we are
   realloc()'ing.  malloc respects this flag and will not modify the size
   before sending things through to the system malloc.  But it *will*
   treat the newly allocated memory as containing our magic header chunk
   at the beginning and write the relevant information into it.  Then
   realloc just has to convert the chunk back into a normal application
   pointer. */
void *
realloc(void *pointer, size_t n_bytes)
{

  void *retval;

#ifdef TRACE_INTERFACE
  printf("entering realloc\n");
#endif

  MAYBE_INIT_STUBS();

  /* probably what the library does underneath, but simpler for our purposes */
  if(pointer == NULL) {
    /* don't set the flag here, since this is a new allocation */
    retval = malloc(n_bytes);
  }
  else {
    csprof_state_t *state = csprof_get_state();
    struct csprof_malloc_chunk *chunk = POINTER_TO_CHUNK(pointer);
    size_t real_amount = n_bytes + sizeof(struct csprof_malloc_chunk);
    struct csprof_malloc_chunk *newchunk;

    csprof_state_flag_set(state, CSPROF_MALLOCING_DURING_REALLOC);
    newchunk = csprof_xrealloc(chunk, real_amount);
    csprof_state_flag_clear(state, CSPROF_MALLOCING_DURING_REALLOC);

    if(newchunk == NULL) {
      /* malloc failed */
      retval = newchunk;
    }
    else {
      newchunk->bytes = n_bytes;
      retval = CHUNK_TO_POINTER(newchunk);
    }
  }

#ifdef TRACE_INTERFACE
  printf("leaving realloc\n");
#endif

  return retval;
}

static int
csprof_sampled_chunk_p(struct csprof_malloc_chunk *chunk)
{
    csprof_cct_node_t *node = chunk->allocating_node;

    unsigned long bits = (((unsigned long)node) >> 48) & 0xffff;

    return bits == ((CSPROF_SAMPLE_CHUNK_UPPER_BITS >> 48) & 0xffff);
}

static void *
csprof_remove_chunk_bits(void *x)
{
    unsigned long bits = (unsigned long) x;

    return ((void *)(bits ^ CSPROF_SAMPLE_CHUNK_UPPER_BITS));
}

void
free(void *ptr)
{
    struct csprof_malloc_chunk *chunk = POINTER_TO_CHUNK(ptr);

#ifdef TRACE_INTERFACE
    printf("entering free\n");
#endif

    MAYBE_INIT_STUBS();

    /* check to see whether this was a sampled block or not */
    /* C++ library passes NULL to free... */
    if(ptr != NULL) {
        if(csprof_sampled_chunk_p(chunk)) {
            size_t block_size = chunk->bytes;
            csprof_cct_node_t *node = chunk->allocating_node;

            DBGMSG_PUB(CSPROF_DBG_MALLOCPROF,
                       "Attempting to free %#lx | %ld, bits=%d", ptr, block_size,
                       ((unsigned long)node) >> 48);

            /* one of our own; lop the bits off */
            node = csprof_remove_chunk_bits(node);

            DBGMSG_PUB(CSPROF_DBG_MALLOCPROF, "node = %#lx", node);

            if(node != NULL) {
                node->metrics[1] += block_size;
            }

            csprof_xfree(chunk);
        }
        else {
            /* we didn't record anything in this one, but we still added info
               to the front of the chunk... */
            csprof_xfree(chunk);
        }
    }

#ifdef TRACE_INTERFACE
    printf("leaving free\n");
#endif
}
