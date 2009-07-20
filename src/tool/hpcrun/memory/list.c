/*
  Copyright ((c)) 2002, Rice University 
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
#include <stdlib.h>

#include "list.h"

#include <messages/messages.h>

// TODO check this ...

#if 0 // temp kill
typedef struct _freelist_s {
  void *first;
  csprof_mem_t memstore;
} freelist;

typedef struct _listnode_s {
  void *next;
  void  *data;
} listnode;

csprof mem_t free_head;
void
csprof_init_freelist(void *head, size_t size)
{
  
  csprof_mem__init(&(((freelist *)head)->memstore),size,0);
  head.first = NULL;
}

void *
csprof_list_alloc(freelist *head, size_t size)
{
  void *node = head.first;
  if (! node){
    return csprof_mem_alloc_main(&(head->memstore),size);
  }
  else {
    head.first = ((listnode *)node)->next;
    return node;
  }
}

void
csprof_list_free(freelist *head, void *node)
{
  ((listnode *)node)->next = head.first;
  head.first = node;
}

#endif // TEMP kill
