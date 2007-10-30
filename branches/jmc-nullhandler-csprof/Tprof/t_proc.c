#include <stdlib.h>
#include "general.h"

extern void t_driver_init(void);
extern void t_driver_fini(void);

int wait_for_gdb = 1;

void _init_internal(void){
    if (getenv("CSPROF_WAIT")){
      while(wait_for_gdb);
    }
    MSG(CSPROF_MSG_SHUTDOWN, "***> csprof init internal ***");
    t_driver_init();

}

void _fini_internal(void){
  t_driver_fini();

  MSG(1, "would be writing profile data");
}
