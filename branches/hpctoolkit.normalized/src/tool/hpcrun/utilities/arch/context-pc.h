#ifndef CONTEXT_PC
#define CONTEXT_PC

#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

#include <utilities/arch/mcontext.h>

void* hpcrun_context_pc(void* context);


#endif // CONTEXT_PC
