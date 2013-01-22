// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$ 
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2012, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *

/*
 * Interval tree code specific to unwind_java intervals.
 * This is the interface to the unwind_java interval tree.
 *
 * Note: the external functions assume the tree is not yet locked.
 *
 * $Id$
 */

#include <sys/types.h>
#include <assert.h>
#include <string.h>

#include <loadmap.h>
#include <memory/hpcrun-malloc.h>
#include <messages/messages.h>
#include <lib/prof-lean/spinlock.h>

#include <jni.h>

#include "fnbounds_interface.h"
#include "splay.h"
#include "splay-interval.h"
#include "thread_data.h"
#include "fnbounds_java.h"
#include "safe-sampling.h"

#include "java/java_asgct.h"
#include "java/jmt.h"


#define UI_TREE_JAVA_LOCK  do {	 \
  TD_GET(splay_lock) = 0;	 \
  spinlock_lock(&ui_tree_lock);  \
  TD_GET(splay_lock) = 1;	 \
} while (0)

#define UI_TREE_JAVA_UNLOCK  do {       \
  spinlock_unlock(&ui_tree_lock);  \
  TD_GET(splay_lock) = 0;	   \
} while (0)

#define JAVA_MAX_FRAMES 256

static void free_ui_java_tree_locked(interval_tree_node *tree);
static void free_ui_java_node_locked(interval_tree_node *node);

// Locks both the UI tree and the UI free list.
static spinlock_t ui_tree_lock;

static interval_tree_node *ui_tree_root = NULL;
static interval_tree_node *ui_free_list = NULL;
static size_t the_ui_size = 0;

static load_module_t *lm_java = NULL;

static JavaVM 	*java_vm;
static jvmtiEnv *java_jvmti;
static JNIEnv *java_env;

//---------------------------------------------------------------------
// interface operations
//---------------------------------------------------------------------


/***
 * Initialize java interval tree
 * 
 * @param jo_filename : filename of the .jo file
 *
 ***/
void
hpcjava_interval_tree_init(const char *jo_filename)
{
  TMSG(JAVA, "init unwind_java interval tree");

  ui_tree_root = NULL;

  dso_info_t *dso_info  = hpcrun_dso_make(jo_filename, NULL, NULL,
			  0, 0, 0 );
  lm_java = hpcrun_loadmap_map(dso_info);

  UI_TREE_JAVA_UNLOCK;
}


void
hpcjava_release_splay_lock(void)
{
  UI_TREE_JAVA_UNLOCK;
}


/*
 * Return a node from the free list if non-empty, else make a new one
 * via hpcrun_malloc().  Each architecture has a different struct
 * size, but they all extend interval tree nodes, which is all we use
 * here.
 *
 * Note: the claim is that all calls to hpcjava_ui_malloc() go through
 * hpcjava_addr_to_interval() and thus the free list doesn't need its
 * own lock (maybe a little sleazy, and be careful the code doesn't
 * grow to violate this assumption).
 */
void *
hpcjava_ui_malloc(size_t ui_size)
{
    void *ans;

  /* Verify that all calls have the same ui_size. */
  if (the_ui_size == 0)
    the_ui_size = ui_size;
  assert(ui_size == the_ui_size);

  if (ui_free_list != NULL) {
    ans = (void *)ui_free_list;
    ui_free_list = RIGHT(ui_free_list);
  } else {
    ans = hpcrun_malloc(ui_size);
  }

  if (ans != NULL)
    memset(ans, 0, ui_size);

  return (ans);
}


void 
hpcjava_set_jvmti(JavaVM *jvm, jvmtiEnv *jvmti)
{
  java_vm = jvm;
  java_jvmti = jvmti;
}

void 
hpcjava_attach_thread(JavaVM *vm, jvmtiEnv *jvmti, JNIEnv *jenv)
{
  jint res;
  hpcjava_set_jvmti(vm, jvmti);
  java_env = jenv;

  res = (*vm)->AttachCurrentThread(vm, (void **)&java_env, NULL);
  TMSG(JAVA, "AttachCurrentThread: %d", res);
}

void
hpcjava_detach_thread()
{
  (*java_vm)->DetachCurrentThread(java_vm);
}

/***
 * Asynchronous Java to get call stack
 * 
 * return list of call stacks
 ***/
static int
hpcjava_get_async_call_trace(void **callstack, int count)
{
  ASGCT_CallTrace trace;
  ucontext_t uContext;

  ASGCT_CallFrame storage[JAVA_MAX_FRAMES]; 
  trace.frames = storage;
  trace.env_id = java_env;

  getcontext(&uContext);

  AsyncGetCallTrace(&trace, count, &uContext);
  if (trace.num_frames>0) 
  {
    if (trace.frames != NULL)
    {
      TMSG(JAVA, "frames AsyncGetCallTrace: %d", trace.num_frames);
      int i;
      for (i=0; i<trace.num_frames; i++)
      {
	if (trace.frames[i].method_id != NULL && trace.frames[i].lineno>=0) {
	  //jlocation start, end;
	  //jvmtiError err = (*java_jvmti)->GetMethodLocation(java_jvmti, trace.frames[i].method_id, &start, &end);
	  //TMSG(JAVA, "%d.  %d: %p (%p)", i, err, trace.frames[i].method_id, start);
	  void *addr = jmt_get_address(trace.frames[i].method_id);
	  TMSG(JAVA, "%d  : %p (%p)", i, trace.frames[i].method_id, addr);
	}
      }
    } else
    {
      TMSG(JAVA, "null frames from AsyncGetCallTrace");
    }
  } else 
  {
    TMSG(JAVA, "AsyncGetCallTrace: %d frames", trace.num_frames);
  }
  *callstack = trace.frames;
  return trace.num_frames;
}

static jvmtiFrameInfo java_frames[JAVA_MAX_FRAMES];

/****
 * Synchronous Java get call stack
 * return the current call strack of the current thread
 */

#if 0
static int
hpcjava_get_call_trace(void **callstack, int count)
{
  jvmtiError err;
  
  err = (*java_jvmti)->GetStackTrace(java_jvmti, NULL, 0, JAVA_MAX_FRAMES, 
					java_frames, &count);
  if (err == JVMTI_ERROR_NONE && count > 0)
  {
    unsigned char *methodName;
    unsigned char *signatureName;
    int i;
    for (i=0; i<count; i++) {
      err = (*java_jvmti)->GetMethodName(java_jvmti, java_frames[i].method, 
                       &methodName, &signatureName, NULL);
      if (err == JVMTI_ERROR_NONE) {
        printf("%d method %p: %s\n", i, java_frames[i].method, methodName);
	(*java_jvmti)->Deallocate(java_jvmti, methodName);
	(*java_jvmti)->Deallocate(java_jvmti, signatureName);
      } 
    }
  }
  return err;
}
#endif


/*
 * look for an interval for a given address
 * 
 * Note: if an interval is not found, it's either:
 * 	- Java's code is not jitted yet. In this case the result will be
 * 	  incorrect and there's no way to get an interval
 *	- it isn't a Java byte code
 * */
splay_interval_t *
hpcjava_get_interval(void *addr)
{
  interval_tree_node *p = interval_tree_lookup(&ui_tree_root, addr);
  if (p != NULL) {
    void *bt;

    hpcjava_get_async_call_trace(&bt, 20);

    TMSG(JAVA, "found in Java unwind tree: addr %p", addr);
  }
  return (splay_interval_t *)p;
}

/*
 * Lookup the instruction pointer 'addr' in the interval tree and
 * return a pointer to the interval containing that address (the new
 * root).  Grow the tree lazily and memo-ize the answers.
 *
 * Extension to support fast normalization of IPs: Given an (dynamic)
 * IP 'ip', return the normalized (static) IP through the argument
 * 'ip_norm'.  Both 'ip' and 'ip_norm' are optional and may be NULL.
 * NOTE: typically 'addr' equals 'ip', but there are subtle use cases
 * where they are different.  (In all cases, 'addr' and 'ip' should be
 * in the same load module.)
 *
 * N.B.: tallent: The above extension is not as clean as I would like.
 * An alternative is to write a separate function, but then the
 * typical usage would require two back-to-back lock-protected
 * lookups.  Thus for the time being, in this limited circumstance, I
 * have chosen performance over a cleaner design.
 *
 * Returns: pointer to unwind_java_interval struct if found, else NULL.
 */
splay_interval_t *
hpcjava_add_address_interval(const void *addr_start, const void *addr_end)
{
  //UI_TREE_JAVA_LOCK;
  splay_interval_t *intvl = hpcjava_addr_to_interval_locked(addr_start, addr_end);
  //UI_TREE_JAVA_UNLOCK;
  
  return intvl;
}


/*
 * The locked version of hpcjava_addr_to_interval().  Lookup the PC
 * address in the interval tree and return a pointer to the interval
 * containing that address.
 *
 * Returns: pointer to unwind_java_interval struct if found, else NULL.
 * The caller must hold the ui-tree lock.
 */
splay_interval_t *
hpcjava_addr_to_interval_locked(const void *addr_start, const void *addr_end)
{
  interval_tree_node *p;

  /*
   * Get list of new intervals to insert into the tree.
   *
   * Note: we can't hold the ui-tree lock while acquiring the fnbounds
   * lock, that could lead to LOR deadlock.  Releasing and reacquiring
   * the lock could cause the insert to fail if another thread does
   * the insert first, but in that case, it's not really a failure.
   */

  TMSG(JAVA, "begin unwind_java insert addr %p to %p",
       addr_start, addr_end);

  /* create an interval address node */
  p = hpcjava_ui_malloc(sizeof(interval_tree_node));
  p->start  = (void*) addr_start;
  p->end    = (void*) addr_end;
  p->lm     = lm_java;

  /* adjust the load module bound */
  dso_info_t *dso = lm_java->dso_info;

  if ( (dso->end_addr == 0) ||dso->end_addr < addr_end)
	 dso->end_addr = p->end;

  if ( (dso->start_addr == 0) || (dso->start_addr > addr_start) )
	 dso->start_addr = p->start;

  if (interval_tree_insert(&ui_tree_root, p) != 0) {
      TMSG(JAVA, "BAD unwind_java interval [%p, %p) insert failed",
           START(p), END(p));
      free_ui_java_node_locked(p);
  }

  TMSG(JAVA, "end unwind_java lm bound [%p, %p]", 
	p->lm->dso_info->start_addr, p->lm->dso_info->end_addr);
  return (p);
}


/*
 * Remove intervals in the range [start, end) from the unwind_java interval
 * tree, and move the deleted nodes to the unwind_java free list.
 */
void
hpcjava_delete_ui()
{
  UI_TREE_JAVA_LOCK;

  TMSG(JAVA, "begin unwind_java delete");

  free_ui_java_tree_locked(ui_tree_root);

  UI_TREE_JAVA_UNLOCK;
}


//---------------------------------------------------------------------
// private operations
//---------------------------------------------------------------------


/*
 * The ui free routines are in the private section in order to enforce
 * locking.  Free is only called from hpcjava_addr_to_interval() or
 * hpcjava_delete_ui_range() where the free list is already locked.
 *
 * Free the nodes in post order, since putting a node on the free list
 * modifies its child pointers.
 */
static void
free_ui_java_tree_locked(interval_tree_node *tree)
{
  if (tree == NULL)
    return;

  free_ui_java_tree_locked(LEFT(tree));
  free_ui_java_tree_locked(RIGHT(tree));
  RIGHT(tree) = ui_free_list;
  ui_free_list = tree;
}


static void
free_ui_java_node_locked(interval_tree_node *node)
{
  RIGHT(node) = ui_free_list;
  ui_free_list = node;
}


//---------------------------------------------------------------------
// debug operations
//---------------------------------------------------------------------


static void
hpcjava_print_interval_tree_r(FILE* fs, interval_tree_node *node);


void
hpcjava_print_interval_tree()
{
  FILE* fs = stdout;

  fprintf(fs, "Interval tree:\n");
  hpcjava_print_interval_tree_r(fs, ui_tree_root);
  fprintf(fs, "\n");
  fflush(fs);
}


static void
hpcjava_print_interval_tree_r(FILE* fs, interval_tree_node *node)
{
  // Base case
  if (node == NULL) {
    return;
  }

  fprintf(fs, "  {%p start=%p end=%p}\n", node, START(node), END(node));

  // Recur
  hpcjava_print_interval_tree_r(fs, RIGHT(node));
  hpcjava_print_interval_tree_r(fs, LEFT(node));
}
