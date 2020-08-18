#include <stdio.h>
#include <stdint.h>
#include <byteswap.h>
int main(int argc, char **argv)
{
  uint64_t x = (uint64_t) argc;
  uint64_t y = bswap_64(x);
  return y > 2;
}
