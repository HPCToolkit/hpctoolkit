#ifndef PMSG_H
#define PMSG_H

#include "pmask.h"

#ifdef __cplusplus
extern "C" {
#endif

void PMSG(int mask, char *str, ...);
void set_mask_f_numstr(char *s);
void EMSG(char *str, ...);

#ifdef __cplusplus
}
#endif

#endif
