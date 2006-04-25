/* interacting with the pthreads library */
#ifndef CSPROF_PTHREAD_H

#ifdef CSPROF_THREADS
#define CSPROF_PTHREAD_H

/* system include files */
#include <pthread.h>
#include <sys/types.h>

/* user include files */
#include "structs.h"
#include "list.h"

/* constants for determining the state of a thread */
#define CSPROF_PTHREAD_LIVE ((void *)0xf00f)
#define CSPROF_PTHREAD_DEAD ((void *)0xdead)

/* based on Mellor-Crummey's implementation */
struct csprof_thread_queue {
    csprof_list_node_t *head;
    csprof_list_node_t *tail;
};

/* the key that references the profiling state of the thread */
extern pthread_key_t prof_data_key;
/* our list node on `all_threads' */
extern pthread_key_t thread_node_key;
/* per-thread memory allocator state */
extern pthread_key_t mem_store_key;

extern struct csprof_thread_queue all_threads;

/* library initialization of this "module" */
void csprof_pthread_init_funcptrs();
void csprof_pthread_init_data();

/* number of threads currently outstanding. */
unsigned int csprof_pthread_count();

/* per-thread state initialization */
void csprof_pthread_state_init();
/* per-thread state finalization */
void csprof_pthread_state_fini();
/* per-thread state finalization outside the context of the thread */
void csprof_pthread_state_fini2(csprof_state_t *, csprof_list_node_t *);

typedef void* pthread_func (void *);

extern int (*csprof_pthread_create)(pthread_t *, const pthread_attr_t *,
                                    pthread_func *, void *);
extern int (*csprof_pthread_sigmask)(int, const sigset_t *, sigset_t *);

#endif

#endif
