#ifndef common_method_def_h
#define common_method_def_h

#include "sample_source_obj.h"

// OO macros specifically for common method definitions

#define CMETHOD_DEF(retn,name,...) retn (*hpcrun_ss_ ## name)(sample_source_t* self, ##__VA_ARGS__)
#define CMETHOD_FN(n,...) hpcrun_ss_ ## n(sample_source_t*self, ##__VA_ARGS__)


#endif
