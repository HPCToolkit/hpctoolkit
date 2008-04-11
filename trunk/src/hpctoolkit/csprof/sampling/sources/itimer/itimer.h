#ifndef ITIMER_H
#define ITIMER_H

#include "csprof_options.h"

int  csprof_itimer_start(void);
int  csprof_itimer_stop(void);
void csprof_itimer_init(csprof_options_t *opts, int lush_metrics);

#endif
