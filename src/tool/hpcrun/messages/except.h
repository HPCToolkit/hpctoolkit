#ifndef EXCEPT_H
#define EXCEPT_H

#include <string.h>
#include <unistd.h>

typedef struct Except_T {
  const char* msg;
} Except_T;

#define RAISE(e) do {write(2, e.msg, strlen(e.msg));} while(0)

#endif // EXCEPT_H
