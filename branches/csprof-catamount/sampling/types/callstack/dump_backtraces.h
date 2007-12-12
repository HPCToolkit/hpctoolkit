#ifndef DUMP_BACKTRACES_H
#define DUMP_BACKTRACES_H
#include "state.h"
#include "cct.h"
extern void dump_backtraces(csprof_state_t *state, csprof_frame_t *unwind);
#endif
