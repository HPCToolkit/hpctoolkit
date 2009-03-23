//
// $Id$
//

#include "csprof_dlfns.h"
#include "atomic-ops.h"
#include "fnbounds_interface.h"
#include "sample_event.h"
#include "spinlock.h"
#include "pmsg.h"


//
// Protect dlopen(), dlclose() and dl_iterate_phdr() with a readers-
// writers lock.  That is, either one writer and no readers, or else
// no writers and any number of readers.
//
// dlopen and dlclose modify the internal dl data structures, so
// they're writers, dl_iterate_phdr (via sampling or pthread_create)
// is a reader.  Note: this lock needs to be process-wide.
//
// And if we could just separate dlopen() into mmap() and its init
// constructor, then we'd only need to block the mmap part. :-(
//
// DLOPEN_RISKY disables the read locks (always succeed), so that
// sampling will never be blocked in this case.  But we keep the write
// locks for the benefit of the fnbounds functions.
// 
//
static spinlock_t dlopen_lock = SPINLOCK_UNLOCKED;
static volatile long dlopen_num_readers = 0;
static volatile char dlopen_num_writers = 0;


// Writers always wait until they aquire the lock.
static void
csprof_dlopen_write_lock(void)
{
  int aquire = 0;

  do {
    spinlock_lock(&dlopen_lock);
    if (dlopen_num_writers == 0) {
      dlopen_num_writers = 1;
      aquire = 1;
    }
    spinlock_unlock(&dlopen_lock);
  } while (! aquire);

  // Wait for any readers to finish.
  if (! ENABLED(DLOPEN_RISKY)) {
    while (dlopen_num_readers > 0) ;
  }
}


static void
csprof_dlopen_write_unlock(void)
{
  dlopen_num_writers = 0;
}


// Downgrade the dlopen lock from a writers lock to a readers lock.
// Must already hold the writers lock.
static void
csprof_dlopen_downgrade_lock(void)
{
  if (! ENABLED(DLOPEN_RISKY)) {
    dlopen_num_readers = 1;
  }
  dlopen_num_writers = 0;
}


// Readers try to aquire a lock, but they don't wait if that fails.
// Returns: 1 if aquired, else 0 if not.
int
csprof_dlopen_read_lock(void)
{
  int aquire = 0;

  if (ENABLED(DLOPEN_RISKY))
    return (1);

  spinlock_lock(&dlopen_lock);
  if (dlopen_num_writers == 0) {
    fetch_and_add(&dlopen_num_readers, 1L);
    aquire = 1;
  }
  spinlock_unlock(&dlopen_lock);

  return (aquire);
}


void
csprof_dlopen_read_unlock(void)
{
  if (ENABLED(DLOPEN_RISKY))
    return;
  fetch_and_add(&dlopen_num_readers, -1L);
}


void 
csprof_pre_dlopen(const char *path, int flags)
{
  csprof_dlopen_write_lock();
}


// FIXME: We would prefer to downgrade the dl lock to a read lock
// before calling fnbounds_map_open_dsos() to allow sampling in other
// threads, but that currently leads to lock-order-reversal (LOR)
// deadlock on the fnbounds lock and the system dl-iterate-phdr()
// lock.
//
void 
csprof_dlopen(const char *module_name, int flags, void *handle)
{
  TMSG(EPOCH, "dlopen: handle = %p, name = %s", handle, module_name);
  // csprof_dlopen_downgrade_lock();
  fnbounds_map_open_dsos();
  csprof_dlopen_write_unlock();
}


void
csprof_dlclose(void *handle)
{
  csprof_dlopen_write_lock();
}


void
csprof_post_dlclose(void *handle, int ret)
{
  TMSG(EPOCH, "dlclose: handle = %p", handle);
  // csprof_dlopen_downgrade_lock();
  fnbounds_unmap_closed_dsos();
  csprof_dlopen_write_unlock();
}
