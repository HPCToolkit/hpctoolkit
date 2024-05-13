// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//*****************************************************************************
// system includes
//*****************************************************************************

#define _GNU_SOURCE

#include <string.h>

//*****************************************************************************
// local includes
//*****************************************************************************

#include "level0-data-node.h"
#include "../../../../memory/hpcrun-malloc.h"

//******************************************************************************
// local data
//******************************************************************************

static __thread level0_data_node_t *free_list = NULL;

//*****************************************************************************
// interface operations
//*****************************************************************************

// Allocate a node for the linked list representing a command list
level0_data_node_t*
level0_data_node_new
(
)
{
  level0_data_node_t *first = free_list;

  if (first) {
    free_list = first->next;
  } else {
    first = (level0_data_node_t *) hpcrun_malloc_safe(sizeof(level0_data_node_t));
  }

  memset(first, 0, sizeof(level0_data_node_t));

  return first;
}

// Return a node for the linked list to the free list
void
level0_data_node_return_free_list
(
  level0_data_node_t* node
)
{
  node->next = free_list;
  free_list = node;
}


void
level0_data_list_free
(
 level0_data_node_t* head
)
{
  level0_data_node_t* next;
  for (; head != NULL; head=next) {
    next = head->next;
    level0_data_node_return_free_list(head);
  }
}
