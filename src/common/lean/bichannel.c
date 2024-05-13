// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

//*****************************************************************************
// local includes
//*****************************************************************************

#include "bichannel.h"
#include "bistack.h"



//*****************************************************************************
// interface operations
//*****************************************************************************

void
bichannel_init
(
 bichannel_t *ch
)
{
  bistack_init(&ch->bistacks[bichannel_direction_forward]);
  bistack_init(&ch->bistacks[bichannel_direction_backward]);
}


void
bichannel_push
(
 bichannel_t *ch,
 bichannel_direction_t dir,
 s_element_t *e
)
{
  bistack_push(&ch->bistacks[dir], e);
}


s_element_t *
bichannel_pop
(
 bichannel_t *ch,
 bichannel_direction_t dir
)
{
  return bistack_pop(&ch->bistacks[dir]);
}


void
bichannel_reverse
(
 bichannel_t *ch,
 bichannel_direction_t dir
)
{
  bistack_reverse(&ch->bistacks[dir]);
}


void
bichannel_steal
(
 bichannel_t *ch,
 bichannel_direction_t dir
)
{
  bistack_steal(&ch->bistacks[dir]);
}
