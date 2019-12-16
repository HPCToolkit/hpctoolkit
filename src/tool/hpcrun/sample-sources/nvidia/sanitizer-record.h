#ifndef _HPCTOOLKIT_SANITIZER_RECORD_H_
#define _HPCTOOLKIT_SANITIZER_RECORD_H_

#include <cuda.h>
#include "sanitizer-node.h"

typedef void (*sanitizer_record_fn_t)();


void
sanitizer_record_init();

#endif
