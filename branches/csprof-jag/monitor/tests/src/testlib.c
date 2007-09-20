#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

static void ctor() __attribute__((constructor));
static void dtor() __attribute__((destructor));

void ctor()
{
fprintf(stdout,"Library initialized from process %d.\n",getpid());
}

void dtor()
{
fprintf(stdout,"Library shutdown from process %d.\n",getpid());
}

__pid_t fork (void)
{
  __pid_t p = __libc_fork();
  if (p == 0)
    ctor();
  return(p);
}

