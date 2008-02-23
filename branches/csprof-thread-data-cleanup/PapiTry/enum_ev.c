#include <stdio.h>
#include <stdlib.h>
#include <papi.h>

static int
papi_setup(void)
{
  int ret = PAPI_library_init(PAPI_VER_CURRENT);

  if (ret != PAPI_VER_CURRENT){
    fprintf(stderr,"Failed: PAPI_library_init: %d\n",ret);
    abort();
  }

#if 0
  ret = PAPI_thread_init(pthread_self);
  if(ret != PAPI_OK){
    fprintf("stderr,Failed: PAPI_thread_init: %d\n",ret);
    abort();
  }
#endif
}

int
main(int argc, char *argv[])
{
  PAPI_event_info_t info;
  int rv;

  papi_setup();

  printf("%-30s %-11s\n","Name","Symbol");
  int i = PAPI_PRESET_MASK;

  do {
    rv = PAPI_get_event_info(i,&info);
    printf("%-30s 0x%-9x\n",info.symbol,info.event_code);
  } while(PAPI_enum_event(&i, PAPI_ENUM_ALL) == PAPI_OK);

  printf("\nNATIVE EVENTS\n");
  i = PAPI_NATIVE_MASK;

  do {
    rv = PAPI_get_event_info(i,&info);
    printf("%-30s 0x%-9x\n",info.symbol,info.event_code);
  } while(PAPI_enum_event(&i, PAPI_ENUM_ALL) == PAPI_OK);

  return 0;
}
