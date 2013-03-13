#ifndef FENCE_ENUM
#define FENCE_ENUM

typedef enum {
  FENCE_NONE,
  FENCE_MAIN,
  FENCE_THREAD,
  FENCE_TRAMP,
  FENCE_BAD,
} fence_enum_t;

static char* fence_enum_names[] = {
  "FENCE_NONE",
  "FENCE_MAIN",
  "FENCE_THREAD",
  "FENCE_TRAMP",
  "FENCE_BAD",
};

static inline char*
fence_enum_name(fence_enum_t f)
{
  if (f < FENCE_NONE || f > FENCE_BAD)
    return "FENCE_UNINITIALIZED!!";
  return fence_enum_names[f];
}

#endif // FENCE_ENUM
