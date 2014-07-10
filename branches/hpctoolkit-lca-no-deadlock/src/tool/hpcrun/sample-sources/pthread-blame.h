#ifndef PTHREAD_BLAME_H
#define PTHREAD_BLAME_H
#include <stdint.h>
#include <stdbool.h>

#include <pthread.h>

#define PTHREAD_EVENT_NAME "PTHREAD_WAIT"

#define PTHREAD_BLAME_METRIC "PTHREAD_BLAME"
#define PTHREAD_BLOCKWAIT_METRIC "PTHREAD_BLOCK_WAIT"
#define PTHREAD_SPINWAIT_METRIC "PTHREAD_SPIN_WAIT"

// pthread blame shifting enabled
extern bool pthread_blame_lockwait_enabled(void);

//
// handling pthread blame
//
extern void pthread_directed_blame_shift_blocked_start(void* obj);
extern void pthread_directed_blame_shift_spin_start(void* obj);
extern void pthread_directed_blame_shift_end(void);
extern void pthread_directed_blame_accept(void* obj);

#endif // PTHREAD_BLAME_H
