#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
// #include <errno.h>
// #include <string.h>

#include <sys/mman.h>           /* for mmap() */
#include <sys/stat.h>
// #include <sys/types.h>          /* for pthreads */
#include <setjmp.h>

/* user include files */

#include "handling_sample.h"
#include "unwind.h"
#include "mem.h"
#include "pmsg.h"
#include "sample_event.h"
#include "thread_data.h"
#include "hpcrun_return_codes.h"
#include "monitor.h"
#include "vptr_add.h"

#include "mem_const.h"

/* the system malloc is good about rounding odd amounts to be aligned.
   we need to do the same thing.
   NOTE: might need to be different for 32-bit vs. 64-bit
 */
static size_t csprof_align_malloc_request(size_t size){
  return (size + (8 - 1)) & (~(8 - 1));
}

static void
_stop_activity(void)
{
  if (csprof_is_handling_sample()){
    TMSG(MALLOC,"memory allocation failure during sampling. Dropping Sample");
    csprof_drop_sample();
  }
  else {
    TMSG(MALLOC,"memory allocation failure, NOT sampling. Doing whatever cleanup is necessary");
    sigjmp_buf_t *it = &(TD_GET(mem_error));
    siglongjmp(it->jb,9);
  }
}

void
csprof_handle_memory_error(void)
{
  csprof_disable_sampling();
  EEMSG("Sampling disabled due to memory allocation failure");
  _stop_activity();
}

// debugging aid
static const char *
csprof_mem_store__str(csprof_mem_store_t st)
{
  switch (st) {
    case CSPROF_MEM_STORE:    return "general";
    case CSPROF_MEM_STORETMP: return "temp";
    default:
      EMSG("csprof_mem_store__str programming error");
      monitor_real_abort();
      return "";
  }
}

// csprof_mem__grow: Creates a new pool of memory that is at least as
// large as 'sz' bytes for the mem store specified by 'st' and returns
// HPCRUN_OK; otherwise returns HPCRUN_ERR.
int
csprof_mem__grow(csprof_mem_t *x, size_t sz, csprof_mem_store_t st)
{
  // Note: we cannot use our memory stores until we have allocated more mem
  void* mmbeg = NULL;
  int fd, mmflags, mmprot;
  size_t mmsz = 0, mmsz_base = 0;

  csprof_mmap_info_t** store = NULL;
  csprof_mmap_alloc_info_t *allocinf = NULL;
  csprof_mmap_info_t *mminfo = NULL;
  offset_t *sz_next = NULL;

  // Setup pointers
  if(st == CSPROF_MEM_STORE) {
    mmsz_base = x->sz_next;
    store = &(x->store);
    allocinf = &(x->allocinf);
    sz_next = &(x->sz_next);
  } else if(st == CSPROF_MEM_STORETMP) {
    mmsz_base = x->sz_next_tmp;
    store = &(x->store_tmp);
    allocinf = &(x->allocinf_tmp);
    sz_next = &(x->sz_next_tmp);
  } else {
    EMSG("MEM_GROW programming error");
    csprof_handle_memory_error();
  }

  if(sz > mmsz_base) { mmsz_base = sz; } // max of 'mmsz_base' and 'sz'

  mmsz = mmsz_base + sizeof(csprof_mmap_info_t);

  TMSG(MMAP, "creating new %s memory store of %lu bytes", 
       csprof_mem_store__str(st), mmsz_base);

  // Open a new memory map for storage: Create a mmap using swap space
  // as the shared object.
  // FIXME: Linux had MAP_NORESERVE here; no equivalent on Alpha
  mmflags = MAP_PRIVATE; // MAP_VARIABLE
  mmprot = (PROT_READ | PROT_WRITE);
  if((fd = open("/dev/zero", O_RDWR)) == -1) {
    mmflags |= MAP_ANONYMOUS;
  } else {
    mmflags |= MAP_FILE;
  }
  if((mmbeg = mmap(NULL, mmsz, mmprot, mmflags, fd, 0)) == MAP_FAILED) {
    EMSG("mem grow mmap failed");
    return HPCRUN_ERR;
  }

  // Allocate some space at the beginning of the map to store
  // 'mminfo', add to mmap list and reset allocation information.
  mminfo = (csprof_mmap_info_t*)mmbeg;
  csprof_mmap_info__init(mminfo);
  
  if(allocinf->mmap == NULL) {
    if(*store != NULL) {
      EMSG("csprof_mem__grow programming error: allocinf label failure");
      monitor_real_abort();
    }
    allocinf->mmap = mminfo;
    *store = mminfo;
  } else {
    allocinf->mmap->next = mminfo;
    allocinf->mmap = mminfo;
  }

  allocinf->next = VPTR_ADD(mmbeg, sizeof(*mminfo));
  allocinf->beg = mmbeg;
  allocinf->end = VPTR_ADD(mmbeg, mmsz);
  
  // Prepare for (possible) future growth
  *sz_next = (mmsz_base * 2);

  return HPCRUN_OK;
}

// csprof_mem__init: Initialize and prepare memory stores for use,
// using 'sz' and 'sz_tmp' as the initial sizes (in bytes) of the
// respective stores.  If either size is 0, the respective store is
// disabled; however it is an error for both stores to be
// disabled.  Returns HPCRUN_OK upon success; HPCRUN_ERR on error.

int
csprof_mem__init(csprof_mem_t *x, offset_t sz, offset_t sz_tmp)
{
  if(sz == 0 && sz_tmp == 0) { return HPCRUN_ERR; }

  memset(x, 0, sizeof(*x));

  x->sz_next     = sz;
  x->sz_next_tmp = sz_tmp;
  
  TMSG(MEM,"csprof mem init about to call grow");
  if(sz != 0
     && csprof_mem__grow(x, sz, CSPROF_MEM_STORE) != HPCRUN_OK) {
    EMSG("** MEM GROW for main store failed");
    return HPCRUN_ERR;
  }
  if(sz_tmp != 0 
     && csprof_mem__grow(x, sz_tmp, CSPROF_MEM_STORETMP) != HPCRUN_OK) {
    EMSG("** MEM GROW for tmp store failed");
    return HPCRUN_ERR;
  }
  
  x->status = CSPROF_MEM_STATUS_INIT;
  return HPCRUN_OK;
}

// csprof_mem__alloc: Attempts to allocate 'sz' bytes in the store
// specified by 'st'.  If there is sufficient space, returns a
// pointer to the beginning of a block of memory of exactly 'sz'
// bytes; otherwise returns NULL.
static void *
csprof_mem__alloc(csprof_mem_t *x, size_t sz, csprof_mem_store_t st)
{
  void* m, *new_mem;
  csprof_mmap_alloc_info_t *allocinf = NULL;

  // Setup pointers
  switch (st) {
    case CSPROF_MEM_STORE:    allocinf = &(x->allocinf); break;
    case CSPROF_MEM_STORETMP: allocinf = &(x->allocinf_tmp); break;
    default: EMSG("csprof_mem__alloc programming error"); monitor_real_abort();
  }

  TMSG(MEM__ALLOC,"Mem alloc requesting %ld",sz);
  // Sanity check
  if(sz <= 0) { return NULL; }
  if(!allocinf->mmap) { EMSG("csprof_mem__alloc programming error: allocinf label"); monitor_real_abort(); }

  // Check for space
  m = VPTR_ADD(allocinf->next, sz);
  if(m >= allocinf->end) { TMSG(MEM__ALLOC,"request for %ld bytes failed",sz); return NULL; }

  // Allocate the memory
  new_mem = allocinf->next;
  allocinf->next = m;        // bump the 'next' pointer
  
  TMSG(MEM__ALLOC,"request succeeds");
  return new_mem;
}

void *
csprof_mem_alloc_main(csprof_mem_t *x, size_t sz)
{
  return csprof_mem__alloc(x,sz,CSPROF_MEM_STORE);
}

#if 0
// csprof_mem__free: Attempts to free 'sz' bytes in the store
// specified by 'st'.  Returns HPCRUN_OK upon success; HPCRUN_ERR on
// error.
static int
csprof_mem__free(csprof_mem_t *x, void* ptr, size_t sz, csprof_mem_store_t st)
{
  void* m;
  csprof_mmap_alloc_info_t *allocinf = NULL;

  // Setup pointers
  switch (st) {
    case CSPROF_MEM_STORE:    allocinf = &(x->allocinf); break;
    case CSPROF_MEM_STORETMP: allocinf = &(x->allocinf_tmp); break;
    default: EMSG("csprof_mem__free programming error: pointer setup"); monitor_real_abort();
  }

  // Sanity check
  if(!ptr) { return HPCRUN_OK; }
  if(sz <= 0) { return HPCRUN_OK; }
  if(!allocinf->mmap) { EMSG("csprof_mem__free programming error: allocinf"); monitor_real_abort();}

  // Check for space
  m = VPTR_ADD(allocinf->next, -sz);
  if(m < allocinf->beg) {
    return HPCRUN_ERR;
  }
  
  // Unallocate the memory
  allocinf->next = m; 

  return HPCRUN_OK;
}
#endif
// csprof_mem__is_enabled: Returns 1 if the memory store 'st' is
// enabled, 0 otherwise.
static int
csprof_mem__is_enabled(csprof_mem_t *x, csprof_mem_store_t st)
{
  switch (st) {
    case CSPROF_MEM_STORE:    return (x->store) ? 1 : 0;
    case CSPROF_MEM_STORETMP: return (x->store_tmp) ? 1 : 0;
    default:
      EMSG("programming error: csprof_mem__is_enabled");
      return -1;
  }
}
/* csprof_mmap_info_t and csprof_mmap_alloc_info_t */

csprof_mem_t *
csprof_malloc_init(offset_t sz, offset_t sz_tmp)
{
  csprof_mem_t *memstore = &(TD_GET(_mem));

  if(memstore == NULL) { TMSG(MEM,"malloc_init returns NULL"); return NULL; }
  if(sz == 1) { sz = CSPROF_MEM_INIT_SZ; }
  if(sz_tmp == 1) { sz_tmp = CSPROF_MEM_INIT_SZ_TMP; }
  
  TMSG(MEM,"csprof_malloc_init called with sz = %ld, sz_tmp = %ld",sz, sz_tmp);
  int status = csprof_mem__init(memstore, sz, sz_tmp);

  if(status == HPCRUN_ERR) {
    return NULL;
  }

  return memstore;
}

csprof_mem_t *
csprof_malloc2_init(offset_t sz, offset_t sz_tmp)
{
  csprof_mem_t *memstore = &(TD_GET(_mem2));

  if(memstore == NULL) { TMSG(MEM2,"malloc_init returns NULL"); return NULL; }
  if(sz == 1) { sz = CSPROF_MEM_INIT_SZ; }
  if(sz_tmp == 1) { sz_tmp = CSPROF_MEM_INIT_SZ_TMP; }
  
  TMSG(MEM2,"csprof_malloc2_init called with sz = %ld, sz_tmp = %ld",sz, sz_tmp);
  int status = csprof_mem__init(memstore, sz, sz_tmp);

  if(status == HPCRUN_ERR) {
    return NULL;
  }

  return memstore;
}

// See header file for documentation of public interface.
static void *
csprof_malloc_threaded(csprof_mem_t *memstore, size_t size)
{
  void *mem;

  // Sanity check
  if (csprof_mem__get_status(memstore) != CSPROF_MEM_STATUS_INIT) {
    EMSG("NO MEM STATUS");
    return NULL;
  }
  if (!csprof_mem__is_enabled(memstore, CSPROF_MEM_STORE)) {
    EMSG("NO MEM ENBL");
    return NULL;
  }

  if (size <= 0) {
    // check to prevent an infinite loop!
    EMSG("CSPROF_MALLOC: size <=0");
    return NULL;
  } 

  size = csprof_align_malloc_request(size);
  TMSG(CSP_MALLOC,"requested size after alignment = %ld",size);

  // Try to allocate the memory (should loop at most once)
  while ((mem = csprof_mem__alloc(memstore, size, CSPROF_MEM_STORE)) == NULL) {

    TMSG(CSP_MALLOC,"--inside mem__alloc loop");
    if((csprof_mem__grow(memstore, size, CSPROF_MEM_STORE) != HPCRUN_OK)) {
      EMSG("could not allocate swap space for memory manager");
      csprof_handle_memory_error();  // used to abort
    }
  }

  return mem;
}

void *
csprof_malloc(size_t size)
{
  TMSG(CSP_MALLOC,"requesting size %ld",size);
  csprof_mem_t *memstore = TD_GET(memstore);
  return csprof_malloc_threaded(memstore, size);
}

void *
csprof_malloc2(size_t size)
{
  TMSG(CSP_MALLOC,"requesting size %ld",size);
  csprof_mem_t *memstore = TD_GET(memstore2);
  return csprof_malloc_threaded(memstore, size);
}

int
csprof_mmap_info__init(csprof_mmap_info_t *x)
{
  memset(x, 0, sizeof(*x));
  return HPCRUN_OK;
}

int
csprof_mmap_alloc_info__init(csprof_mmap_alloc_info_t *x)
{
  memset(x, 0, sizeof(*x));
  return HPCRUN_OK;
}

