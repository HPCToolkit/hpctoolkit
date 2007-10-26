#ifndef PMSG_H
#define PMSG_H


#ifdef __cplusplus
extern "C" {
#endif
#include "pmask.h"

void pmsg_init(void);
void PMSG(int mask, const char *str, ...);
void set_mask_f_numstr(const char *s);
void set_pmsg_mask(int m);
void EMSG(const char *str, ...);

#ifdef __cplusplus
}
#endif

#endif
