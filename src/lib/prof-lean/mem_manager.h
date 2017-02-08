/*
 * mem_manager.h
 *
 *  Abstract memory allocator and de-allocator.
 *
 *  Created on: Dec 9, 2015
 *      Author: dxnguyen
 */

#ifndef __MEM_MANAGER_H__
#define __MEM_MANAGER_H__

//************************* System Include Files ****************************

#include <stddef.h>

typedef void* (*mem_alloc)(size_t size);
typedef void (*mem_free)(void* ptr);

#endif /* __MEM_MANAGER_H__ */
