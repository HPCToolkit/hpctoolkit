#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

volatile double a = 0.1, b = 1.1, c = 2.1;

int main(int argc, char **argv)
{
  int stat, i;
  printf("Parent %d, sleeping for 1s, then 100000000 iters.\n",getpid());
  sleep(1);
  if (fork() == 0)
    { 
      printf("Child %d, sleeping for 1s, then 2*100000000 iters.\n",getpid());
      sleep(1); 
      for (i=0;i<2*100000000;i++)
	a += b * c;
    }
  else
    { 
      for (i=0;i<2*100000000;i++)
	a += b * c;
      wait(&stat);
    }
  exit(0);
}
