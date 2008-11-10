//
// $Id$
//

#include "csprof_dlfns.h"
#include "atomic-ops.h"
#include "fnbounds_interface.h"
#include "sample_event.h"

static long dlopens_pending = 0;


void 
csprof_pre_dlopen()
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
     csprof_suspend_sampling(1);
     fnbounds_map_open_dsos();
     csprof_suspend_sampling(-1);
   }
}


void 
csprof_dlclose(void *handle)
{
   csprof_suspend_sampling(1);
   fnbounds_unmap_closed_dsos();
   csprof_suspend_sampling(-1);
}
