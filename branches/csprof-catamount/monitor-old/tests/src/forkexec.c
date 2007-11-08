#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char **argv)
{
  int stat;
  if (fork() == 0)
    { 
      execlp("sleep","sleep", "1", NULL);
    }
  else
    { 
      wait(&stat);
    }
  exit(0);
}
