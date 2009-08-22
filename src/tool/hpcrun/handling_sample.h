#ifndef HANDLING_SAMPLE_H
#define HANDLING_SAMPLE_H

#include "thread_data.h"

extern void csprof_init_handling_sample(thread_data_t *td, int in, int id);
extern void csprof_set_handling_sample(thread_data_t *td);
extern void csprof_clear_handling_sample(thread_data_t *td);
extern int  csprof_is_handling_sample(void);


#endif
