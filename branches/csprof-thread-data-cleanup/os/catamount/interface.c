#if 0
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#endif

#include "monitor.h"

#include "general.h"
#include "interface.h"

#include "epoch.h"
#include "mem.h"

extern char *executable_name;

/* these symbols will be defined when the program is fully linked */ 
extern void _start(void);
extern int __stop___libc_freeres_ptrs;

void *static_epoch_offset = (void *)&_start;
void *static_epoch_end    = (void *)&__stop___libc_freeres_ptrs;
char *static_executable_name = NULL; 

void csprof_init_process(struct monitor_start_main_args *m)
{
  static_executable_name =  strdup((m->argv)[0]);
  csprof_init_internal();
}

void csprof_fini_process()
{
  csprof_fini_internal();
}

void csprof_epoch_get_loaded_modules(csprof_epoch_t *epoch,
				     csprof_epoch_t *previous_epoch)
{
  csprof_epoch_module_t *newmod;

  if (previous_epoch == NULL) {
    long static_epoch_size = 
      ((long) static_epoch_end) - ((long)static_epoch_offset);

    MSG(1,"synthesize initial epoch");
    newmod = csprof_malloc(sizeof(csprof_epoch_module_t));
    newmod->module_name = static_executable_name;
    newmod->mapaddr = static_epoch_offset;
    newmod->vaddr = NULL;
    newmod->size = static_epoch_size;

    newmod->next = epoch->loaded_modules;
    epoch->loaded_modules = newmod;
    epoch->num_modules++;
  } else epoch = previous_epoch;


  MSG(1,"epoch information: name = %s\n"
      "         mapaddr = %p\n"
      "         size    = %lx", 
      epoch->loaded_modules->module_name,
      epoch->loaded_modules->mapaddr,
      epoch->loaded_modules->size);

}
