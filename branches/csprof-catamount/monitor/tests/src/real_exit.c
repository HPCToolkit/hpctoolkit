#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include "monitor.h"

int main(int argc, char **argv)
{
  monitor_real_exit(1);
}
