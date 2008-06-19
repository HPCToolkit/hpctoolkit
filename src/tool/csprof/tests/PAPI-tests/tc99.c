#include <stdio.h>
#include <stdlib.h>

#ifdef C99
char *c99_str = "yes";
#else
char *c99_str = "no";
#endif

int main(int a, char *v[])
{
  printf("C99 def = %s\n",c99_str);
  return 0;
}
