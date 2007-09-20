#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

int main(int argc, char **argv)
{
  raise(SIGINT);
  exit(0);
}
