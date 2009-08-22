#ifndef MEM_H
#define MEM_H

static char __buf[1];

#define RESIZE(a,b) a
#define ALLOC(n) &__buf[0]

#endif // MEM_H
