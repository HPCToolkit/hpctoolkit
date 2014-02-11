#ifndef PTHREAD_BLAME_H
#define PTHREAD_BLAME_H
#include <stdint.h>
#include <stdbool.h>

#include <pthread.h>

#define DIRECTED_BLAME_NAME "PTHREAD_BLAME"

// pthread blame shifting enabled
extern bool pthread_blame_lockwait_enabled(void);

//
// handling pthread blame
//
extern void pthread_directed_blame_shift_start(void* obj);
extern void pthread_directed_blame_shift_end(void);
extern void pthread_directed_blame_accept(void* obj);

#endif // PTHREAD_BLAME_H
