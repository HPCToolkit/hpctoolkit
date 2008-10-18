#ifndef _XED_INTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "xed-interface.h"

#ifdef __cplusplus
};
#endif

#define iclass(xptr) xed_decoded_inst_get_iclass(xptr)
#define iclass_eq(xptr, class) (iclass(xptr) == (class))

#endif
