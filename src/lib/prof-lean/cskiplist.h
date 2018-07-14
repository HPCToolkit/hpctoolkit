// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2018, Rice University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage.
//
// ******************************************************* EndRiceCopyright *



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
 * Inititalize mcs lock on global free csklnode list.
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
