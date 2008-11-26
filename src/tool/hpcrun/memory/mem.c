#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
// #include <errno.h>
// #include <string.h>

#include <sys/mman.h>           /* for mmap() */
#include <sys/stat.h>
// #include <sys/types.h>          /* for pthreads */

/* user include files */

#include "mem.h"
#include "pmsg.h"
#include "thread_data.h"

static const offset_t CSPROF_MEM_INIT_SZ     = 2 * 1024 * 1024; // 2 Mb
// static const offset_t CSPROF_MEM_INIT_SZ     = 1024; // test small size

static const offset_t CSPROF_MEM_INIT_SZ_TMP = 128 * 1024;  // 128 Kb, (rarely used, so small)

// static const offset_t CSPROF_MEM_INIT_SZ_TMP = 128;  // test small size

/* the system malloc is good about rounding odd amounts to be aligned.
   we need to do the same thing.
   NOTE: might need to be different for 32-bit vs. 64-bit
 */
static size_t csprof_align_malloc_request(size_t size){
  return (size + (8 - 1)) & (~(8 - 1));
}


/* debugging aid */

static const char *
csprof_mem_store__str(csprof_mem_store_t st)
{
  switch (st) {
    case CSPROF_MEM_STORE:    return "general";
    case CSPROF_MEM_STORETMP: return "temp";
    default:
      DIE("programming error", __FILE__, __LINE__);
      return "";
  }
}

// csprof_mem__grow: Creates a new pool of memory that is at least as
// large as 'sz' bytes for the mem store specified by 'st' and returns
// CSPROF_OK; otherwise returns CSPROF_ERR.
static int
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
    abort();
  }

  if(sz > mmsz_base) { mmsz_base = sz; } // max of 'mmsz_base' and 'sz'

  mmsz = mmsz_base + sizeof(csprof_mmap_info_t);

  TMSG(MEM, "creating new %s memory store of %lu bytes", 
       csprof_mem_store__str(st), mmsz_base);

#ifdef NO_MMAP
  PMSG(MEM,"mem grow about to call malloc");
  mmbeg = malloc(mmsz);
  if (!mmbeg){
    EMSG("mem grow malloc failed!");
    return CSPROF_ERR;
  }
#else
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
    return CSPROF_ERR;
  }
#endif  
  // Allocate some space at the beginning of the map to store
  // 'mminfo', add to mmap list and reset allocation information.
  mminfo = (csprof_mmap_info_t*)mmbeg;
  csprof_mmap_info__init(mminfo);
  
  if(allocinf->mmap == NULL) {
    if(*store != NULL) { DIE("programming error", __FILE__, __LINE__); }
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

  return CSPROF_OK;
}

// csprof_mem__init: Initialize and prepare memory stores for use,
// using 'sz' and 'sz_tmp' as the initial sizes (in bytes) of the
// respective stores.  If either size is 0, the respective store is
// disabled; however it is an error for both stores to be
// disabled.  Returns CSPROF_OK upon success; CSPROF_ERR on error.

static int
csprof_mem__init(csprof_mem_t *x, offset_t sz, offset_t sz_tmp){
  if(sz == 0 && sz_tmp == 0) { return CSPROF_ERR; }

  memset(x, 0, sizeof(*x));

  x->sz_next     = sz;
  x->sz_next_tmp = sz_tmp;
  
  // MSG(1,"csprof mem init about to call grow");
  if(sz != 0
     && csprof_mem__grow(x, sz, CSPROF_MEM_STORE) != CSPROF_OK) {
    EMSG("** MEM GROW for main store failed");
    return CSPROF_ERR;
  }
  if(sz_tmp != 0 
     && csprof_mem__grow(x, sz_tmp, CSPROF_MEM_STORETMP) != CSPROF_OK) {
    EMSG("** MEM GROW for tmp store failed");
    return CSPROF_ERR;
  }
  
  // MSG(1,"mem init setting status to INIT");
  x->status = CSPROF_MEM_STATUS_INIT;
  return CSPROF_OK;
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

  // MSG(1,"csprof mem alloc: sz = %ld",sz);

  // Setup pointers
  switch (st) {
    case CSPROF_MEM_STORE:    allocinf = &(x->allocinf); break;
    case CSPROF_MEM_STORETMP: allocinf = &(x->allocinf_tmp); break;
    default: DIE("programming error", __FILE__, __LINE__);
  }

  // Sanity check
  if(sz <= 0) { return NULL; }
  if(!allocinf->mmap) { DIE("programming error", __FILE__, __LINE__); }

  // Check for space
  m = VPTR_ADD(allocinf->next, sz);
  if(m >= allocinf->end) { return NULL; }

  // Allocate the memory
  new_mem = allocinf->next;
  allocinf->next = m;        // bump the 'next' pointer
  
  return new_mem;
}

// csprof_mem__free: Attempts to free 'sz' bytes in the store
// specified by 'st'.  Returns CSPROF_OK upon success; CSPROF_ERR on
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
    default: DIE("programming error", __FILE__, __LINE__);
  }

  // Sanity check
  if(!ptr) { return CSPROF_OK; }
  if(sz <= 0) { return CSPROF_OK; }
  if(!allocinf->mmap) { DIE("programming error", __FILE__, __LINE__); }

  // Check for space
  m = VPTR_ADD(allocinf->next, -sz);
  if(m < allocinf->beg) {
    return CSPROF_ERR;
  }
  
  // Unallocate the memory
  allocinf->next = m; 

  return CSPROF_OK;
}

// csprof_mem__is_enabled: Returns 1 if the memory store 'st' is
// enabled, 0 otherwise.
static int
csprof_mem__is_enabled(csprof_mem_t *x, csprof_mem_store_t st)
{
  switch (st) {
    case CSPROF_MEM_STORE:    return (x->store) ? 1 : 0;
    case CSPROF_MEM_STORETMP: return (x->store_tmp) ? 1 : 0;
    default:
      DIE("programming error", __FILE__, __LINE__);
      return -1;
  }
}
/* csprof_mmap_info_t and csprof_mmap_alloc_info_t */

csprof_mem_t *
csprof_malloc_init(offset_t sz, offset_t sz_tmp)
{
  csprof_mem_t *memstore = &(TD_GET(_mem));

  if(memstore == NULL) { return NULL; }
  if(sz == 1) { sz = CSPROF_MEM_INIT_SZ; }
  if(sz_tmp == 1) { sz_tmp = CSPROF_MEM_INIT_SZ_TMP; }
  
  // MSG(1,"csprof malloc init calls csprof_mem__init");
  TMSG(MEM,"csprof malloc called with sz = %ld, sz_tmp = %ld",sz, sz_tmp);
  int status = csprof_mem__init(memstore, sz, sz_tmp);

  if(status == CSPROF_ERR) {
    return NULL;
  }

  // MSG(1,"malloc_init returns %p",memstore);
  return memstore;
}

// See header file for documentation of public interface.
static void *
csprof_malloc_threaded(csprof_mem_t *memstore, size_t size)
{
  void *mem;

  // Sanity check
  if(csprof_mem__get_status(memstore) != CSPROF_MEM_STATUS_INIT) { EMSG("NO MEM STATUS");return NULL; }
  if(!csprof_mem__is_enabled(memstore, CSPROF_MEM_STORE)) { EMSG("NO MEM ENBL");return NULL; }

  if(size <= 0) { EMSG("CSPROF_MALLOC: size <=0");return NULL; } // check to prevent an infinite loop!

  size = csprof_align_malloc_request(size);

  // Try to allocate the memory (should loop at most once)
  while ((mem = csprof_mem__alloc(memstore, size, CSPROF_MEM_STORE)) == NULL) {

    if((csprof_mem__grow(memstore, size, CSPROF_MEM_STORE) != CSPROF_OK)) {
      EMSG("could not allocate swap space for memory manager");
      abort();
    }
  }

  return mem;
}

void *csprof_malloc(size_t size){
  csprof_mem_t *memstore = TD_GET(memstore);

  // MSG(1,"csprof malloc memstore = %p",memstore);
  return csprof_malloc_threaded(memstore, size);
}

int
csprof_mmap_info__init(csprof_mmap_info_t *x)
{
  memset(x, 0, sizeof(*x));
  return CSPROF_OK;
}

int
csprof_mmap_alloc_info__init(csprof_mmap_alloc_info_t *x)
{
  memset(x, 0, sizeof(*x));
  return CSPROF_OK;
}

