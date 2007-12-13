#ifndef ITIMER_H
#define ITIMER_H

#include "csprof_options.h"

void csprof_set_timer(void);
void csprof_disable_timer(void);
void itimer_event_init(csprof_options_t *opts);
void csprof_init_itimer_signal_handler();

#endif
