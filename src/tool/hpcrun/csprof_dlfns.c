//
// $Id$
//

#include "csprof_dlfns.h"
#include "atomic-ops.h"
#include "fnbounds_interface.h"
#include "sample_event.h"
#include "pmsg.h"

static long dlopens_pending = 0;
//static long dlcloses_pending = 0;

// FIXME: look into fetch_and_add optimization for the dlclose side
//        also

// FIXME: Turn sampling off around the real dlopen until we rework how
// we load function addresses into memory not to use dlopen.
//
void 
csprof_pre_dlopen(const char *path, int flags)
{
   fetch_and_add(&dlopens_pending, 1L);
}


int 
csprof_dlopen_pending()
{
  return dlopens_pending;
}


void 
csprof_dlopen(const char *module_name, int flags, void *handle)
{
   long pending = fetch_and_add(&dlopens_pending, -1L);

   if (pending == 1) {
     // there is no other dlopen pending
     fnbounds_map_open_dsos();
   }
   TMSG(EPOCH, "dlopen: handle = %p, name = %s", handle, module_name);
}


void
csprof_dlclose(void *handle)
{
}


void
csprof_post_dlclose(void *handle, int ret)
{
   fnbounds_unmap_closed_dsos();
   TMSG(EPOCH, "dlclose: handle = %p", handle);
}
