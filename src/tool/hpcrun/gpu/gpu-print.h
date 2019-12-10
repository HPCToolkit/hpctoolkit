//******************************************************************************
// self-contained printing macros
//******************************************************************************

#if DEBUG
#include <stdio.h>
#define PRINT(...) fprintf(stderr, __VA_ARGS__)
#else
#define PRINT(...)
#endif



