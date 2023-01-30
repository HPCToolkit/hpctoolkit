#include <ucontext.h>

//***************************************************************************
// local include files
//***************************************************************************

#include <utilities/arch/context-pc.h>
#include <utilities/arch/mcontext.h>

//****************************************************************************
// interface functions
//****************************************************************************

void*
hpcrun_context_pc_async(void* context)
{
  return ucontext_pc(context);
}
