#include "general.h"
#include "monitor.h"

#include <unistd.h>
#define M(s) write(2,s"\n",strlen(s)+1)

#ifdef NO
void csprof_init_internal(void);
void csprof_fini_internal(void);
#endif

extern void _init_internal(void);
extern void _fini_internal(void);

void monitor_init_process(struct monitor_start_main_args *m){
  M("Monitor calling init");
  _init_internal();
}

void monitor_fini_process(void){
  M("monitor calling _fini");
  _fini_internal();
}

