#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <dlfcn.h>

int main(int argc, char **argv)
{
  void *ptr;

  ptr = dlopen("libm.so", RTLD_LAZY);
  if (ptr == NULL)
    {
      fprintf(stderr,"%s\n",dlerror());
      exit(1);
    }
  exit(0);
}
