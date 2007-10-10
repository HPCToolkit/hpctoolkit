#ifndef PMSG_H
#define PMSG_H

#include "pmask.h"

#ifdef __cplusplus
extern "C" {
#endif

void PMSG(int mask, const char *str, ...);
void set_mask_f_numstr(const char *s);
void EMSG(const char *str, ...);

#ifdef __cplusplus
}
#endif

#endif
