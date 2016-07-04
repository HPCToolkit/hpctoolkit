/*
 * cskiplist_ilmstat_btuwi_pair.h
 *
 *      Author: dxnguyen
 */

#ifndef __CSKIPLIST_ILMSTAT_BTUWI_PAIR_H__
#define __CSKIPLIST_ILMSTAT_BTUWI_PAIR_H__

//******************************************************************************
// global include files
//******************************************************************************

#include <stdbool.h>

//******************************************************************************
// local include files
//******************************************************************************

#include "ilmstat_btuwi_pair.h"


//******************************************************************************
// abstract data type
//******************************************************************************

typedef struct cskl_ilmstat_btuwi_s cskl_ilmstat_btuwi_t;

//******************************************************************************
// interface operations
//******************************************************************************

void
cskl_ilmstat_btuwi_init(int maxheight, mem_alloc m_alloc);

cskl_ilmstat_btuwi_t *
cskl_ilmstat_btuwi_new(int maxheight, mem_alloc m_alloc);

ilmstat_btuwi_pair_t*
cskl_ilmstat_btuwi_cmp_find(cskl_ilmstat_btuwi_t *cskl, ilmstat_btuwi_pair_t* value);

ilmstat_btuwi_pair_t*
cskl_ilmstat_btuwi_inrange_find(cskl_ilmstat_btuwi_t *cskl, uintptr_t value);

bool
cskl_ilmstat_btuwi_insert(cskl_ilmstat_btuwi_t *cskl, ilmstat_btuwi_pair_t* value, mem_alloc m_alloc);

bool
cskl_ilmstat_btuwi_cmp_del_bulk_unsynch(
	cskl_ilmstat_btuwi_t *cskl,
	ilmstat_btuwi_pair_t* low,
	ilmstat_btuwi_pair_t* high);

bool
cskl_ilmstat_btuwi_inrange_del_bulk_unsynch(
	cskl_ilmstat_btuwi_t *cskl,
	uintptr_t low,
	uintptr_t high);

/*
 * pre-condition: *nodeval is an ilmstat_btuwi_pair_t.
 */
void
cskl_ilmstat_btuwi_node_tostr(void* nodeval, int node_height, int max_height,
	char str[], int max_cskl_str_len);

void
cskl_ilmstat_btuwi_tostring(cskl_ilmstat_btuwi_t *cskl, char str[]);

void
cskl_ilmstat_btuwi_print(cskl_ilmstat_btuwi_t *cskl);



#endif /* __CSKIPLIST_ILMSTAT_BTUWI_PAIR_H__ */
