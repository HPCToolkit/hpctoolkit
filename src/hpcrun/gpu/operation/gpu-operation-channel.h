// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef gpu_operation_channel_h
#define gpu_operation_channel_h

//******************************************************************************
// local includes
//******************************************************************************




//******************************************************************************
// forward type declarations
//******************************************************************************

typedef struct gpu_operation_item_t gpu_operation_item_t;

typedef struct gpu_operation_channel_t gpu_operation_channel_t;



//******************************************************************************
// interface operations
//******************************************************************************

/**
 * \brief Returns the thread-local channel.
 * If the channel is not initialized, it first initializes thread-local channel.
 */
gpu_operation_channel_t *
gpu_operation_channel_get_local
(
 void
);


/**
 * \brief Sets \p on_send_fn as a on-send callback.
 * The callback  is invoked whenever gpu_operation_channel_send is invoked;
 * \p arg is passed as the argument of \p on_send_fn .
 * If the callback has been set previously, it is overwritten.
*/
void
gpu_operation_channel_init_on_send_callback
(
 gpu_operation_channel_t *channel,
 void (*on_send)(void *),
 void *arg
);


/**
 * \brief Sets \p on_receive_fn as a on-receive callback.
 * The callback  is invoked whenever gpu_operation_channel_receive is invoked;
 * \p arg is passed as the argument of \p on_receive_fn .
 * If the callback has been set previously, it is overwritten.
*/
void
gpu_operation_channel_init_on_receive_callback
(
 gpu_operation_channel_t *channel,
 void (*on_receive_fn)(void *),
 void *arg
);


/**
 * \brief Sends \p item to \p channel
*/
void
gpu_operation_channel_send
(
 gpu_operation_channel_t *channel,
 const gpu_operation_item_t *item
);


/**
 * \brief Calls \p receive_fn for each operation item in the \p channel ;
 * current operation item, thread data associated with the channel, and \p arg
 * are passed as the arguments of \p receive_fn() .
 * All operation items passed to \p receive_fn are removed from the channel.
*/
void
gpu_operation_channel_receive_all
(
 gpu_operation_channel_t *channel,
 void (*receive_fn)(gpu_operation_item_t *, void *),
 void *arg
);

#endif
