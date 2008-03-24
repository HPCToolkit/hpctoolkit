#include <string.h>
#include <unistd.h>
#include "monitor.h"
#include "name.h"
#include "pmsg.h"

// #define M(s) write(2,s"\n",strlen(s)+1)
#define M(s) 

extern void csprof_init_internal(void);
extern void csprof_fini_internal(void);

// always need this variable, but only init thread support will turn it on
int csprof_using_threads = 0;

void *
monitor_init_process(int *argc, char **argv, void *data)
{
  M("monitor calling csprof_init_internal");
  csprof_set_executable_name(argv[0]);
  csprof_init_internal();
  return data;
}

void 
monitor_fini_process(void *data)
{
  M("monitor calling csprof_fini_internal");
  csprof_fini_internal();
}

