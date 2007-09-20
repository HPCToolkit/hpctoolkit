#include <string.h>
#include <unistd.h>
#include "monitor.h"

#define M(s) write(2,s"\n",strlen(s)+1)

extern char *static_epoch_xname;
extern void csprof_init_internal(void);
extern void csprof_fini_internal(void);

void monitor_init_process(struct monitor_start_main_args *m){
  M("monitor calling csprof_init_internal");
  static_epoch_xname = strdup((m->argv)[0]);
  csprof_init_internal();
}

void monitor_fini_process(void){
  M("monitor calling csprof_fini_internal");
  csprof_fini_internal();
}

