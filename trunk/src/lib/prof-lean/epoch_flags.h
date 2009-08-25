#ifndef EPOCH_FLAGS_H
#define EPOCH_FLAGS_H

typedef struct epoch_flags_bitfield {
  bool lush_active : 1;
  unsigned rest1   : 31;
  unsigned rest2   : 32;
} epoch_flags_bitfield;

typedef union epoch_flags_t {
  epoch_flags_bitfield flags;
  uint64_t             all;
} epoch_flags_t;

#endif // EPOCH_FLAGS_H
