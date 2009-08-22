#include <stdlib.h>
#include <assert.h>
#include "thread_data.h"

#include "handling_sample.h"

#include <messages/messages.h>

//
// id specifically passed in, since id has not been set yet!!
//
void
csprof_init_handling_sample(thread_data_t *td, int in, int id)
{
  NMSG(HANDLING_SAMPLE,"INIT called f thread %d", id);
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
