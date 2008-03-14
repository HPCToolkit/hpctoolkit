#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char *argv[])
{
  char *s1 = getenv("SWITCH_TO_PAPI");
  if (s1){
    printf("switching to papi\n");
    char *s2 = getenv("PAPI_EVLST");
    if (s2) {
      printf("Got papi evlst = '%s'\n",s2);
    }
    else {
      printf("NO papi evlst, use default papi event\n");
    }
  }
  else {
    printf("papi not used\n");
  }
  return 0;
}
