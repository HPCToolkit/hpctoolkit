#ifndef X86_INTERVAL_ARG_H
#define X86_INTERVAL_ARG_H

// as extra arguments are added to the process inst routine, better
// to add fields to the structure, rather than keep changing the function signature

typedef struct interval_arg_t {
  void *beg;
  void *end;
  void *return_addr; // A place to store void * return values.
} interval_arg_t;

#endif // X86_INTERVAL_ARG_H
