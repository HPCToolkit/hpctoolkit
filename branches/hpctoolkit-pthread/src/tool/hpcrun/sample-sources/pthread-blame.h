#ifndef PTHREAD_BLAME_H
#define PTHREAD_BLAME_H
#include <stdint.h>
#include <stdbool.h>

#define DIRECTED_BLAME_NAME "LOCKWAIT"
extern int hpcrun_get_pthread_directed_blame_metric_id(void);

// blame object manipulation
extern uint64_t pthread_blame_get_blame_target(void);

// pthread blame shifting enabled
extern bool pthread_blame_lockwait_enabled(void);

#endif // PTHREAD_BLAME_H
