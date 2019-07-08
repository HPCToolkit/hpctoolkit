#include "ompt/ompt-interface.h"

#define BLAME_NAME OMP_MUTEX
#define BLAME_LAYER OpenMP
#define BLAME_REQUEST ompt_mutex_blame_shift_request
#define BLAME_DIRECTED

#include "blame-shift/blame-sample-source.h"
