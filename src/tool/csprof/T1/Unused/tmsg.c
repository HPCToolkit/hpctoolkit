#include <stdio.h>

#include "tmsg.h"

int main(int argc,char **argv){
  static int nn = 46;

  MSG(1,"This is a msg, int = %d",nn);
  
  return 0;
}
