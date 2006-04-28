#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "env.h"
#include "xpthread.h"
#include "state.h"
#include "general.h"
#include "mem.h"
#include "epoch.h"

#ifndef CSPROF_THREADS
/* non-threaded profilers can have a single profiling state...
   but it can't be statically allocated because of epochs */
static csprof_state_t *current_state;
#endif

/* forward declarations */
static int csprof_state__gupdate_pstate(csprof_state_t* x);
static int csprof_state__destroy_pstate(csprof_state_t* x);

static int csprof_pstate__init(csprof_pstate_t* x);
static int csprof_pstate__fini(csprof_pstate_t* x);


/* fetching states */

static csprof_state_t *
csprof_get_state_internal()
{
#ifdef CSPROF_THREADS
    return pthread_getspecific(prof_data_key);
#else
    return current_state;
#endif
}

csprof_state_t *
csprof_get_state()
{
    csprof_state_t *state = csprof_get_state_internal();

#ifdef CSPROF_THREADS
    if(state == NULL) {
        csprof_pthread_state_init();
        state = csprof_get_state_internal();
    }
#endif

    return state;
}

void
csprof_set_state(csprof_state_t *state)
{
    csprof_state_t *old = csprof_get_state_internal();

    state->next = old;

#ifdef CSPROF_THREADS
    pthread_setspecific(prof_data_key, state);
#else
    current_state = state;
#endif
}

#define CSPROF_NEED_PSTATE 0

int
csprof_state_init(csprof_state_t *x)
{
    /* ia64 Linux has this function return a `long int', which is a 64-bit
       integer.  Tru64 Unix returns an `int'.  it probably won't hurt us
       if we get truncated on ia64, right? */
    int hostid = gethostid(); // FIXME: added gethostid
    pid_t pid = getpid();

    memset(x, 0, sizeof(*x));

    x->pstate.pid = pid;
    x->pstate.hostid = hostid;

#if CSPROF_NEED_PSTATE
    // Persistent state: Note that filename should be a function of pid,
    // b/c we may need to find it again. // FIXME: added gethostid
    csprof_pstate__init(&x->pstate);
    sprintf(x->pstate_fnm, "./%s%ld-%d%s", /* opts.out_path ,*/ /* FIXME */
            CSPROF_FNM_PFX, hostid, pid, CSPROF_PSTATE_FNM_SFX);
    if (csprof_state__gupdate_pstate(x) != CSPROF_OK) {
        DIE("could not read/update persistent state file '%s'", __FILE__, __LINE__, x->pstate_fnm);
    }
#endif

    return CSPROF_OK;
}

/* csprof_state_alloc: Special initialization for items stored in
   private memory.  Private memory must be initialized!  Returns
   CSPROF_OK upon success; CSPROF_ERR on error. */
int 
csprof_state_alloc(csprof_state_t *x)
{
  csprof_csdata__init(&x->csdata);

  x->epoch = csprof_get_epoch();

#ifdef CSPROF_TRAMPOLINE_BACKEND
  x->pool = csprof_list_pool_new(32);
#if defined(CSPROF_LIST_BACKTRACE_CACHE)
  x->backtrace = csprof_list_new(x->pool);
#else
  x->btbuf = csprof_malloc(sizeof(csprof_frame_t)*32);
  x->bufend = x->btbuf + 32;
  x->bufstk = x->bufend;
  x->treenode = NULL;
#endif
#else
  x->bt_len_max = CSPROF_BACKTRACE_CACHE_INIT_SZ;
  x->bt = csprof_malloc(sizeof(void *) * x->bt_len_max);
#endif

  return CSPROF_OK;
}

int
csprof_state_fini(csprof_state_t *x)
{
#if CSPROF_NEED_PSTATE
  csprof_pstate__fini(&x->pstate);
  csprof_state__destroy_pstate(x);
#endif

  return CSPROF_OK;
}

int
csprof_state_insert_backtrace(csprof_state_t *state, int metric_id,
			      csprof_frame_t *start, csprof_frame_t *end,
			      size_t count)
{
    void *tn = csprof_csdata_insert_backtrace(&state->csdata, state->treenode,
					      metric_id, start, end, count);
    DBGMSG_PUB(CSPROF_DBG_CCT_INSERTION, "Treenode is %p", tn);

    state->treenode = tn;

    if(tn == NULL) {
	return CSPROF_ERR;
    }
    else {
        return CSPROF_OK;
    }
}

csprof_frame_t *
csprof_state_expand_buffer(csprof_state_t *state, csprof_frame_t *unwind)
{
    /* how big is the current buffer? */
    size_t sz = state->bufend - state->btbuf;
    size_t newsz = sz*2;
    /* how big is the current backtrace? */
    size_t btsz = state->bufend - state->bufstk;
    /* how big is the backtrace we're recording? */
    size_t recsz = unwind - state->btbuf;
    /* get new buffer */
    csprof_frame_t *newbt = csprof_malloc(newsz*sizeof(csprof_frame_t));

    if(state->bufstk > state->bufend) {
        DIE("Invariant bufstk > bufend violated", __FILE__, __LINE__);
    }

    /* copy frames from old to new */
    memcpy(newbt, state->btbuf, recsz*sizeof(csprof_frame_t));
    memcpy(newbt+newsz-btsz, state->bufend-btsz, btsz*sizeof(csprof_frame_t));

    /* setup new pointers */
    state->btbuf = newbt;
    state->bufend = newbt+newsz;
    state->bufstk = newbt+newsz-btsz;

    /* return new unwind pointer */
    return newbt+recsz;
}

/* csprof_state_free: Special finalization for items stored in
   private memory.  Private memory must be initialized!  Returns
   CSPROF_OK upon success; CSPROF_ERR on error. */
int
csprof_state_free(csprof_state_t *x)
{
  csprof_csdata__fini(&x->csdata);

  // no need to free memory

  return CSPROF_OK;
}


/* persistent state handling */

/* gets and updates (a 'gupdate', of course!) the persistent state. */
static int 
csprof_state__gupdate_pstate(csprof_state_t* x)
{
#if CSPROF_NEED_PSTATE
  int fexists = 0, fd;
  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
  ssize_t sz;
  long int hostid = gethostid(); // FIXME: added gethostid
  pid_t pid = getpid();
  
  if (x->pstate_fnm[0] == '\0') {
    return CSPROF_ERR;
  }
  
  // 1. Open the associated file, checking to see if it already exists
  // (use unbuffered access).
  fd = open(x->pstate_fnm, O_RDWR | O_CREAT | O_EXCL, mode); 
  if (fd < 0) { 
    // the file exists; please reopen
    fexists = 1;
    fd = open(x->pstate_fnm, O_RDWR, mode);
    if (fd < 0) { return CSPROF_ERR; }
  }

  /* 2. Read any existing persistent state (Note: It is permissible
     to simply read/write structures because this file is only valid
     for the duration of the profile run.  Thus, we do not have to
     worry about issues like compiler-added structure padding or
     endianness b/c the reads/writes will be self-consistent during
     the profile run.) */
  if (fexists) {

    if (lseek(fd, 0, SEEK_SET) < 0) { return CSPROF_ERR; }
    sz = read(fd, &x->pstate, sizeof(x->pstate));
    if (sz != sizeof(x->pstate)) { return CSPROF_ERR; }

    /* 2a. Sanity check */
    if (x->pstate.pid != pid) { return CSPROF_ERR; } 
    
  }
  
  /* 3. Update and save persistent state */
  x->pstate.hostid = hostid; // FIXME: added gethostid
  x->pstate.pid = pid;
  x->pstate.ninit++;
  
  if (lseek(fd, 0, SEEK_SET) < 0) { return CSPROF_ERR; }
  sz = write(fd, &x->pstate, sizeof(x->pstate));
  if (sz != sizeof(x->pstate)) { return CSPROF_ERR; }

  if (close(fd) < 0) { return CSPROF_ERR; }
#endif

  return CSPROF_OK;
}

static int 
csprof_state__destroy_pstate(csprof_state_t* x)
{
#if CSPROF_NEED_PSTATE
  if (unlink(x->pstate_fnm) < 0) {
    DBGMSG_PUB(1, "error removing persistant state file '%s'", __FILE__, __LINE__, x->pstate_fnm);
    return CSPROF_ERR;
  }
#endif
  return CSPROF_OK;
}

static int 
csprof_pstate__init(csprof_pstate_t* x)
{
  memset(x, 0, sizeof(*x));
  return CSPROF_OK;
}

static int 
csprof_pstate__fini(csprof_pstate_t* x)
{
  return CSPROF_OK;
}
