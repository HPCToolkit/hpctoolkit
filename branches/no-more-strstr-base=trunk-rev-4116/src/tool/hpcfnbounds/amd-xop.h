#ifndef AMD_XOP
#define AMD_XOP

#include <stdbool.h>
#include <stdlib.h>

typedef struct {
  bool success;
  bool weak;
  size_t len;
} amd_decode_t;

extern void adv_amd_decode(amd_decode_t* stat, void* ins);

#endif // AMD_XOP
