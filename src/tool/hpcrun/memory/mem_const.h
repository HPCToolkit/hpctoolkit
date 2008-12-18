#ifndef MEM_CONST_H
#define MEM_CONST_H

#include "mem_error_dbg.h"

#if USE_SMALL_MEM // test w small mem
static const offset_t CSPROF_MEM_INIT_SZ     = 5 * 1024; // test small size
static const offset_t CSPROF_MEM_INIT_SZ_TMP = 128;  // test small size

#else

static const offset_t CSPROF_MEM_INIT_SZ     = 2 * 1024 * 1024; // 2 Mb
static const offset_t CSPROF_MEM_INIT_SZ_TMP = 128 * 1024;  // 128 Kb, (rarely used, so small)

#endif // USE_SMALL_MEM

#endif // MEM_CONST_H

