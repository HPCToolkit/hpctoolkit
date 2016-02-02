/*
 * cskiplist_ildmod_btuwi_pair.h
 *
 * A cskiplist whose nodes are ildmod_btuwi_pair_t objects.
 *
 *      Author: dxnguyen
 */

#ifndef __CSKIPLIST_ILDMOD_BTUWI_PAIR_H__
#define __CSKIPLIST_ILDMOD_BTUWI_PAIR_H__

//******************************************************************************
// global include files
//******************************************************************************

#include <stdbool.h>

//******************************************************************************
// local include files
//******************************************************************************

#include "ildmod_btuwi_pair.h"


//******************************************************************************
// abstract data type
//******************************************************************************

typedef struct cskl_ildmod_btuwi_s cskl_ildmod_btuwi_t;

//******************************************************************************
// interface operations
//******************************************************************************

cskl_ildmod_btuwi_t *
cskl_ildmod_btuwi_new(int maxheight, mem_alloc m_alloc);

ildmod_btuwi_pair_t*
cskl_ildmod_btuwi_cmp_find(cskl_ildmod_btuwi_t *cskl, ildmod_btuwi_pair_t* value);

ildmod_btuwi_pair_t*
cskl_ildmod_btuwi_inrange_find(cskl_ildmod_btuwi_t *cskl, uintptr_t value);

bool
cskl_ildmod_btuwi_insert(cskl_ildmod_btuwi_t *cskl, ildmod_btuwi_pair_t* value, mem_alloc m_alloc);

bool
cskl_ildmod_btuwi_delete(cskl_ildmod_btuwi_t *cskl, ildmod_btuwi_pair_t* value);

bool
cskl_ildmod_btuwi_cmp_del_bulk_unsynch(cskl_ildmod_btuwi_t *cskl,
	ildmod_btuwi_pair_t* low, ildmod_btuwi_pair_t* high, mem_free m_free);

bool
cskl_ildmod_btuwi_inrange_del_bulk_unsynch(cskl_ildmod_btuwi_t *cskl,
	uintptr_t low, uintptr_t high, mem_free m_free);

/*
 * pre-condition: *nodeval is an ildmod_btuwi_pair_t.
 */
void
cskl_ildmod_btuwi_node_tostr(void* nodeval, int node_height, int max_height,
	char str[], int max_cskl_str_len);

void
cskl_ildmod_btuwi_tostring(cskl_ildmod_btuwi_t *cskl, char str[]);

void
cskl_ildmod_btuwi_print(cskl_ildmod_btuwi_t *cskl);

#endif /* __CSKIPLIST_ILDMOD_BTUWI_PAIR_H__ */
