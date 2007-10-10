#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
int main(int argc, char **argv)
{
  exit(execlp("/bin/bash","-","--noprofile", "--norc", "-c", "date ; date", NULL));
}
