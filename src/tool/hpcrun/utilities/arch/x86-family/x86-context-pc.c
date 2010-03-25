//***************************************************************************
// system include files 
//***************************************************************************

#include <stdio.h>
#include <setjmp.h>
#include <stdbool.h>
#include <assert.h>

#include <sys/types.h>
#include <ucontext.h>

//***************************************************************************
// local include files 
//***************************************************************************

#include <utilities/arch/context-pc.h>
#include <utilities/arch/mcontext.h>

//****************************************************************************
// forward declarations 
//****************************************************************************

//****************************************************************************
// interface functions
//****************************************************************************

void*
hpcrun_context_pc(void* context)
{
  mcontext_t *mc = GET_MCONTEXT(context);
  return MCONTEXT_PC(mc);
}
