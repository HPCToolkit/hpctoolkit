/* a hack to get compilation right until other issues are sorted out */
#ifdef CSPROF_THREADS

#include <stdlib.h>
#include <dlfcn.h>
#include <assert.h>
#include <signal.h>
#include <setjmp.h>

#include "atomic.h"
#include "mem.h"
#include "driver.h"
#include "state.h"
#include "general.h"
#include "xpthread.h"
#include "util.h"
#include "libstubs.h"

#ifdef __osf__

#define PTHREAD_CREATE_FN __pthread_create
#define PTHREAD_EXIT_FN __pthread_exit

#else

#define PTHREAD_CREATE_FN pthread_create
#define PTHREAD_EXIT_FN pthread_exit

#endif

int (*csprof_pthread_sigmask)(int, const sigset_t *, sigset_t *);

struct tramp_data {
    pthread_func *func;
    void * arg;
};

pthread_key_t thread_node_key;
pthread_key_t prof_data_key;
pthread_key_t mem_store_key;

static pthread_key_t k;

static pthread_once_t iflg = PTHREAD_ONCE_INIT;

#include "thread_data.h"

pthread_mutex_t mylock;

struct csprof_thread_queue all_threads;

extern int csprof_write_profile_data(csprof_state_t *);

/* there are some generalized interfaces (thread vs. non-thread) which
   we don't use in this file, since we already *know* we're using the
   threaded variants. */

static int library_stubs_initialized = 0;

static void
init_library_stubs()
{
#ifndef CSPROF_FIXED_LIBCALLS
#ifdef NOMON
#ifdef __osf__
    /* Tru64 does some funky things to certain pthread functions */
    CSPROF_GRAB_FUNCPTR(pthread_create, __pthread_create);
    CSPROF_GRAB_FUNCPTR(pthread_exit, __pthread_exit);
#else
    /* everybody else is pretty normal */
    CSPROF_GRAB_FUNCPTR(pthread_create, pthread_create);
    CSPROF_GRAB_FUNCPTR(pthread_exit, pthread_exit);
#endif
#endif
    CSPROF_GRAB_FUNCPTR(pthread_sigmask, pthread_sigmask);
#endif
    library_stubs_initialized = 1;
}

void
csprof_pthread_init_funcptrs()
{
    MAYBE_INIT_STUBS();
}

static void n_init(void){
  int e;
  e = pthread_key_create(&k,free);
}

void
csprof_pthread_init_data()
{
    pthread_key_create(&prof_data_key, NULL);
    pthread_key_create(&thread_node_key, NULL);
    pthread_key_create(&mem_store_key, NULL);

    pthread_mutex_init(&mylock,NULL);

    all_threads.head = all_threads.tail = NULL;
}

/* taken from Mellor-Crummey's paper */
static void
csprof_pthread_enqueue(csprof_list_node_t *node)
{
    csprof_list_node_t *last;

    node->next = NULL;
    last = (csprof_list_node_t *)csprof_atomic_exchange_pointer(&all_threads.tail, node);

    if(last == NULL) {
        csprof_atomic_exchange_pointer(&all_threads.head, node);
    }
    else {
        csprof_atomic_exchange_pointer(&last->next, node);
    }
}

/* we maintain this counter to provide sane filenames when writing out
   the profile data.  sure, we could use the pthread_t identifier for
   each thread, but that becomes unwieldly on some platforms (e.g. on
   Tru64, pthread_t is an address) and there's also the slim possibility
   that two different threads could have the same pthread_t identifier
   over the executon of a program.  unlikely, but still possible.  hence,
   this identifier */
static volatile long csprof_pthread_id = 0;

void csprof_pthread_state_init()
{
    csprof_state_t *state;
    csprof_mem_t *memstore;

    pthread_t me = pthread_self();

    DBGMSG_PUB(CSPROF_DBG_PTHREAD, "Adding %lx to the all_threads list", me);

    memstore = csprof_malloc_init(1, 0);

    if(memstore == NULL) {
        DIE("Couldn't allocate memory for profiling state storage",
            __FILE__, __LINE__);
    }

    DBGMSG_PUB(CSPROF_DBG_PTHREAD, "Setting mem_store_key");

    pthread_setspecific(mem_store_key, memstore);

    state = csprof_malloc(sizeof(csprof_state_t));
    if(state == NULL) {
        DBGMSG_PUB(1, "Couldn't allocate memory for profiling state");
    }

    DBGMSG_PUB(CSPROF_DBG_PTHREAD, "Allocated state, now init'ing and alloc'ing");

    csprof_state_init(state);
    csprof_state_alloc(state);

    DBGMSG_PUB(CSPROF_DBG_PTHREAD, "Atomically incrementing thread_id");

    state->pstate.thrid = csprof_atomic_increment(&csprof_pthread_id);

    /* must set the data before putting the thread on the list */
    pthread_setspecific(prof_data_key, state);

    /* FIXME: is this the right place to put this call? */
    csprof_driver_thread_init(state);

    /* add us to the queue/list thingie */
    {
        /* it would be awfully nice if these could be allocated from
           a freelist or contiguous memory of some sort.  unfortunately
           that would mean the introduction of locking, which is a no-no.
           so we tread all over memory with this */
        csprof_list_node_t *node = csprof_malloc(sizeof(csprof_list_node_t));
        if(node == NULL) {
            ERRMSG("Couldn't allocate memory for thread node",
                   __FILE__, __LINE__);
            return;
        }

        node->ip = me;
        node->sp = CSPROF_PTHREAD_LIVE;
        node->node = state;

        csprof_pthread_enqueue(node);
        pthread_setspecific(thread_node_key, node);
    }

    return;
}

void
csprof_pthread_state_fini2(csprof_state_t *state, csprof_list_node_t *node)
{
    sigset_t oldset;

    libcall3(csprof_pthread_sigmask, SIG_BLOCK, &prof_sigset, &oldset);

    /* FIXME: need to figure out how to unpatch for register swizzles */
    /* FIXME: need to avoid code duplication */
#ifdef CSPROF_TRAMPOLINE_BACKEND
    if(csprof_swizzle_patch_is_address(state)) {
        *(state->swizzle_patch) = state->swizzle_return;
    }
#endif
    /* instead of attempting to delete the thread's node from the
       "queue", we simply mark it as "deleted" and then ignore it
       in all subsequent scans over the list.  this has potential
       to cause problems if threads can be re-used, so we may want
       to look at this issue at a later point FIXME */
    MSG(1,"would set CSPROF THREAD DEAD f %lx",node->sp);
#ifdef NO
    node->sp = CSPROF_PTHREAD_DEAD;
#endif
    MSG(1,"fini2 driver thread fini");
    csprof_driver_thread_fini(state);

    libcall3(csprof_pthread_sigmask, SIG_SETMASK, &oldset, NULL);
}

void
csprof_pthread_state_fini()
{
    csprof_state_t *state;
    csprof_list_node_t *node;

    MSG(1,"pthread state fini");
    state = pthread_getspecific(prof_data_key);
    node = pthread_getspecific(thread_node_key);

    csprof_pthread_state_fini2(state, node);
}

static void *
csprof_pthread_tramp(void *tramp_arg)
{
    struct tramp_data *ts = (struct tramp_data *)tramp_arg;
    void *result;

    DBGMSG_PUB(CSPROF_DBG_PTHREAD, "Entering pthread_tramp");

    csprof_pthread_state_init();

    DBGMSG_PUB(CSPROF_DBG_PTHREAD, "Preparing to funcall");

    /* go about our merry way */
    result = (ts->func)(ts->arg);

    DBGMSG_PUB(CSPROF_DBG_PTHREAD, "Returned from funcall, cleaning up");

    csprof_pthread_state_fini();

    /* FIXME: free the trampoline data? */

    return result;
}

typedef struct _xptfnt {
  void *(*fn)(void *);
  void *arg_f;
} xpthread_fn_t;


#ifdef NOMON
void *doit (void *arg){
  void *rv;
  void *(*ff)(void *);
  void *argf;
  xpthread_fn_t *aa;
  thread_data_t *loc;

  MSG(1,"*** Pre fn called !!! ***");
  pthread_once(&iflg,n_init);

  aa = (xpthread_fn_t *)arg;

  ff   = aa->fn;
  argf = aa->arg_f;

  loc = malloc(sizeof(thread_data_t));
  loc->id = (int)argf;

  pthread_setspecific(k,(void *)loc);

  csprof_set_timer();
  rv   = (*ff)(argf);

  free(arg);

  return rv;
}
#endif

void thread_write_data(void){
    csprof_state_t *state;
    
    state = pthread_getspecific(prof_data_key);
    csprof_write_profile_data(state);
}


#ifdef NOMON
int
PTHREAD_CREATE_FN(pthread_t *thrid, const pthread_attr_t *attr,
		  pthread_func *func, void *funcarg)
{

  static first = 1;
#ifdef NO
    struct tramp_data *ts = malloc(sizeof(struct tramp_data));
#endif
    xpthread_fn_t *the_arg;
    int status;

#ifdef TRACE_INTERFACE
    printf("entering pthread_create (there won't be an exit)\n");
#endif

    MAYBE_INIT_STUBS();

#ifdef NO
    printf("Creating thread func %lx with data %lx\n", func, ts);
    ts->func = func;
    ts->arg = funcarg;
#endif

    MSG(1,"creating thread function w data %lx",funcarg);
    the_arg = malloc(sizeof(xpthread_fn_t));
    the_arg->fn    = func;
    the_arg->arg_f = funcarg;

#ifdef NO
    status = libcall4(csprof_pthread_create, thrid, attr,
                      csprof_pthread_tramp, (void *)ts);
#endif
    status = libcall4(csprof_pthread_create, thrid, attr,
                      doit, (void *)the_arg);

    if (first) {
         thread_data_t *loc;

         pthread_once(&iflg,n_init);
         MSG(1,"*** INIT Thread 0!!! ***");
         loc = malloc(sizeof(thread_data_t));
         loc->id = -1;

         pthread_setspecific(k,(void *)loc);
         csprof_set_timer();
         first = 0;
         MSG(1,"print self is %lx",pthread_self());
    }
    return status;
}

/* FIXME: need to get this all figured out */
void
PTHREAD_EXIT_FN(void *result)
{

#ifdef TRACE_INTERFACE
    printf("entering pthread_exit (there won't be an exit)\n");
#endif
    MAYBE_INIT_STUBS();

    MSG(1, "xpthread exiting: %#lx", pthread_self());

    csprof_pthread_state_fini();
}
#endif
int
pthread_sigmask(int mode, const sigset_t *inset, sigset_t *outset)
{
    sigset_t safe_inset;

    MAYBE_INIT_STUBS();

    if(inset != NULL) {
        /* sanitize */
        memcpy(&safe_inset, inset, sizeof(sigset_t));

        if(sigismember(&safe_inset, SIGPROF)) {
            sigdelset(&safe_inset, SIGPROF);
        }
    }

    return libcall3(csprof_pthread_sigmask, mode, &safe_inset, outset);
}

unsigned int
csprof_pthread_count()
{
    csprof_list_node_t *runner = all_threads.head;
    unsigned int count = 0;

    while(runner != NULL) {
        if(runner->sp == CSPROF_PTHREAD_LIVE) {
            count++;
        }
        runner = runner->next;
    }

    return count;
}
#endif
