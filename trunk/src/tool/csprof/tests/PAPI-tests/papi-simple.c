#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

// ************************* PAPI stuff **********************

typedef char *caddr_t; // add this myself to see if it works

#include "papi.h"

#include "papi-simple.h"

#define EVENT              PAPI_TOT_CYC
#define DEF_THRESHOLD      1000000

#define errx(code,msg,...) do { \
  fprintf(stderr,msg "\n", ##__VA_ARGS__); \
  abort();\
} while(0)

#define PMSG(code,msg,...) fprintf(stderr, msg "\n", ##__VA_ARGS__)

static int count    =  0;
static int EventSet = -1;

int skip_thread_init = 0;

static void
my_handler(int EventSet, void *pc, long long ovec, void *context)
{
  count += 1;
}

int
default_papi_count(void)
{
  return count;
}

void
papi_setup(void (*handler)(int EventSet, void *pc, long long ovec, void *context))
{
  int threshold = DEF_THRESHOLD;

#if 0
  PAPI_set_debug(0x3ff);
#endif

  if (! handler )
    handler = my_handler;
  int ret = PAPI_library_init(PAPI_VER_CURRENT);
  if (ret != PAPI_VER_CURRENT)
    errx(1,"Failed: PAPI_library_init");
  PMSG(DC,"PAPI library init = %d",ret);

  if (! skip_thread_init) {
    if (PAPI_thread_init(pthread_self) != PAPI_OK)
      errx(1,"Failed: PAPI_thread_init");
    PMSG(DC,"PAPI_thread_init OK");
  }

  if (PAPI_create_eventset(&EventSet) != PAPI_OK)
    errx(1, "PAPI_create_eventset failed");
  PMSG(DC,"PAPI create eventset OK, eventset = %d",EventSet);
  
  if (PAPI_add_event(EventSet, EVENT) != PAPI_OK)
    errx(1, "PAPI_add_event failed");

  char nm[256];
  PAPI_event_code_to_name(EVENT,nm);
  PMSG(DC,"PAPI add event %s OK",nm);

  if (PAPI_overflow(EventSet, EVENT, threshold, 0, handler) != PAPI_OK)
    errx(1, "PAPI_overflow failed");
  PMSG(DC,"PAPI_overflow ok");

  if (PAPI_start(EventSet) != PAPI_OK)
    errx(1, "PAPI_start failed");
  PMSG(DC,"PAPI_start ok");
}
  
void
papi_over(void)
{
  long_long values = -1;
  if (PAPI_stop(EventSet, &values) != PAPI_OK)
    errx(1,"PAPI_stop failed");
  PAPI_shutdown();
}

int
papi_count(void)
{
  return count;
}

void
papi_skip_thread_init(int flag)
{
  skip_thread_init = flag;
}
