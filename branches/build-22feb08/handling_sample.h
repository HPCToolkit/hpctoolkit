#ifndef HANDLING_SAMPLE_H
#define HANDLING_SAMPLE_H

#include <stdlib.h>
#include "thread_data.h"

void csprof_init_handling_sample(thread_data_t *td, int in);
void csprof_set_handling_sample(thread_data_t *td);
void csprof_clear_handling_sample(thread_data_t *td);
int  csprof_is_handling_sample(void);

#endif // HANDLING_SAMPLE_H
