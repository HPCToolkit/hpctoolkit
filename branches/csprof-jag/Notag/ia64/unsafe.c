#include "interface.h"

/* XXX */
extern void _start();

int
csprof_context_is_unsafe(void *context)
{
  struct ucontext *ctx = (struct ucontext *) context;
  unsigned long ip = (unsigned long) csprof_get_pc(&ctx->uc_mcontext);

  /* not safe to take samples in the dynamic loader */
#define LDSO_START 0x2000000000000000UL
  /* really should make this configurable via a command-line parameter to
     accomodate potentially different machines */
#define LDSO_SIZE 0x27000UL
  return (ip >= LDSO_START && ip < (LDSO_START + LDSO_SIZE));
}
