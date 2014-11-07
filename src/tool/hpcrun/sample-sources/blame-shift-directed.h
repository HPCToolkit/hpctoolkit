#ifndef __hpctoolkit_ompt_blame_shift_directed_h__
#define __hpctoolkit_ompt_blame_shift_directed_h__

#include <stdint.h>

/******************************************************************************
 * interface operations for clients 
 *****************************************************************************/

void ompt_directed_blame_shift_start(uint64_t obj) ;
void ompt_directed_blame_shift_end();
void ompt_directed_blame_accept(uint64_t obj);

#endif // __hpctoolkit_ompt_blame_shift_directed_h__
