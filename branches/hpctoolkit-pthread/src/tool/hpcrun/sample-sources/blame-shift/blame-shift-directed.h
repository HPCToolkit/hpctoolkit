#ifndef __hpctoolkit_blame_shift_directed_h__
#define __hpctoolkit_blame_shift_directed_h__

#include <stdint.h>

/******************************************************************************
 * interface operations for clients 
 *****************************************************************************/

void directed_blame_shift_start(uint64_t obj) ;
void directed_blame_shift_end(void);
void directed_blame_accept(uint64_t obj);

#endif // __hpctoolkit_blame_shift_directed_h__
