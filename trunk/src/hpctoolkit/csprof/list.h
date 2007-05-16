/*
  Copyright ((c)) 2002-2007, Rice University 
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  * Neither the name of Rice University (RICE) nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  This software is provided by RICE and contributors "as is" and any
  express or implied warranties, including, but not limited to, the
  implied warranties of merchantability and fitness for a particular
  purpose are disclaimed. In no event shall RICE or contributors be
  liable for any direct, indirect, incidental, special, exemplary, or
  consequential damages (including, but not limited to, procurement of
  substitute goods or services; loss of use, data, or profits; or
  business interruption) however caused and on any theory of liability,
  whether in contract, strict liability, or tort (including negligence
  or otherwise) arising in any way out of the use of this software, even
  if advised of the possibility of such damage.
*/
#ifndef CSPROF_LIST_H
#define CSPROF_LIST_H

#include "mem.h"

/* doubly linked lists in private memory */
struct csprof_list_node {
    struct csprof_list_node *next;
    struct csprof_list_node *prev;
    void *ip;                   /* the PC of a procedure */
    void *sp;                   /* its stack pointer, to distingush
                                   different recursive calls */
    void *node;                 /* a vdegree tree node */
};

/* list pool management */
struct csprof_list_pool {
    struct csprof_list_node *freelist;
    unsigned int count;
};

typedef struct csprof_list_node csprof_list_t;
typedef struct csprof_list_node csprof_list_node_t;
typedef struct csprof_list_pool csprof_list_pool_t;

/* function prototypes */
csprof_list_pool_t *csprof_list_pool_new(unsigned int);

csprof_list_t *csprof_list_new(csprof_list_pool_t *);
void csprof_list_destroy(csprof_list_pool_t *, csprof_list_t *);

int csprof_list_isempty(csprof_list_t *);

/* places `list2' at the end of `list1' */
void csprof_list_append(csprof_list_t *, csprof_list_t *);

void csprof_list_remove_head(csprof_list_pool_t *, csprof_list_t *);
void csprof_list_remove_tail(csprof_list_pool_t *, csprof_list_t *);

void csprof_list_add_head(csprof_list_pool_t *, csprof_list_t *,
                          void *, void *, void *);
void csprof_list_add_tail(csprof_list_pool_t *, csprof_list_t *,
                          void *, void *, void *);

#endif /* CSPROF_LIST_H */
