// SPDX-FileCopyrightText: 2011-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#ifndef FENCE_ENUM
#define FENCE_ENUM

#define FENCE_ENUMS \
  _MM(NONE) \
  _MM(MAIN) \
  _MM(THREAD) \
  _MM(TRAMP) \
  _MM(BAD)

typedef enum {
#define _MM(a) FENCE_ ## a,
FENCE_ENUMS
#undef _MM
} fence_enum_t;

static const char* fence_enum_names[] = {
#define _MM(a) "FENCE_" #a,
FENCE_ENUMS
#undef _MM
};

static inline const char*
fence_enum_name(fence_enum_t f)
{
  if (f < FENCE_NONE || f > FENCE_BAD)
    return "FENCE_UNINITIALIZED!!";
  return fence_enum_names[f];
}

#endif // FENCE_ENUM
