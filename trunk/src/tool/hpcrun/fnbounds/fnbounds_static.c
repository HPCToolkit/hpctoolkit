#include <assert.h>
#include "fnbounds_interface.h"

//-------------------------------------------------------------------------
// the external variables below will be defined in a 
// machine-generated file
//-------------------------------------------------------------------------
extern void *csprof_nm_addrs[];
extern int      csprof_nm_addrs_len;

int 
fnbounds_init()
{
  return 0;
}


int 
fnbounds_query(void *pc)
{
  assert(0);
  return 0;
}


int 
fnbounds_add(char *module_name, void *start, void *end)
{
  assert(0);
  return 0;
}


int 
fnbounds_enclosing_addr(void *addr, void **start, void **end)
{
    int failure =  fnbounds_table_lookup( csprof_nm_addrs, 
					 csprof_nm_addrs_len,  addr,  start, end);
    return failure;
}


void 
fnbounds_fini()
{
}


void 
fnbounds_epoch_finalize()
{
  csprof_epoch_add_module(files_executable_name(), 0 /* no vaddr */, 
			  csprof_nm_addrs[0], 
			  csprof_nm_addrs[csprof_nm_addrs_len - 1]);
} 

