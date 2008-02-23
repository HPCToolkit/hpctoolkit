#ifndef PMSG_LOOKUP_H
#define PMSG_LOOKUP_H
typedef struct _item_s {
  char *key;
  unsigned int val;
} _item_t;

static _item_t lookup[] = {
#undef E
#define E(s) #s,s
#include "pmsg2.src"
};
#endif

