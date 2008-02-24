#include <stdlib.h>
#include "thread_data.h"

#include "handling_sample.h"
#include "pmsg.h"

void
csprof_init_handling_sample(thread_data_t *td, int in)
{
  EMSG("handling sample called f thread %d",td->id);
  td->handling_sample = in;
}


void
csprof_set_handling_sample(thread_data_t *td)
{
  assert(td->handling_sample == 0);

  td->handling_sample = 0xDEADBEEF;
}


void
csprof_clear_handling_sample(thread_data_t *td)
{
  assert(td->handling_sample == 0xDEADBEEF);

  td->handling_sample = 0;
}


int
csprof_is_handling_sample(void)
{
  return (TD_GET(handling_sample) == 0xDEADBEEF);
}
