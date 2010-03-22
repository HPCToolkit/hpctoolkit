/*
 * This file is used to overload malloc family functions (malloc, calloc, realloc).
 * We need to cope with direct and indirect calls of malloc in hpcrun and application
 * codes. Lock is used to cope with multi-threaded environment (splay tree insert)
 * Besides LD_PRELOAD, we also use -Wl,--wrap,malloc to define __wrap_malloc()
 *
 * We won't collect malloc data when hpcrun is not initialized and when the async 
 * sampling is blocked
 *
*/
/******************************************************************************
 *  * system includes
 *   *****************************************************************************/

#include <stdio.h> 
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h> 
 
/******************************************************************************
 *  * libmonitor
 *   *****************************************************************************/
 
#include <monitor.h>
 
 
 
/******************************************************************************
 *  * local includes
 *   *****************************************************************************/
 
#include <hpcrun/hpcrun_options.h>
#include <hpcrun/disabled.h>
#include <hpcrun/metrics.h>
#include <sample_event.h>
#include "sample_source_obj.h"
#include "common.h"
#include <hpcrun/sample_sources_registered.h>
#include "simple_oo.h"
#include <hpcrun/thread_data.h>
#include "ibs_init.h" 
#include "splay.h"
#include "splay-interval.h"
#include <unwind/common/unwind.h>

#include <messages/messages.h>

/******************************************************************************
 *  *  * forward functions declaration
 *   *   *****************************************************************************/

extern bool hpcrun_is_initialized();
void* __real_malloc(size_t size);
void* __real_calloc(size_t, size_t);
void* __real_realloc(void* ,size_t);
int insert_splay_tree(interval_tree_node*, void*, size_t, int32_t);
interval_tree_node* splaytree_lookup(void*);

// ****** local variables ******

static int first = 0;//init at the first time

static int malloc_id = -1;
static int calloc_id = -1;
static int realloc_id = -1;

static interval_tree_node *root = NULL;//splay tree's root (originally be NULL)

static int recursive_calloc = 0;//calloc in dlsym or not

pthread_mutex_t mutex_splay = PTHREAD_MUTEX_INITIALIZER;//lock for splay tree

/*wrap versions are retained for the future usage (static link)*/
void* __wrap_malloc(size_t size)
{
  void* ret=__real_malloc(size);

  return ret;
}

void* __wrap_calloc(size_t nmemb, size_t size)
{
  void* ret=__real_calloc(nmemb, size);
 
  return ret;
}

void* __wrap_realloc(void *ptr, size_t size)
{
  void* ret=__real_realloc(ptr, size);
 
  return ret;
}

void
special_metric_init(void)
{
  int n = hpcrun_get_num_metrics();
 
  printf("Num metrics = %d\n", n);
 
  /*n will not be 3 if there are other async sampling*/
  malloc_id = n-3;
  calloc_id = n-2;
  realloc_id = n-1;
 
  hpcrun_set_metric_name(malloc_id, "malloc");
  hpcrun_set_metric_name(calloc_id, "calloc");
  hpcrun_set_metric_name(realloc_id, "realloc");
}

// overwrite the malloc family functions
void* malloc(size_t size)
{
  ucontext_t context1;
  getcontext(&context1);
  void* pc = hpcrun_context_pc(&context1);
  /*check whether we monitor this malloc*/
  if((!hpcrun_is_initialized())||(hpcrun_async_is_blocked(pc)))
  {/*just call real malloc*/
    static void* (*fptr1_malloc)(size_t size);
    if(!fptr1_malloc){
    fptr1_malloc = dlsym(RTLD_NEXT, "malloc");
    }
    if (fptr1_malloc == NULL)
    {
      printf("error in locating\n");
      return NULL;
    }
    void* ret  = fptr1_malloc(size);
    return ret;
  }
  /*block the async sampling*/
  hpcrun_async_block();
  /*initialization*/
  if(first==0)
  {
    first = 1;
    special_metric_init();
  }
  printf("malloc(%d) metric id = %d\n",size,  malloc_id);
  /*unwind the stack*/
  ucontext_t context;
  getcontext(&context);
  cct_node_t* cct_node = hpcrun_sample_callpath(&context, malloc_id, 0, 0, 1);
  /*unblock the async sampling*/
  hpcrun_async_unblock();
  /*call real malloc*/
  static void* (*fptr_malloc)(size_t size);
  if(!fptr_malloc){
  fptr_malloc = dlsym(RTLD_NEXT, "malloc");

  }
  if (fptr_malloc == NULL)
  {
    printf("error in locating\n");
    return NULL;
  }
  void* ret  = fptr_malloc(size);
  printf("malloc ptr %p %d\n", ret, size);
  /*block the async sampling*/
  hpcrun_async_block();
  /*lock splay tree, insert splay tree and unlock it*/
  interval_tree_node* node = hpcrun_malloc(sizeof(interval_tree_node));
  pthread_mutex_lock(&mutex_splay);
  if(insert_splay_tree(node, ret, size, cct_node->persistent_id)<0)
    printf("insert_splay_tree error\n");
  /*unblock async sampling*/
  pthread_mutex_unlock(&mutex_splay);
  hpcrun_async_unblock();
  return ret;
}

void* calloc(size_t nmemb, size_t size)
{
  ucontext_t context1;
  getcontext(&context1);
  void* pc = hpcrun_context_pc(&context1);
  /*check whether we monitor this calloc*/
  if((!hpcrun_is_initialized())||(hpcrun_async_is_blocked(pc)))
  {/*just call real malloc*/
  static void* (*fptr1_calloc)(size_t, size_t);
  if(!fptr1_calloc)
  {
    /*avoid deadlock*/
    if(recursive_calloc==1)//if this is called by dlsym, just return null
    {
      return NULL;
    }
    recursive_calloc=1;
    fptr1_calloc = (void * (*)(size_t, size_t))dlsym(RTLD_NEXT, "calloc");
    if (fptr1_calloc == NULL)
    {
      printf("error in locating\n");
      return NULL;
    }
    recursive_calloc=0;
  }
  void*ret  = (*fptr1_calloc)(nmemb, size);
  printf("calloc ptr %x %d\n", ret, nmemb*size);
  return ret;
  }
  /*block the async sampling*/
  hpcrun_async_block();
  /*initialization*/
  if(first==0)
  {
    first = 1;
    special_metric_init();
  }
  printf("calloc(%d) metric id = %d\n", nmemb*size, calloc_id);
  /*unwind the stack*/
  ucontext_t context;
  getcontext(&context);
  cct_node_t* cct_node = hpcrun_sample_callpath(&context, calloc_id, 0, 0, 1);
  /*unblock the async sampling*/
  hpcrun_async_unblock();
  /*call real malloc*/
  static void* (*fptr_calloc)(size_t, size_t);
  if(!fptr_calloc)
  {
    if(recursive_calloc==1)
      return NULL;
    recursive_calloc=1;
    fptr_calloc = (void * (*)(size_t, size_t))dlsym(RTLD_NEXT, "calloc");
    if (fptr_calloc == NULL)
    {
      printf("error in locating\n");
      return NULL;
    }
    recursive_calloc=0;
  }
  void*ret  = (*fptr_calloc)(nmemb, size);
  printf("calloc ptr %x %d\n", ret, nmemb*size);
  /*block the async sampling*/
  hpcrun_async_block();
  /*lock splay tree, insert splay tree and unlock it*/
  interval_tree_node* node = hpcrun_malloc(sizeof(interval_tree_node));
  pthread_mutex_lock(&mutex_splay);
  if(insert_splay_tree(node, ret, nmemb*size, cct_node->persistent_id)<0)
    printf("insert_splay_tree error\n");
  /*unblock async sampling*/
  pthread_mutex_unlock(&mutex_splay);
  hpcrun_async_unblock();
  return ret;
}

void* realloc(void *ptr, size_t size)
{
  ucontext_t context1;
  getcontext(&context1);
  void* pc = hpcrun_context_pc(&context1);
  if((!hpcrun_is_initialized())||(hpcrun_async_is_blocked(pc)))
  {
  void* (*fptr1_realloc)(void*, size_t);
  void* handle = (void*) -1;
  fptr1_realloc = (void * (*)(void*, size_t))dlsym(handle, "realloc");
  if (fptr1_realloc == NULL)
  {
    printf("error in locating\n");
    return NULL;
  }
  void*ret  = (*fptr1_realloc)(ptr, size);
  printf("realloc ptr %x %d\n", ret, size);
  return ret;
  }
  /*block the async sampling*/
  hpcrun_async_block();
  /*initialization*/
  if(first==0)
  {
    first = 1;
    special_metric_init();
    printf("realloc after init\n");fflush(stdout);
  }
  printf("realloc metric id = %d\n", realloc_id);
  /*unwind the stack*/
  ucontext_t context;
  getcontext(&context);
  cct_node_t* cct_node = hpcrun_sample_callpath(&context, realloc_id, 0, 0, 1);
  /*unblock the async sampling*/
  hpcrun_async_unblock();
  /*call real malloc*/
  void* (*fptr_realloc)(void*, size_t);
  void* handle = (void*) -1;
  fptr_realloc = (void * (*)(void*, size_t))dlsym(handle, "realloc");
  if (fptr_realloc == NULL)
  {
    printf("error in locating\n");
    return NULL;
  }
  void*ret  = (*fptr_realloc)(ptr, size);
  printf("realloc ptr %x %d\n", ret, size);
  /*block the async sampling*/
  hpcrun_async_block();
  /*lock splay tree, insert splay tree and unlock it*/
  interval_tree_node* node = hpcrun_malloc(sizeof(interval_tree_node));
  pthread_mutex_lock(&mutex_splay);
  if(insert_splay_tree(node, ret, size, cct_node->persistent_id)<0)
    printf("insert_splay_tree error\n");
  /*unblock async sampling*/
  pthread_mutex_unlock(&mutex_splay);
  hpcrun_async_unblock();
  return ret;
}

int insert_splay_tree(interval_tree_node* node,  void* start, size_t size, int32_t id)
{
  interval_tree_node* dummy;

  memset(node, 0, sizeof(interval_tree_node));
  node->start = start;
  node->end = (void *)((unsigned long)start+size);
  node->cct_id = id;
  /*first delete and then insert*/
  interval_tree_delete(&root, &dummy, node->start, node->end);
  if(interval_tree_insert(&root, node)!=0)
    return -1;//insert fail
  printf("splay insert: start(%p), end(%p)\n", node->start, node->end);
  return 0;
  
}

interval_tree_node* splaytree_lookup(void* p)
{
  interval_tree_node* result_node = hpcrun_malloc(sizeof(interval_tree_node));
  result_node = interval_tree_lookup(&root, p);
  return result_node;
}
