// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_channel_util_h
#define gpu_channel_util_h



//******************************************************************************
// local includes
//******************************************************************************

#include "../../../../common/lean/bichannel.h"
#include "../../../../common/lean/stacks.h"

//*****************************************************************************
// system includes
//*****************************************************************************

#include <stddef.h>


//******************************************************************************
// macros
//******************************************************************************

#define channel_item_alloc(channel, channel_item_type)          \
  (channel_item_type *) channel_item_alloc_helper               \
  ((bichannel_t *) channel, sizeof(channel_item_type))

#define channel_item_free(channel, item)                        \
    channel_item_free_helper                                    \
    ((bichannel_t *) channel,                                   \
     (s_element_t *) item)



//******************************************************************************
// interface functions
//******************************************************************************

s_element_t *
channel_item_alloc_helper
(
 bichannel_t *c,
 size_t size
);


void
channel_item_free_helper
(
 bichannel_t *c,
 s_element_t *se
);



#endif
