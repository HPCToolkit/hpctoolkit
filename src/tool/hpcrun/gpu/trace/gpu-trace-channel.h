// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2024, Rice University
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

#ifndef gpu_trace_channel_h
#define gpu_trace_channel_h


//******************************************************************************
// local includes
//******************************************************************************

#include "../../thread_data.h"

#include "gpu-trace-item.h"



//******************************************************************************
// type declarations
//******************************************************************************

typedef struct gpu_trace_channel_t gpu_trace_channel_t;



//******************************************************************************
// interface operations
//******************************************************************************

/**
 * Returns thread data associated with channel
*/
thread_data_t *
gpu_trace_channel_get_thread_data
(
 gpu_trace_channel_t *channel
);


/**
 * Creates a new channel and associates \p thread_data with it
 * @return the newly created channel
*/
gpu_trace_channel_t *
gpu_trace_channel_new
(
 thread_data_t *thread_data
);


/**
 * \brief Sets \p on_send_fn as a on-send callback .
 * The callback  is invoked whenever gpu_trace_channel_send is invoked;
 * \p arg is passed as the argument of \p on_send_fn .
 * If the callback has been set previously, it is overwritten.
*/
void
gpu_trace_channel_init_on_send_callback
(
 gpu_trace_channel_t *channel,
 void (*on_send_fn)(void *),
 void *arg
);


/**
 * \brief Sets \p on_receive_fn as a on-receive callback.
 * The callback  is invoked whenever gpu_trace_channel_receive is invoked;
 * \p arg is passed as the argument of \p on_receive_fn .
 * If the callback has been set previously, it is overwritten.
*/
void
gpu_trace_channel_init_on_receive_callback
(
 gpu_trace_channel_t *channel,
 void (*on_receive_fn)(void *),
 void *arg
);


/**
 * \brief Sends \p trace_item to \p channel
*/
void
gpu_trace_channel_send
(
 gpu_trace_channel_t *channel,
 const gpu_trace_item_t *trace_item
);


/**
 * \brief Calls \p receive_fn for each trace_item in the thread-local channel;
 * current trace_item, thread data associated with the channel, and \p arg
 * are passed as the arguments of \p receive_fn() .
 * All trace items passed to \p receive_fn are removed from the channel.
*/
void
gpu_trace_channel_receive_all
(
 gpu_trace_channel_t *channel,
 void (*receive_fn)(gpu_trace_item_t *, thread_data_t *, void *),
 void *arg
);


#endif
