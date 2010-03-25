// ******************************************************************************
// System Includes
// ******************************************************************************

#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <ucontext.h>

// ******************************************************************************
// Local Includes
// ******************************************************************************

#include <utilities/arch/mcontext.h>
#include <utilities/arch/context-pc.h>

//***************************************************************************
// interface functions
//***************************************************************************


void *
hpcrun_context_pc(void *context)
{
  ucontext_t* ctxt = (ucontext_t*)context;
  return ucontext_pc(ctxt);
}
