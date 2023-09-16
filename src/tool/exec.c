#include <unistd.h>

#ifndef CMD_PATH
#error Must define CMD_PATH to give the command to run
#endif

#define STRINGIFY_(X) #X
#define STRINGIFY(X) STRINGIFY_(X)

int main(int argc, char** argv) {
  execv(STRINGIFY(CMD_PATH), argv);
  return 126;
}
