#include <linux/unistd.h>
#include <stdlib.h>
#include <string.h>

#define M1 "hpc_main calling setup"
#define M2 "set up itimer"
#define M3 "write out data"

extern int MAIN_(int arc, char **argv);
extern void csprof_init_internal(void);
extern void csprof_fini_internal(void);

#ifdef NO
void csprof_fini_internal(void){
  write(2,M3 "\n",strlen(M3)+1);
}

void csprof_init_internal(void){
  write(2,M2 "\n",strlen(M2)+1);
}
#endif

extern char *static_epoch_xname;

int HPC_MAIN_(int argc, char **argv){
  int ret;

  write(2,M1 "\n",strlen(M1)+1);
  atexit(csprof_fini_internal);
  static_epoch_xname = strdup(argv[0]);
  csprof_init_internal();
  asm(".globl monitor_unwind_fence1");
  asm("monitor_unwind_fence1:");
  ret = MAIN_(argc,argv);
  asm(".globl monitor_unwind_fence2");
  asm("monitor_unwind_fence2:");
}
