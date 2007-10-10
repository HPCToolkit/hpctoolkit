#include <stdio.h>

#if defined(__LIBCATAMOUNT__)
#undef __CRAYXT_CATAMOUNT_TARGET
#define __CRAYXT_CATAMOUNT_TARGET
#endif

#ifdef __CRAYXT_CATAMOUNT_TARGET
char msg[] = "CRAYXT_CAT defined";
#else
char msg[] = "WHOOPS, CRAYXT_CAT NOT defined";
#endif

int main(int argc, char *argv[]){
  printf("%s\n",msg);
  return 0;
}
