#include "ompt/ompt-interface.h"

#define BLAME_NAME OMP_IDLE
#define BLAME_LAYER OpenMP
#define BLAME_REQUEST ompt_idle_blame_shift_request

#include "blame-shift/blame-sample-source.h"
