// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

//******************************************************************************
//
// File: cskiplist.h
//   $HeadURL$
//
// Purpose:
//   Define an API for a concurrent skip list.
//
//******************************************************************************


#ifndef __CSKIPLIST_H__
#define __CSKIPLIST_H__

//******************************************************************************
// global include files
//******************************************************************************

#include <stdbool.h>

//******************************************************************************
// local include files
//******************************************************************************

#include "generic_val.h"
#include "mem_manager.h"

//******************************************************************************
// macro
//******************************************************************************

#define MAX_CSKIPLIST_STR 65536
#define OPAQUE_TYPE 0
//******************************************************************************
// abstract data type
//******************************************************************************

#if OPAQUE_TYPE

// opaque type not supported by gcc 4.4.*
typedef struct cskiplist_s cskiplist_t;

#else
#include "cskiplist_defs.h"
#endif

/*
 * string representation of a node in the skip list.
 */
typedef void (*cskl_node_tostr)(void* node_val, int node_height, int max_height,
        char str[], int max_cskl_str_len);


//******************************************************************************
// interface operations
//******************************************************************************

/*
 * Initialize mcs lock on global free csklnode list.
 */
void
cskl_init();

cskiplist_t*
cskl_new(
        void *lsentinel,
        void *rsentinel,
        int maxheight,
        val_cmp compare,
        val_cmp inrange,
        mem_alloc m_alloc);

void
cskl_free(void* node);

/*
 * If cskl has a node that matches val then return that node's value,
 * otherwise return NULL.
 */
void*
cskl_cmp_find(cskiplist_t *cskl, void *val);


/*
 * If cskl has a node that contains val then return that node's value,
 * otherwise return NULL.
 */
void*
cskl_inrange_find(cskiplist_t *cskl, void *val);

csklnode_t *
cskl_insert(cskiplist_t *cskl, void *value,
            mem_alloc m_alloc);

bool
cskl_delete(cskiplist_t *cskl, void *value);

bool
cskl_cmp_del_bulk_unsynch(cskiplist_t *cskl, void *low, void *high, mem_free m_free);

bool
cskl_inrange_del_bulk_unsynch(cskiplist_t *cskl, void *low, void *high, mem_free m_free);


//******************************************************************************
// interface operations
//******************************************************************************

/*
 * print the levels for each node
 */
void
cskl_levels_tostr (int height, int max_height, char str[],
        int max_cskl_str_len);

/*
 * append the node string to the current cskiplist string
 */
void
cskl_append_node_str(char nodestr[], char csklstr[], int max_cskl_str_len);

/*
 * compute a string representation of a cskiplist
 * and return the result in the csklstr parameter.
 */
void
cskl_tostr(cskiplist_t *cskl, cskl_node_tostr node_tostr,
        char csklstr[], int max_cskl_str_len);

void
cskl_dump(cskiplist_t *cskl, cskl_node_tostr node_tostr);

void
cskl_print(cskiplist_t *cskl, cskl_node_tostr node_tostr);

void
cskl_check_dump
(
 cskiplist_t *cskl,         // cskiplist instance
 cskl_node_tostr node_tostr // print node value
);

#endif /* __CSKIPLIST_H__ */
