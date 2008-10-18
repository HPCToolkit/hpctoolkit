// LINUX specific: add a module obtained from dlopen to the
// dl_bound infrastructure

#include <link.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
// #include <stdio.h>

#include "dl_bound.h"
#include "pmsg.h"

static int
callback(struct dl_phdr_info *info, size_t size, void *data)
{
  int j;
  char realp[PATH_MAX];

  //  printf("name=%s (%d segments)\n", info->dlpi_name,
  //	 info->dlpi_phnum);

  
  TMSG(DL_ADD_MODULE,"name in = %s, name considered = %s",(char *)data,info->dlpi_name);
  if (strstr(info->dlpi_name,(const char *)data)){
    char *start = (char *) -1;
    char *end   = NULL;

    TMSG(DL_ADD_MODULE,"   **Name correspondence:# headers = %d",info->dlpi_phnum);
    /* compute first & last executable chunks */
    for (j = 0; j < info->dlpi_phnum; j++) {
      if (info->dlpi_phdr[j].p_flags & PF_X){
	char *addr1 = (char *) info->dlpi_addr + info->dlpi_phdr[j].p_vaddr;
	char *addr2 = addr1 + info->dlpi_phdr[j].p_memsz;
	TMSG(DL_ADD_MODULE,"  header %d is excutable, computed addr = %p,size = %x",j,
	     addr1,info->dlpi_phdr[j].p_memsz);
	if (addr1 < start){
	  TMSG(DL_ADD_MODULE,"  New start %p found. (Old start = %p)",addr1,start);
	  start = addr1;
	}
	if (addr2 >= end) {
	  TMSG(DL_ADD_MODULE,"  New end %p found. (Old end = %p)",addr2,end);
	  end = addr2;
	}
      }
    }
    realpath(info->dlpi_name,realp);
    TMSG(ADD_MODULE_BASE,"name in:%s => %s, start = %p, end = %p",(char *)data,
	 realp,start,end);
    dl_add_module_base(realp, start, end);
    return 1;
  }
  return 0;
}

void 
dl_add_module(const char *module)
{
  dl_iterate_phdr(callback, (void *)module);
}
